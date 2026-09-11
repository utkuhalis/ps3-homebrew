#include "font.h"
#include "font_vec.h"

/* Text is drawn as triangle meshes, not from a texture: tex2D still returns
 * black on RSX, and the previous 8x8 bitmap looked like what it was.
 *
 * tools/make_font.py flattens and triangulates every glyph of Nunito ahead
 * of time (source/font_vec.h). Here each glyph becomes two rings of points:
 * the outline pushed half a pixel inward (opacity 1) and half a pixel
 * outward (opacity 0). The fill uses the inner ring; the strip between the
 * rings fades out across one pixel. The GPU interpolates that opacity, which
 * gives antialiased edges without MSAA and without a texture. */

/* Top of the capitals sits at y; the baseline is one cap height below. */
#define BASELINE_PER_SCALE (FONTV_CAP_HEIGHT * FONT_EM_PER_SCALE)

/* Width of the antialiasing ramp in pixels. One pixel is the right amount:
 * wider looks soft, narrower brings the stair steps back. */
#define EDGE_PX 1.0f

/* Largest glyph in font_vec.h has 67 points and 195 fill indices. */
#define MAX_GLYPH_PTS 128
#define MAX_GLYPH_IDX (MAX_GLYPH_PTS * 3 + MAX_GLYPH_PTS * 6)

static const FontVGlyph *glyph_for(unsigned int cp)
{
    int lo = 0, hi = FONTV_GLYPHS - 1;

    /* ASCII is stored first and in order, so it needs no search */
    if (cp >= 0x20 && cp < 0x7F)
        return &fontv_glyphs[cp - 0x20];

    while (lo <= hi) {
        int mid = (lo + hi) / 2;

        if (fontv_glyphs[mid].cp == cp)
            return &fontv_glyphs[mid];
        if (fontv_glyphs[mid].cp < cp)
            lo = mid + 1;
        else
            hi = mid - 1;
    }
    return &fontv_glyphs['?' - 0x20];
}

/* Reads one UTF-8 code point and advances the pointer; '?' for bad bytes. */
static unsigned int utf8_next(const char **s)
{
    const unsigned char *p = (const unsigned char *)*s;
    unsigned int cp;
    int extra, i;

    if (p[0] < 0x80) {
        *s += 1;
        return p[0];
    }

    if ((p[0] & 0xE0) == 0xC0) {
        cp = p[0] & 0x1F;
        extra = 1;
    } else if ((p[0] & 0xF0) == 0xE0) {
        cp = p[0] & 0x0F;
        extra = 2;
    } else if ((p[0] & 0xF8) == 0xF0) {
        cp = p[0] & 0x07;
        extra = 3;
    } else {
        *s += 1;
        return '?';
    }

    for (i = 1; i <= extra; i++) {
        if ((p[i] & 0xC0) != 0x80) {
            *s += 1;
            return '?';
        }
        cp = (cp << 6) | (p[i] & 0x3F);
    }

    *s += extra + 1;
    return cp;
}

int font_text_width(const char *utf8, int scale)
{
    const char *s = utf8;
    float em = FONT_EM_PER_SCALE * (float)scale;
    float w = 0.0f;

    while (*s != '\0')
        w += glyph_for(utf8_next(&s))->advance * em;
    return (int)(w + 0.5f);
}

static void draw_glyph(const FontVGlyph *g, float x, float base, float em,
                       color_t c)
{
    static OvlPoint pts[MAX_GLYPH_PTS * 2];
    static unsigned short tri[MAX_GLYPH_IDX];
    const float h = EDGE_PX * 0.5f;
    unsigned int n = g->npt, k, e, start;
    int ni = 0;

    if (n == 0 || n > MAX_GLYPH_PTS)
        return;

    for (k = 0; k < n; k++) {
        const float *p = fontv_pts[g->pt0 + k];
        const float *m = fontv_miter[g->pt0 + k];
        float px = x + p[0] * em, py = base - p[1] * em;
        float mx = m[0] * h, my = -m[1] * h;       /* font y is up, screen y down */

        pts[k].x = px - mx;     pts[k].y = py - my;     pts[k].a = 1.0f;
        pts[n + k].x = px + mx; pts[n + k].y = py + my; pts[n + k].a = 0.0f;
    }

    for (k = 0; k < g->nidx; k++)
        tri[ni++] = fontv_idx[g->idx0 + k];

    /* the fading strip, one quad per outline edge */
    start = 0;
    for (e = 0; e < g->nend; e++) {
        unsigned int end = fontv_end[g->end0 + e];

        for (k = start; k < end; k++) {
            unsigned int j = (k + 1 < end) ? k + 1 : start;

            tri[ni++] = (unsigned short)k;
            tri[ni++] = (unsigned short)j;
            tri[ni++] = (unsigned short)(n + j);
            tri[ni++] = (unsigned short)k;
            tri[ni++] = (unsigned short)(n + j);
            tri[ni++] = (unsigned short)(n + k);
        }
        start = end;
    }

    overlay_tris(pts, (int)(n * 2), tri, ni, c, 255);
}

void font_draw_text(int x, int y, int scale, const char *utf8, color_t c)
{
    const char *s = utf8;
    float em = FONT_EM_PER_SCALE * (float)scale;
    float base = (float)y + BASELINE_PER_SCALE * (float)scale;
    float pen = (float)x;

    while (*s != '\0') {
        const FontVGlyph *g = glyph_for(utf8_next(&s));

        draw_glyph(g, pen, base, em, c);
        pen += g->advance * em;
    }
}

void font_draw_center(int cx, int y, int scale, const char *utf8, color_t c)
{
    font_draw_text(cx - font_text_width(utf8, scale) / 2, y, scale, utf8, c);
}
