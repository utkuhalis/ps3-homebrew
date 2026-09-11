#!/usr/bin/env python3
"""Convert a TrueType font into triangle meshes for the overlay renderer.

The RSX texture path still returns black from tex2D, so a glyph atlas is not
an option yet. Instead every glyph is drawn as geometry: its outline is
flattened into polygons and triangulated here, once, at development time.

For each glyph the output carries:
  * the flattened outline points (em units, y up, baseline at 0),
  * a miter vector per point pointing away from the filled area,
  * fill triangles (indices local to the glyph),
  * the end offset of every contour.

At draw time font.c insets the fill by half a pixel along the miter vectors
and adds a one-pixel ring whose alpha falls from 1 to 0 across the edge. That
ring is the antialiasing: the GPU interpolates the alpha, so edges come out
smooth without MSAA and without any texture.

Usage (needs fontTools, numpy, mapbox_earcut and skia-pathops):
    python3 tools/make_font.py assets/font/Nunito.ttf source/font_vec.h --weight 700

The output is committed; the build does not run this script.
"""

import argparse
import math
import sys

import mapbox_earcut as earcut
import numpy as np
from fontTools.pens.basePen import BasePen
from fontTools.ttLib import TTFont
from fontTools.varLib.instancer import OverlapMode, instantiateVariableFont

# ASCII plus what the UI needs now and Turkish letters for later localisation.
EXTRA = "°·×—ÇçĞğİıÖöŞşÜü"
CODEPOINTS = list(range(0x20, 0x7F)) + [ord(c) for c in EXTRA]

# Maximum distance between a curve and its flattened chord, in em. At a 20 px
# em this is 0.3 px; finer would only add vertices nobody can see.
TOLERANCE_EM = 0.015
# Points closer than this are merged, in em.
MERGE_EM = 0.004
# Miter length is clamped so sharp corners do not throw long spikes.
MITER_LIMIT = 2.5


class FlatPen(BasePen):
    """Collects contours as lists of points, flattening curves on the way."""

    def __init__(self, glyphset, tol):
        super().__init__(glyphset)
        self.tol = tol
        self.contours = []
        self.cur = None

    def _moveTo(self, p):
        self.cur = [p]

    def _lineTo(self, p):
        self.cur.append(p)

    def _qCurveToOne(self, p1, p2):
        p0 = self.cur[-1]
        # chord deviation of a quadratic split into n pieces is |p0-2p1+p2|/(4n^2)
        dx = p0[0] - 2 * p1[0] + p2[0]
        dy = p0[1] - 2 * p1[1] + p2[1]
        n = max(1, math.ceil(math.sqrt(math.hypot(dx, dy) / (4 * self.tol))))
        for i in range(1, n + 1):
            t = i / n
            a, b, c = (1 - t) ** 2, 2 * (1 - t) * t, t * t
            self.cur.append((a * p0[0] + b * p1[0] + c * p2[0],
                             a * p0[1] + b * p1[1] + c * p2[1]))

    def _curveToOne(self, p1, p2, p3):
        p0 = self.cur[-1]
        d = max(math.hypot(p0[0] - 2 * p1[0] + p2[0], p0[1] - 2 * p1[1] + p2[1]),
                math.hypot(p1[0] - 2 * p2[0] + p3[0], p1[1] - 2 * p2[1] + p3[1]))
        n = max(1, math.ceil(math.sqrt(3 * d / (4 * self.tol))))
        for i in range(1, n + 1):
            t = i / n
            a, b, c, e = (1 - t) ** 3, 3 * (1 - t) ** 2 * t, 3 * (1 - t) * t * t, t ** 3
            self.cur.append((a * p0[0] + b * p1[0] + c * p2[0] + e * p3[0],
                             a * p0[1] + b * p1[1] + c * p2[1] + e * p3[1]))

    def _closePath(self):
        if self.cur and len(self.cur) >= 3:
            self.contours.append(self.cur)
        self.cur = None

    _endPath = _closePath


