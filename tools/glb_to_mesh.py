#!/usr/bin/env python3
"""GLB (binary glTF 2.0) -> Basic Water mesh format.

No external dependencies: a GLB is JSON plus raw buffers, read in pure
Python. The output is a flat binary that the PS3 side can copy straight
into a vertex buffer (big-endian, the same byte order as the PowerPC).

Format:
    magic   "BWM2"                       4 bytes
    part count                           u32
    for each part:
        name                             32 bytes (null padded)
        vertex count                     u32
        index count                      u32
        vertices: x,y,z, nx,ny,nz, r,g,b, metallic  10 x float32
        indices:  u16
        (the end of each part is aligned to 4 bytes)

Axes: glTF's own convention (+Y up, -Z forward) is exactly the game's, so
NO extra axis conversion is done. Blender's Z-up layout is already encoded
in the root node's transform (-90 degrees about X).

The node hierarchy is walked recursively and parent transforms are
accumulated; this model hangs under a single root ("Air Plane"), so a flat
walk gave wrong results. --flip reverses the nose direction.
"""

import json
import struct
import sys

COMP_SIZE = {5120: 1, 5121: 1, 5122: 2, 5123: 2, 5125: 4, 5126: 4}
COMP_FMT = {5120: 'b', 5121: 'B', 5122: 'h', 5123: 'H', 5125: 'I', 5126: 'f'}
NUM_COMP = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4, 'MAT4': 16}


def read_glb(path):
    data = open(path, 'rb').read()
    magic, ver, _ = struct.unpack('<III', data[:12])
    if magic != 0x46546C67:
        raise SystemExit('not a GLB: %s' % path)
    off = 12
    js = None
    bin_ = b''
    while off < len(data):
        clen, ctype = struct.unpack('<II', data[off:off + 8])
        chunk = data[off + 8:off + 8 + clen]
        if ctype == 0x4E4F534A:
            js = json.loads(chunk)
        elif ctype == 0x004E4942:
            bin_ = chunk
        off += 8 + clen + (-clen % 4)
    return js, bin_


def accessor(js, bin_, index):
    a = js['accessors'][index]
    n = NUM_COMP[a['type']]
    ctype = a['componentType']
    fmt = COMP_FMT[ctype]
    size = COMP_SIZE[ctype] * n

    if 'bufferView' not in a:
        return [(0,) * n] * a['count']

    bv = js['bufferViews'][a['bufferView']]
    base = bv.get('byteOffset', 0) + a.get('byteOffset', 0)
    stride = bv.get('byteStride', size)

    out = []
    for i in range(a['count']):
        o = base + i * stride
        out.append(struct.unpack_from('<' + fmt * n, bin_, o))
    return out


