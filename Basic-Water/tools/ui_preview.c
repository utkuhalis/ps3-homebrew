/* HUD onizlemesi: PS3 olmadan, host tarafinda.
 *
 * overlay modulunun yerine gecen bir CPU rasterlayici. GPU'nun yaptigi isi
 * taklit eder: MSAA kapali oldugu icin bir piksel yalnizca MERKEZI
 * dikdortgenin icine dusuyorsa boyanir. Boylece yarim piksel boyutundaki
 * dikdortgenlerin gercekten gorunup gorunmedigi burada olculebilir.
 *
 * Cikti: PPM (P6). build.sh preview ile calisir. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../source/overlay.h"
#include "../source/font.h"
#include "../source/gauges.h"

static unsigned char fb[OVL_H][OVL_W][3];

static void put(int x, int y, color_t c, int alpha)
{
    int r = (c >> 16) & 0xFF, g = (c >> 8) & 0xFF, b = c & 0xFF;

    if (x < 0 || y < 0 || x >= OVL_W || y >= OVL_H)
        return;

    fb[y][x][0] = (unsigned char)((r * alpha + fb[y][x][0] * (255 - alpha)) / 255);
    fb[y][x][1] = (unsigned char)((g * alpha + fb[y][x][1] * (255 - alpha)) / 255);
    fb[y][x][2] = (unsigned char)((b * alpha + fb[y][x][2] * (255 - alpha)) / 255);
}

/* Dondurulmus dikdortgen: piksel merkezi testi (GPU ile ayni kural) */
static void raster_rot(float cx, float cy, float w, float h, float ang,
                       color_t c, int alpha)
{
    float ca = cosf(-ang), sa = sinf(-ang);
    float ext = (w > h ? w : h) * 0.75f + 2.0f;
    int y0 = (int)floorf(cy - ext), y1 = (int)ceilf(cy + ext);
    int x0 = (int)floorf(cx - ext), x1 = (int)ceilf(cx + ext);
    int x, y;

    for (y = y0; y <= y1; y++) {
        for (x = x0; x <= x1; x++) {
            float px = (float)x + 0.5f - cx;
            float py = (float)y + 0.5f - cy;
            float lx = px * ca - py * sa;
            float ly = px * sa + py * ca;

            if (lx >= -w * 0.5f && lx <= w * 0.5f &&
                ly >= -h * 0.5f && ly <= h * 0.5f)
                put(x, y, c, alpha);
        }
    }
}

void overlay_fill_rect(int x, int y, int w, int h, color_t c)
{
    int i, j;

    for (j = y; j < y + h; j++)
        for (i = x; i < x + w; i++)
            put(i, j, c, 255);
}

void overlay_blend_rect(int x, int y, int w, int h, color_t c, int alpha)
{
    int i, j;

    for (j = y; j < y + h; j++)
        for (i = x; i < x + w; i++)
            put(i, j, c, alpha);
}

void overlay_soft_blob(int x, int y, int w, int h, color_t c, int alpha)
{
    overlay_blend_rect(x, y, w, h, c, alpha / 2);
}

void overlay_rot_rect(float cx, float cy, float w, float h, float angle,
                      color_t c, int alpha)
{
    raster_rot(cx, cy, w, h, angle, c, alpha);
}

void overlay_ring(float cx, float cy, float radius, float thickness,
                  color_t c, int alpha)
{
    int x, y;
    float ro = radius, ri = radius - thickness;

    for (y = (int)(cy - ro - 1); y <= (int)(cy + ro + 1); y++) {
        for (x = (int)(cx - ro - 1); x <= (int)(cx + ro + 1); x++) {
            float dx = (float)x + 0.5f - cx, dy = (float)y + 0.5f - cy;
            float d = sqrtf(dx * dx + dy * dy);

            if (d <= ro && d >= ri)
                put(x, y, c, alpha);
        }
    }
}

void overlay_disc(float cx, float cy, float radius, color_t c, int alpha)
{
    int x, y;

    for (y = (int)(cy - radius - 1); y <= (int)(cy + radius + 1); y++) {
        for (x = (int)(cx - radius - 1); x <= (int)(cx + radius + 1); x++) {
            float dx = (float)x + 0.5f - cx, dy = (float)y + 0.5f - cy;

            if (sqrtf(dx * dx + dy * dy) <= radius)
                put(x, y, c, alpha);
        }
    }
}

int  overlay_init(void) { return 0; }
void overlay_begin(void) { }
void overlay_flush(void) { }
int  overlay_overflowed(void) { return 0; }

int main(int argc, char **argv)
{
    const char *out = (argc > 1) ? argv[1] : "preview.ppm";
    FILE *fp;
    int y;

    /* gokyuzu benzeri arka plan, HUD kontrasti gercekci gorunsun */
    for (y = 0; y < OVL_H; y++) {
        int x;
        for (x = 0; x < OVL_W; x++) {
            fb[y][x][0] = (unsigned char)(120 + y / 12);
            fb[y][x][1] = (unsigned char)(160 + y / 14);
            fb[y][x][2] = (unsigned char)(205 + y / 40);
        }
    }

    /* --- font orneklemesi --- */
    font_draw_text(40, 30, 3, "BASIC WATER", RGB(255, 255, 255));
    font_draw_text(40, 80, 2, "AĞIRLIK  4687 kg", RGB(240, 245, 255));
    font_draw_text(40, 110, 2, "YÖN  6 derece   YÜKSEKLİK", RGB(240, 245, 255));
    font_draw_text(40, 140, 2, "GÖREVLER: Kalkış pistini bul", RGB(240, 245, 255));
    font_draw_text(40, 170, 2, "ŞİMDİ ÇOK GÜZEL ĞÜŞİÖÇ ığşiöç", RGB(240, 245, 255));
    font_draw_text(40, 200, 1, "kucuk etiket: HIZ km/s  EĞİM", RGB(240, 245, 255));
    font_draw_text(40, 225, 2, "abcdefghijklmnopqrstuvwxyz", RGB(240, 245, 255));
    font_draw_text(40, 255, 2, "0123456789 %.,:-/()", RGB(240, 245, 255));

    /* --- gostergeler, cesitli yatis ve burun acilariyla --- */
    gauges_draw(253.0f, 423.0f, 0.0f, 0.0f);
    /* farkli yatis ve burun acilarinda suni ufuk: tasma kontrolu */
    {
        float rolls[5]   = { 0.0f, 0.5f, 1.2f, -0.9f, 2.6f };
        float pitches[5] = { 0.0f, 0.3f, -0.4f, 0.15f, 0.0f };
        int i;

        for (i = 0; i < 5; i++) {
            float cx = 150.0f + i * 130.0f;
            float cy = 400.0f;

            /* gostergenin sinirini gorebilmek icin arka plani isaretle */
            overlay_blend_rect((int)(cx - 60), (int)(cy - 60), 120, 120,
                               RGB(255, 0, 0), 60);
            gauges_draw_attitude(cx, cy, 52.0f, pitches[i], rolls[i]);
        }
    }

    fp = fopen(out, "wb");
    if (fp == NULL) {
        fprintf(stderr, "cikti acilamadi: %s\n", out);
        return 1;
    }
    fprintf(fp, "P6\n%d %d\n255\n", OVL_W, OVL_H);
    fwrite(fb, 1, sizeof(fb), fp);
    fclose(fp);
    printf("onizleme yazildi: %s\n", out);
    return 0;
}
