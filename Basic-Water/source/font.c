#include "font.h"
#include "font_data.h"

/* font_data.h iki tablo saglar:
 *   font8x8_basic[128][8]     U+0000 - U+007F
 *   font8x8_ext_latin[96][8]  U+00A0 - U+00FF  (ç Ç ö Ö ü Ü buradan gelir)
 * Turkcede eksik kalan 6 glif asagida elle cizildi.
 * Her byte bir satir; en dusuk bit (0x01) en soldaki piksel.
 *
 * Cizimde glifler once 16x16'ya buyutulur (scale2x), sonra yarim piksel
 * boyutunda dikdortgenlerle cizilir. Boylece metnin fiziksel boyutu ayni
 * kalirken kose ve capraz hatlar yumusar - 8x8 bitmap'in sert "pixel"
 * gorunumu bu sekilde kirilir. */

/* Turkce glifler.
 *
 * Standart 8x8 tabloda buyuk harfler 0-6. satirlari kaplar. Ustune isaret
 * gelen harflerde (İ Ğ Ö Ü) isaret 0. satira, govde 2-7. satirlara alinir;
 * aksi halde govde kisa kalir ve harf kucuk harf gibi gorunur - ilk yazimda
 * "ALTITUDE" ekranda "ALTITUDE" cikiyordu. */

static const char font_tr_gd[8]  = { 0x3C, 0x00, 0x3C, 0x66, 0x60, 0x6E, 0x66, 0x3C }; /* Ğ */
static const char font_tr_gdl[8] = { 0x3C, 0x00, 0x3E, 0x66, 0x66, 0x3E, 0x06, 0x3C }; /* ğ */
static const char font_tr_id[8]  = { 0x18, 0x00, 0x7E, 0x18, 0x18, 0x18, 0x18, 0x7E }; /* İ */
static const char font_tr_idz[8] = { 0x00, 0x00, 0x1C, 0x18, 0x18, 0x18, 0x3C, 0x00 }; /* ı */
static const char font_tr_sc[8]  = { 0x3C, 0x66, 0x06, 0x3C, 0x60, 0x66, 0x3C, 0x18 }; /* Ş */
static const char font_tr_scl[8] = { 0x00, 0x00, 0x3E, 0x06, 0x3C, 0x60, 0x3E, 0x18 }; /* ş */

/* Ö Ü Ç de elle cizildi: ext_latin tablosundaki bicimlerinde isaret govdeye
 * bitisik geliyor ve 16x16'ya buyutulunce lekeye donusuyor. */
static const char font_tr_od[8]  = { 0x66, 0x00, 0x3C, 0x66, 0x66, 0x66, 0x66, 0x3C }; /* Ö */
static const char font_tr_odl[8] = { 0x66, 0x00, 0x3C, 0x66, 0x66, 0x66, 0x66, 0x3C }; /* ö */
static const char font_tr_ud[8]  = { 0x66, 0x00, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C }; /* Ü */
static const char font_tr_udl[8] = { 0x66, 0x00, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C }; /* ü */
static const char font_tr_cc[8]  = { 0x00, 0x3C, 0x66, 0x06, 0x06, 0x66, 0x3C, 0x18 }; /* Ç */
static const char font_tr_ccl[8] = { 0x00, 0x00, 0x3C, 0x66, 0x06, 0x66, 0x3C, 0x18 }; /* ç */

