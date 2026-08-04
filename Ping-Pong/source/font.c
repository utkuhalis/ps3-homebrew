#include "font.h"
#include "font_data.h"

/* font_data.h iki tablo saglar:
 *   font8x8_basic[128][8]     U+0000 - U+007F
 *   font8x8_ext_latin[96][8]  U+00A0 - U+00FF  (ç Ç ö Ö ü Ü buradan gelir)
 * Turkcede eksik kalan 6 glif asagida elle cizildi.
 * Her byte bir satir; en dusuk bit (0x01) en soldaki piksel. */

static const char font_tr_gd[8]  = { 0x3C, 0x00, 0x3C, 0x66, 0x03, 0x73, 0x7C, 0x00 }; /* U+011E G breve */
static const char font_tr_gdl[8] = { 0x00, 0x3C, 0x00, 0x6E, 0x33, 0x3E, 0x30, 0x1F }; /* U+011F g breve */
static const char font_tr_id[8]  = { 0x0C, 0x00, 0x1E, 0x0C, 0x0C, 0x0C, 0x1E, 0x00 }; /* U+0130 I noktali */
static const char font_tr_idz[8] = { 0x00, 0x00, 0x0E, 0x0C, 0x0C, 0x0C, 0x1E, 0x00 }; /* U+0131 i noktasiz */
static const char font_tr_sc[8]  = { 0x1E, 0x07, 0x0E, 0x38, 0x1E, 0x18, 0x30, 0x1E }; /* U+015E S cedilla */
static const char font_tr_scl[8] = { 0x00, 0x00, 0x3E, 0x07, 0x38, 0x1F, 0x30, 0x1C }; /* U+015F s cedilla */

static const char *glyph_for(unsigned int cp)
{
    switch (cp) {
    case 0x011E: return font_tr_gd;
    case 0x011F: return font_tr_gdl;
    case 0x0130: return font_tr_id;
    case 0x0131: return font_tr_idz;
    case 0x015E: return font_tr_sc;
    case 0x015F: return font_tr_scl;
    default: break;
    }

    if (cp < 128)
        return font8x8_basic[cp];
    if (cp >= 0x00A0 && cp <= 0x00FF)
        return font8x8_ext_latin[cp - 0x00A0];

    return font8x8_basic['?'];
}

/* Bir UTF-8 kod noktasi okur, isaretciyi ilerletir. Bozuk bayt icin '?' doner. */
static unsigned int utf8_next(const char **s)
{
    const unsigned char *p = (const unsigned char *)*s;
    unsigned int cp;
    int extra, i;

    if (p[0] < 0x80) {
        cp = p[0];
        extra = 0;
    } else if ((p[0] & 0xE0) == 0xC0) {
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
        if ((p[i] & 0xC0) != 0x80) {    /* eksik/bozuk devam bayti */
            *s += i;
            return '?';
        }
        cp = (cp << 6) | (p[i] & 0x3F);
    }

    *s += extra + 1;
    return cp;
}

static int utf8_count(const char *s)
{
    int n = 0;
    while (*s != '\0') {
        utf8_next(&s);
        n++;
    }
    return n;
}

int font_text_width(const char *utf8, int scale)
{
    return utf8_count(utf8) * FONT_CELL * scale;
}

void font_draw_text(int x, int y, int scale, const char *utf8, color_t c)
{
    const char *s = utf8;

    while (*s != '\0') {
        const char *glyph = glyph_for(utf8_next(&s));
        int row;

        for (row = 0; row < 8; row++) {
            unsigned char bits = (unsigned char)glyph[row];
            int col = 0;

            /* ayni satirdaki bitisik pikselleri tek dikdortgende ciz */
            while (col < 8) {
                int run;

                if ((bits & (1 << col)) == 0) {
                    col++;
                    continue;
                }
                run = 0;
                while (col + run < 8 && (bits & (1 << (col + run))) != 0)
                    run++;

                video_fill_rect(x + col * scale, y + row * scale,
                                run * scale, scale, c);
                col += run;
            }
        }
        x += FONT_CELL * scale;
    }
}

void font_draw_center(int cx, int y, int scale, const char *utf8, color_t c)
{
    font_draw_text(cx - font_text_width(utf8, scale) / 2, y, scale, utf8, c);
}