def clean(contour, merge):
    """Drop duplicate points, the closing duplicate and collinear points."""
    out = []
    for p in contour:
        if not out or math.hypot(p[0] - out[-1][0], p[1] - out[-1][1]) > merge:
            out.append(p)
    while len(out) > 2 and math.hypot(out[0][0] - out[-1][0], out[0][1] - out[-1][1]) <= merge:
        out.pop()
    changed = True
    while changed and len(out) > 3:
        changed = False
        for i in range(len(out)):
            a, b, c = out[i - 1], out[i], out[(i + 1) % len(out)]
            cross = (b[0] - a[0]) * (c[1] - b[1]) - (b[1] - a[1]) * (c[0] - b[0])
            if abs(cross) < 1e-9:
                out.pop(i)
                changed = True
                break
    return out


def area(c):
    return 0.5 * sum(c[i - 1][0] * c[i][1] - c[i][0] * c[i - 1][1] for i in range(len(c)))


def inside(pt, c):
    x, y = pt
    hit = False
    for i in range(len(c)):
        (x1, y1), (x2, y2) = c[i - 1], c[i]
        if (y1 > y) != (y2 > y) and x < (x2 - x1) * (y - y1) / (y2 - y1) + x1:
            hit = not hit
    return hit


def miters(c):
    """Per-point vector pointing away from the filled area.

    Contours are normalised before this is called: outer rings run
    counter-clockwise and holes clockwise (y up). With that convention the
    filled area is always on the left of the direction of travel, so the
    right-hand normal (dy, -dx) of every edge points outward."""
    n = len(c)
    edge = []
    for i in range(n):
        (x1, y1), (x2, y2) = c[i], c[(i + 1) % n]
        l = math.hypot(x2 - x1, y2 - y1) or 1.0
        edge.append(((y2 - y1) / l, -(x2 - x1) / l))
    out = []
    for i in range(n):
        a, b = edge[i - 1], edge[i]
        mx, my = a[0] + b[0], a[1] + b[1]
        ml = math.hypot(mx, my)
        if ml < 1e-6:
            mx, my, ml = b[0], b[1], 1.0
        mx, my = mx / ml, my / ml
        cos_half = max(mx * b[0] + my * b[1], 1.0 / MITER_LIMIT)
        out.append((mx / cos_half, my / cos_half))
    return out