static const char *glyph_for(unsigned int cp)
{
    switch (cp) {
    case 0x011E: return font_tr_gd;
    case 0x011F: return font_tr_gdl;
    case 0x0130: return font_tr_id;
    case 0x0131: return font_tr_idz;
    case 0x015E: return font_tr_sc;
    case 0x015F: return font_tr_scl;
    case 0x00D6: return font_tr_od;
    case 0x00F6: return font_tr_odl;
    case 0x00DC: return font_tr_ud;
    case 0x00FC: return font_tr_udl;
    case 0x00C7: return font_tr_cc;
    case 0x00E7: return font_tr_ccl;
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

/* 8x8 glifi 16x16'ya buyutur (scale2x / EPX).
 *
 * Her kaynak piksel P dort alt piksele acilir. Komsular esitse kose alt
 * pikseli komsunun degerini alir; boylece merdiven basamaklari kirilir ve
 * capraz hatlar duz gorunur. Bitmap ikili oldugu icin bu kural burada
 * "dis koseleri yuvarla, ic koseleri doldur" anlamina gelir. */
static int px(const char *g, int r, int c)
{
    if (r < 0 || r > 7 || c < 0 || c > 7)
        return 0;
    return (((unsigned char)g[r] >> c) & 1);
}

static void expand2x(const char *g, unsigned short out[16])
{
    int r, c;

    for (r = 0; r < 16; r++)
        out[r] = 0;

    for (r = 0; r < 8; r++) {
        for (c = 0; c < 8; c++) {
            int p = px(g, r, c);
            int a = px(g, r - 1, c);    /* ust */
            int b = px(g, r, c + 1);    /* sag */
            int d = px(g, r, c - 1);    /* sol */
            int e = px(g, r + 1, c);    /* alt */
            int q[4];                   /* sol-ust, sag-ust, sol-alt, sag-alt */
            int i;

            q[0] = q[1] = q[2] = q[3] = p;

            if (d == a && d != e && a != b) q[0] = a;
            if (a == b && a != d && b != e) q[1] = b;
            if (e == d && e != b && d != a) q[2] = d;
            if (b == e && b != a && e != d) q[3] = e;

            for (i = 0; i < 4; i++) {
                if (q[i]) {
                    int rr = r * 2 + (i >> 1);
                    int cc = c * 2 + (i & 1);

                    out[rr] |= (unsigned short)(1u << cc);
                }
            }
        }
    }
}

/* Isaretli Turkce harfler 16x16 uzayda BIRLESTIRILIR.
 *
 * 8x8'lik hucreye hem tam boy govde hem ustteki isaret sigmiyor; govdeyi
 * kisaltinca harf kucuk harf gibi gorunuyordu (Ğ -> ğ). Cizim zaten 16x16'da
 * yapildigi icin taban harf orada asagi kaydirilip isaret ust iki satira
 * yaziliyor. Boylece govde diger buyuk harflerle ayni boyda kaliyor. */

#define MARK_NONE  0
#define MARK_DIA   1        /* iki nokta: Ö Ü ö ü */
#define MARK_BREVE 2        /* yay: Ğ ğ */
#define MARK_DOT   3        /* tek genis nokta: İ */
#define MARK_CED   4        /* alttaki kuyruk: Ş Ç ş ç */

static int tr_base(unsigned int cp, int *mark)
{
    switch (cp) {
    case 0x011E: *mark = MARK_BREVE; return 'G';
    case 0x011F: *mark = MARK_BREVE; return 'g';
    case 0x0130: *mark = MARK_DOT;   return 'I';
    case 0x00D6: *mark = MARK_DIA;   return 'O';
    case 0x00F6: *mark = MARK_DIA;   return 'o';
    case 0x00DC: *mark = MARK_DIA;   return 'U';
    case 0x00FC: *mark = MARK_DIA;   return 'u';
    case 0x015E: *mark = MARK_CED;   return 'S';
    case 0x015F: *mark = MARK_CED;   return 's';
    case 0x00C7: *mark = MARK_CED;   return 'C';
    case 0x00E7: *mark = MARK_CED;   return 'c';
    default:     *mark = MARK_NONE;  return -1;
    }
}

/* Birlestirilmis glifi uretir; cp isaretli bir harf degilse 0 doner. */
static int tr_compose(unsigned int cp, unsigned short out[16])
{
    unsigned short base[16];
    int mark;
    int ch = tr_base(cp, &mark);
    int r;

    if (ch < 0)
        return 0;

    expand2x(font8x8_basic[ch], base);

    for (r = 0; r < 16; r++)
        out[r] = 0;

    if (mark == MARK_CED) {
        /* govde yerinde kalir, kuyruk en alta yazilir */
        for (r = 0; r < 14; r++)
            out[r] = base[r];
        out[14] = 0x01C0;
        out[15] = 0x0180;
        return 1;
    }

    /* govde iki satir asagi kayar, ust iki satir isarete kalir */
    for (r = 0; r < 14; r++)
        out[r + 2] = base[r];

    switch (mark) {
    /* Isaretler govdenin ust kenarindan disa tasar: govde ust satiri
     * 16x16'da 4..11 bitlerini kaplar, bu yuzden noktalar 2-3 ve 12-13
     * bitlerine yaziliyor. Ic tarafa konunca govdeye yapisip okunmuyordu. */
    case MARK_DIA:
        out[0] = 0x300C;
        out[1] = 0x300C;
        break;
    case MARK_BREVE:
        out[0] = 0x300C;
        out[1] = 0x0FF0;
        break;
    case MARK_DOT:
        out[0] = 0x03C0;
        out[1] = 0x03C0;
        break;
    default:
        break;
    }
    return 1;
}

void font_draw_text(int x, int y, int scale, const char *utf8, color_t c)
{
    const char *s = utf8;
    float ps = (float)scale * 0.5f;     /* buyutulmus piksel boyutu */

    /* scale 1'de buyutulmus piksel yarim piksel eder; MSAA kapali oldugu
     * icin bu boyutta dikdortgenler rasterizasyonda kaybolur. Kucuk
     * etiketler bu yuzden klasik 8x8 yolundan cizilir. */
    if (scale < 2) {
        while (*s != '\0') {
            const char *glyph = glyph_for(utf8_next(&s));
            int row;

            for (row = 0; row < 8; row++) {
                unsigned char bits = (unsigned char)glyph[row];
                int col = 0;

                while (col < 8) {
                    int run;

                    if ((bits & (1 << col)) == 0) {
                        col++;
                        continue;
                    }
                    run = 0;
                    while (col + run < 8 && (bits & (1 << (col + run))) != 0)
                        run++;

                    overlay_fill_rect(x + col * scale, y + row * scale,
                                      run * scale, scale, c);
                    col += run;
                }
            }
            x += FONT_CELL * scale;
        }
        return;
    }

    while (*s != '\0') {
        unsigned int cp = utf8_next(&s);
        const char *glyph = glyph_for(cp);
        unsigned short big[16];
        int row;

        if (!tr_compose(cp, big))
            expand2x(glyph, big);

        for (row = 0; row < 16; row++) {
            unsigned int bits = big[row];
            int col = 0;

            /* ayni satirdaki bitisik pikselleri tek dikdortgende ciz */
            while (col < 16) {
                int run;
                float rx, ry;

                if ((bits & (1u << col)) == 0) {
                    col++;
                    continue;
                }
                run = 0;
                while (col + run < 16 && (bits & (1u << (col + run))) != 0)
                    run++;

                /* Yarim piksel cozunurluk gerektigi icin donmemis rot_rect
                 * kullanilir; fill_rect yalnizca tam piksel alir. */
                rx = (float)x + ((float)col + run * 0.5f) * ps;
                ry = (float)y + ((float)row + 0.5f) * ps;
                overlay_rot_rect(rx, ry, run * ps, ps, 0.0f, c, 255);
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
