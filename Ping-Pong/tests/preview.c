/* Cizim katmanini host'ta calistirip PPM goruntusu uretir.
 * Amac: Turkce glifleri ve ekran yerlesimini PS3/emulator olmadan dogrulamak.
 * video.c ve input.c yerine bu dosyadaki stub'lar kullanilir.
 * Calistirma: tests/preview.sh */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../source/video.h"
#include "../source/input.h"
#include "../source/menu.h"
#include "../source/game.h"
#include "../source/draw.h"

/* ---- video stub: bellekte 1280x720 tampon ---- */
static color_t fb[FIELD_H][FIELD_W];

int  video_init(void) { return 0; }
void video_exit(void) { }
void video_flip(void) { }

void video_clear(color_t c)
{
    int x, y;
    for (y = 0; y < FIELD_H; y++)
        for (x = 0; x < FIELD_W; x++)
            fb[y][x] = c;
}

void video_draw_image(const color_t *src, int sw, int sh,
                      int x, int y, int w, int h, int use_alpha)
{
    int px, py;

    if (src == NULL || sw <= 0 || sh <= 0 || w <= 0 || h <= 0)
        return;

    for (py = y; py < y + h; py++) {
        int sy;
        const color_t *srow;

        if (py < 0 || py >= FIELD_H) continue;
        sy = (py - y) * sh / h;
        srow = src + sy * sw;

        for (px = x; px < x + w; px++) {
            int sx;
            color_t s;

            if (px < 0 || px >= FIELD_W) continue;
            sx = (px - x) * sw / w;
            s = srow[sx];

            if (!use_alpha) {
                fb[py][px] = s & 0x00FFFFFF;
            } else {
                unsigned int a = (s >> 24) & 0xFF;
                if (a == 0) continue;
                if (a == 255) {
                    fb[py][px] = s & 0x00FFFFFF;
                } else {
                    color_t d = fb[py][px];
                    unsigned int ia = 255 - a;
                    unsigned int r = (((s >> 16) & 0xFF) * a + ((d >> 16) & 0xFF) * ia) / 255;
                    unsigned int g = (((s >>  8) & 0xFF) * a + ((d >>  8) & 0xFF) * ia) / 255;
                    unsigned int b = (( s        & 0xFF) * a + ( d        & 0xFF) * ia) / 255;
                    fb[py][px] = (r << 16) | (g << 8) | b;
                }
            }
        }
    }
}

void video_fill_rect(int x, int y, int w, int h, color_t c)
{
    int px, py;
    for (py = y; py < y + h; py++) {
        if (py < 0 || py >= FIELD_H) continue;
        for (px = x; px < x + w; px++) {
            if (px < 0 || px >= FIELD_W) continue;
            fb[py][px] = c;
        }
    }
}

/* ---- gomulu gorsel taklidi: data klasorundeki bin dosyalari yuklenir ---- */
const unsigned char *arkaplan_bin = NULL;

static void load_asset(const char *path, const unsigned char **dst)
{
    FILE *f = fopen(path, "rb");
    long n;
    unsigned char *buf;

    if (f == NULL) {
        printf("  (gorsel yok, atlandi: %s)\n", path);
        return;
    }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = (unsigned char *)malloc(n);
    if (buf != NULL && fread(buf, 1, n, f) == (size_t)n) {
        /* data/*.bin PS3 icin big-endian ARGB yazilir. Onizleme little-endian
         * bir makinede calisiyorsa bayt sirasi cevrilmeli, aksi halde kirmizi
         * ve mavi yer degistirmis gorunur ve onizleme yanlis bilgi verir. */
        unsigned int probe = 1;
        if (*(unsigned char *)&probe == 1) {
            long k;
            for (k = 0; k + 3 < n; k += 4) {
                unsigned char t;
                t = buf[k];     buf[k]     = buf[k + 3]; buf[k + 3] = t;
                t = buf[k + 1]; buf[k + 1] = buf[k + 2]; buf[k + 2] = t;
            }
        }
        *dst = buf;
    }
    fclose(f);
    printf("  yuklendi: %s (%ld bayt)\n", path, n);
}

/* ---- input stub: menuyu istedigimiz ekrana surmek icin ---- */
static unsigned int stub_pressed = 0;
static int stub_dir = 0;

int  input_init(void) { return 0; }
void input_exit(void) { }
void input_update(void) { }
int  input_connected(int pad) { (void)pad; return 1; }
int  input_held(int pad, unsigned int m) { (void)pad; (void)m; return 0; }
int  input_pressed(int pad, unsigned int m) { (void)pad; return (stub_pressed & m) != 0; }
int  input_dir(int pad) { (void)pad; return 0; }
int  input_any_pressed(unsigned int m) { return (stub_pressed & m) != 0; }
int  input_any_dir_pressed(void) { return stub_dir; }

static void press(unsigned int mask, int dir)
{
    stub_pressed = mask;
    stub_dir = dir;
    menu_update();
    stub_pressed = 0;
    stub_dir = 0;
}

static void save_ppm(const char *path)
{
    FILE *f = fopen(path, "wb");
    int x, y;

    if (f == NULL) {
        fprintf(stderr, "yazilamadi: %s\n", path);
        exit(1);
    }
    fprintf(f, "P6\n%d %d\n255\n", FIELD_W, FIELD_H);
    for (y = 0; y < FIELD_H; y++) {
        for (x = 0; x < FIELD_W; x++) {
            color_t c = fb[y][x];
            unsigned char rgb[3];
            rgb[0] = (c >> 16) & 0xFF;
            rgb[1] = (c >> 8) & 0xFF;
            rgb[2] = c & 0xFF;
            fwrite(rgb, 1, 3, f);
        }
    }
    fclose(f);
    printf("  yazildi: %s\n", path);
}

int main(void)
{
    Game g;

    load_asset("data/arkaplan.bin", &arkaplan_bin);

    /* 1) ana menu */
    menu_init();
    menu_draw();
    save_ppm("build-test/01-ana-menu.ppm");

    /* 2) Baslat alt menusu (X ile gir) */
    press(PAD_CROSS, 0);
    menu_draw();
    save_ppm("build-test/02-baslat-menu.ppm");

    /* 3) Hakkinda (geri don, asagi in, X) */
    press(PAD_CIRCLE, 0);
    press(0, 1);
    press(PAD_CROSS, 0);
    menu_draw();
    save_ppm("build-test/03-hakkinda.ppm");

    /* 4) oyun ekrani */
    game_init(&g, MODE_BOT, 12345);
    g.score1 = 7;
    g.score2 = 10;
    g.bx = 500.0f;
    g.by = 300.0f;
    g.p1y = 250.0f;
    g.p2y = 380.0f;
    draw_game(&g);
    save_ppm("build-test/04-oyun.ppm");

    /* 5) duraklatma */
    g.paused = 1;
    draw_game(&g);
    save_ppm("build-test/05-duraklatildi.ppm");

    /* 6) sonuc ekrani */
    g.paused = 0;
    g.score1 = 11;
    g.score2 = 9;
    draw_result(&g, 1);
    save_ppm("build-test/06-sonuc.ppm");

    return 0;
}