def build_glyph(name, glyphset, upm):
    pen = FlatPen(glyphset, TOLERANCE_EM * upm)
    glyphset[name].draw(pen)
    contours = [clean([(x / upm, y / upm) for x, y in c], MERGE_EM) for c in pen.contours]
    contours = [c for c in contours if len(c) >= 3 and abs(area(c)) > 1e-7]

    # nesting depth decides outer/hole; orientation in the file is not trusted
    depth = [sum(1 for j, o in enumerate(contours) if j != i and inside(c[0], o))
             for i, c in enumerate(contours)]
    for i, c in enumerate(contours):
        want_ccw = depth[i] % 2 == 0
        if (area(c) > 0) != want_ccw:
            c.reverse()

    # every hole belongs to the smallest outer ring that contains it
    groups = {i: [] for i in range(len(contours)) if depth[i] % 2 == 0}
    for i, c in enumerate(contours):
        if depth[i] % 2 == 1:
            owners = [o for o in groups if inside(c[0], contours[o])]
            if not owners:
                raise SystemExit("glyph %s: hole %d has no enclosing ring" % (name, i))
            owner = min(owners, key=lambda o: abs(area(contours[o])))
            groups[owner].append(i)

    pts, mit, ends, tris = [], [], [], []
    for outer, holes in groups.items():
        base = len(pts)
        ring_pts, rings = [], []
        for ci in [outer] + holes:
            c = contours[ci]
            pts.extend(c)
            mit.extend(miters(c))
            ends.append(len(pts))
            ring_pts.extend(c)
            rings.append(len(ring_pts))
        idx = earcut.triangulate_float64(np.array(ring_pts, dtype=np.float64),
                                         np.array(rings, dtype=np.uint32))
        tris.extend(int(k) + base for k in idx)
    return pts, mit, ends, tris


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("ttf")
    ap.add_argument("out")
    ap.add_argument("--weight", type=float, default=700)
    args = ap.parse_args()

    font = TTFont(args.ttf)
    if "fvar" in font:
        # Variable glyphs are built from overlapping strokes. Left as they are,
        # an overlap looks like a hole to the nesting test below, so overlaps
        # are merged while instancing (needs skia-pathops).
        font = instantiateVariableFont(font, {"wght": args.weight},
                                       overlap=OverlapMode.REMOVE)
    upm = font["head"].unitsPerEm
    cmap = font.getBestCmap()
    glyphset = font.getGlyphSet()
    hmtx = font["hmtx"]
    os2 = font["OS/2"]

    all_pts, all_mit, all_idx, all_end, rows = [], [], [], [], []
    for cp in CODEPOINTS:
        name = cmap.get(cp)
        if name is None:
            print("missing glyph U+%04X" % cp, file=sys.stderr)
            continue
        pts, mit, ends, tris = build_glyph(name, glyphset, upm)
        rows.append((cp, hmtx[name][0] / upm, len(all_pts), len(pts),
                     len(all_idx), len(tris), len(all_end), len(ends)))
        all_pts += pts
        all_mit += mit
        all_idx += tris
        all_end += ends

    with open(args.out, "w") as f:
        f.write("/* Generated by tools/make_font.py from %s (weight %g).\n"
                " * Font: Nunito, SIL Open Font License 1.1 (assets/font/OFL.txt).\n"
                " * Do not edit by hand. */\n\n" % (args.ttf.split("/")[-1], args.weight))
        f.write("#ifndef FONT_VEC_H\n#define FONT_VEC_H\n\n")
        f.write("#define FONTV_CAP_HEIGHT %.4ff\n" % (os2.sCapHeight / upm))
        f.write("#define FONTV_X_HEIGHT   %.4ff\n" % (os2.sxHeight / upm))
        f.write("#define FONTV_ASCENDER   %.4ff\n" % (os2.sTypoAscender / upm))
        f.write("#define FONTV_DESCENDER  %.4ff\n" % (os2.sTypoDescender / upm))
        f.write("#define FONTV_GLYPHS     %d\n\n" % len(rows))
        f.write("typedef struct {\n    unsigned int cp;\n    float advance;\n"
                "    unsigned int pt0, npt, idx0, nidx, end0, nend;\n} FontVGlyph;\n\n")
        f.write("static const float fontv_pts[%d][2] = {\n" % len(all_pts))
        for i in range(0, len(all_pts), 4):
            f.write("    " + " ".join("{%.4ff,%.4ff}," % p for p in all_pts[i:i + 4]) + "\n")
        f.write("};\n\nstatic const float fontv_miter[%d][2] = {\n" % len(all_mit))
        for i in range(0, len(all_mit), 4):
            f.write("    " + " ".join("{%.3ff,%.3ff}," % p for p in all_mit[i:i + 4]) + "\n")
        f.write("};\n\n/* fill triangles, indices local to the glyph */\n")
        f.write("static const unsigned short fontv_idx[%d] = {\n" % len(all_idx))
        for i in range(0, len(all_idx), 16):
            f.write("    " + ",".join(str(k) for k in all_idx[i:i + 16]) + ",\n")
        f.write("};\n\n/* end offset of each contour, local to the glyph */\n")
        f.write("static const unsigned short fontv_end[%d] = {\n" % len(all_end))
        for i in range(0, len(all_end), 16):
            f.write("    " + ",".join(str(k) for k in all_end[i:i + 16]) + ",\n")
        f.write("};\n\n/* sorted by code point */\n")
        f.write("static const FontVGlyph fontv_glyphs[FONTV_GLYPHS] = {\n")
        for r in sorted(rows):
            f.write("    { 0x%04X, %.4ff, %d, %d, %d, %d, %d, %d },\n" % r)
        f.write("};\n\n#endif\n")

    npt = len(all_pts)
    print("%d glyphs, %d points (avg %.1f), %d fill triangles"
          % (len(rows), npt, npt / max(1, len(rows)), len(all_idx) // 3))


if __name__ == "__main__":
    main()