def node_matrix(node):
    """Local transform matrix of a node (4x4, row-major)."""
    if 'matrix' in node:
        m = node['matrix']          # glTF is column-major
        return [[m[0], m[4], m[8], m[12]],
                [m[1], m[5], m[9], m[13]],
                [m[2], m[6], m[10], m[14]],
                [m[3], m[7], m[11], m[15]]]

    t = node.get('translation', [0, 0, 0])
    r = node.get('rotation', [0, 0, 0, 1])      # x,y,z,w
    s = node.get('scale', [1, 1, 1])

    x, y, z, w = r
    rot = [
        [1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
        [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
        [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)],
    ]
    m = [[rot[i][j] * s[j] for j in range(3)] + [t[i]] for i in range(3)]
    m.append([0, 0, 0, 1])
    return m


def apply(m, v, is_point=True):
    w = 1.0 if is_point else 0.0
    return tuple(
        m[i][0] * v[0] + m[i][1] * v[1] + m[i][2] * v[2] + m[i][3] * w
        for i in range(3))


def mat_mul(a, b):
    return [[sum(a[i][k] * b[k][j] for k in range(4)) for j in range(4)]
            for i in range(4)]


def to_game_axes(v, flip):
    """glTF and the game use the same convention; --flip reverses the nose."""
    x, y, z = v
    return (-x, y, -z) if flip else (x, y, z)


def material_metallic(js, index):
    """Metallic value of a material; the shader scales the reflection by it.

    The body and engine cowlings are metallic, tyres and rubber are not.
    Without this split the whole aircraft has the same sheen and looks
    like plastic."""
    if index is None:
        return 0.0

    mat = js['materials'][index]
    if mat.get('name') == 'glass':
        return 0.85

    pbr = mat.get('pbrMetallicRoughness', {})
    metal = pbr.get('metallicFactor', 0.0)
    rough = pbr.get('roughnessFactor', 0.5)

    # Roughness reduces the reflection somewhat but does not remove it.
    return max(0.0, min(1.0, metal * (1.0 - rough * 0.35)))


def material_color(js, index):
    """Vertex colour from a glTF material.

    Looking only at baseColorFactor was not enough: in this model the
    emissive parts (windows/lights) have a base colour of [0,0,0] and their
    colour lives only in emissiveFactor - which is why they rendered black.
    The glass material has no value at all and falls back to the default.

    Metallic surfaces are brightened a little: without a real reflection
    they came out completely matte. """
    if index is None:
        return (0.8, 0.8, 0.8)

    mat = js['materials'][index]
    pbr = mat.get('pbrMetallicRoughness', {})
    c = list(pbr.get('baseColorFactor', [1.0, 1.0, 1.0, 1.0])[:3])
    emis = mat.get('emissiveFactor', [0.0, 0.0, 0.0])
    metal = pbr.get('metallicFactor', 0.0)

    if mat.get('name') == 'glass':
        return (0.42, 0.55, 0.68)

    # emissive parts are drawn with the emissive colour instead of the base colour
    for k in range(3):
        if emis[k] > c[k]:
            c[k] = emis[k]

    # a little base brightness for metallic surfaces
    for k in range(3):
        c[k] = c[k] * (1.0 - 0.35 * metal) + 0.35 * metal * 0.72
        # surfaces left pure black cannot be told apart from the body
        if c[k] < 0.05:
            c[k] = 0.05

    return tuple(c)


def walk(js, node_idx, parent, out):
    """Walks the node tree and collects the accumulated transform for each mesh."""
    node = js['nodes'][node_idx]
    m = mat_mul(parent, node_matrix(node))

    if 'mesh' in node:
        out.append((node.get('name', 'part'), node['mesh'], m))
    for c in node.get('children', []):
        walk(js, c, m, out)


def convert(src, dst, flip=False, scale=1.0):
    js, bin_ = read_glb(src)
    parts = []

    ident = [[1 if i == j else 0 for j in range(4)] for i in range(4)]
    found = []
    scene = js['scenes'][js.get('scene', 0)]
    for root in scene['nodes']:
        walk(js, root, ident, found)

    for name, mesh_idx, mat in found:
        mesh = js['meshes'][mesh_idx]
        verts = []
        idx = []

        for prim in mesh['primitives']:
            attrs = prim['attributes']
            pos = accessor(js, bin_, attrs['POSITION'])
            nrm = (accessor(js, bin_, attrs['NORMAL'])
                   if 'NORMAL' in attrs else [(0, 1, 0)] * len(pos))

            color = material_color(js, prim.get('material'))
            metallic = material_metallic(js, prim.get('material'))

            # Vertex colours take priority when present: the texture information
            # is baked into them (see tools/blender/import_737.py). The alpha
            # channel carries the metallic value.
            vcol = None
            if 'COLOR_0' in attrs:
                vcol = accessor(js, bin_, attrs['COLOR_0'])

            base = len(verts)
            for vi, (p, n) in enumerate(zip(pos, nrm)):
                wp = to_game_axes(apply(mat, p), flip)
                wn = to_game_axes(apply(mat, n, False), flip)
                ln = (wn[0] ** 2 + wn[1] ** 2 + wn[2] ** 2) ** 0.5 or 1.0
                cr, cg, cb, cm = color[0], color[1], color[2], metallic
                if vcol is not None and vi < len(vcol):
                    c = vcol[vi]
                    # unsigned short/byte values are scaled to 0..1
                    if isinstance(c[0], int):
                        div = 65535.0 if max(c) > 255 else 255.0
                        c = tuple(x / div for x in c)
                    cr, cg, cb = c[0], c[1], c[2]
                    if len(c) > 3:
                        cm = c[3]

                verts.append((wp[0] * scale, wp[1] * scale, wp[2] * scale,
                              wn[0] / ln, wn[1] / ln, wn[2] / ln,
                              cr, cg, cb, cm))

            if 'indices' in prim:
                for i in accessor(js, bin_, prim['indices']):
                    idx.append(base + i[0])
            else:
                idx.extend(range(base, base + len(pos)))

        if len(verts) == 0 or len(idx) == 0:
            continue        # empty shell left over from a Blender separation

        if len(verts) > 65535:
            print('  WARNING: %s %d vertices - exceeds the u16 index limit, skipped'
                  % (name, len(verts)))
            continue

        parts.append((name[:31], verts, idx))
        print('  %-16s vertices=%6d triangles=%6d'
              % (name, len(verts), len(idx) // 3))

    with open(dst, 'wb') as f:
        f.write(b'BWM2')
        f.write(struct.pack('>I', len(parts)))
        for name, verts, idx in parts:
            f.write(name.encode('ascii', 'replace').ljust(32, b'\0'))
            f.write(struct.pack('>II', len(verts), len(idx)))
            for v in verts:
                f.write(struct.pack('>10f', *v))
            for i in idx:
                f.write(struct.pack('>H', i))
            # The next part's vertex array must start 4-byte aligned:
            # the PPU traps on unaligned float reads.
            pad = (-f.tell()) % 4
            if pad:
                f.write(b'\0' * pad)

    total = sum(len(i) // 3 for _, _, i in parts)
    print('written: %s  (%d parts, %d triangles)' % (dst, len(parts), total))
    return parts


if __name__ == '__main__':
    if len(sys.argv) < 3:
        raise SystemExit(
            'usage: glb_to_mesh.py <input.glb> <output.msh> [--flip] '
            '[--scale N]')
    fl = '--flip' in sys.argv
    sc = 1.0
    if '--scale' in sys.argv:
        sc = float(sys.argv[sys.argv.index('--scale') + 1])
    convert(sys.argv[1], sys.argv[2], fl, sc)
