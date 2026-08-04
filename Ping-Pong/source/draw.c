#include <stdio.h>
#ifndef PREVIEW_BUILD
#  include <ppu-types.h>   /* gomulu gorsel basliklarinin kullandigi u8 tipi */
#endif

#include "draw.h"
#include "video.h"
#include "font.h"

/* Derleme sirasinda gomulen gorseller (tools/convert_assets.sh uretir).
 * assets/arkaplan.png yoksa data/arkaplan.bin de olusmaz; o durumda
 * arkaplan cizilmez ve oyun eski duz renkli haliyle calisir. */
#ifdef PREVIEW_BUILD
   /* Onizleme (host) derlemesi: gorsel dosyadan yuklenir */
   extern const unsigned char *arkaplan_bin;
#  define HAS_BACKGROUND 1
#elif __has_include("arkaplan_bin.h")
#  include "arkaplan_bin.h"
#  define HAS_BACKGROUND 1
#endif

/* Arkaplan gorseli kullanildiginda oyun alani (sanal 1280x720) masanin ic
 * yuzeyine eslenir; boylece raketler ve top masanin uzerinde oynar, zeminde
 * degil. Degerler arkaplan gorselindeki masa kenarlarindan olculmustur. */
#ifdef HAS_BACKGROUND
#  define AREA_X0  167
#  define AREA_Y0   93
#  define AREA_X1 1113
#  define AREA_Y1  615
#else
#  define AREA_X0    0
#  define AREA_Y0    0
#  define AREA_X1 FIELD_W
#  define AREA_Y1 FIELD_H
#endif

#define AREA_W (AREA_X1 - AREA_X0)
#define AREA_H (AREA_Y1 - AREA_Y0)

static int map_x(float x) { return AREA_X0 + (int)(x * AREA_W / FIELD_W); }
static int map_y(float y) { return AREA_Y0 + (int)(y * AREA_H / FIELD_H); }
static int map_w(int w)   { return w * AREA_W / FIELD_W; }
static int map_h(int h)   { return h * AREA_H / FIELD_H; }

#define COL_BG      RGB(8, 16, 30)
#define COL_FG      RGB(235, 240, 250)
#define COL_NET     RGB(50, 70, 100)
#define COL_SCORE   RGB(120, 200, 255)
#define COL_HINT    RGB(110, 120, 140)
#define COL_DIM     RGB(255, 210, 60)

#ifndef HAS_BACKGROUND
/* Arkaplan gorseli yoksa saha ortasina kesikli cizgi cizilir. */
static void draw_net(void)
{
    int y;
    for (y = 10; y < FIELD_H - 10; y += 40)
        video_fill_rect(FIELD_W / 2 - 3, y, 6, 24, COL_NET);
}
#endif

/* Arkaplan varsa gorseli, yoksa duz rengi + orta cizgiyi cizer. */
static void draw_background(void)
{
#ifdef HAS_BACKGROUND
    video_draw_image((const color_t *)arkaplan_bin, 1280, 720,
                     0, 0, FIELD_W, FIELD_H, 0);
#else
    video_clear(COL_BG);
    draw_net();
#endif
}

void draw_game(const Game *g)
{
    char buf[16];

    draw_background();

    snprintf(buf, sizeof(buf), "%d", g->score1);
    font_draw_center(FIELD_W / 2 - 220, 40, 7, buf, COL_SCORE);
    snprintf(buf, sizeof(buf), "%d", g->score2);
    font_draw_center(FIELD_W / 2 + 220, 40, 7, buf, COL_SCORE);

    video_fill_rect(map_x(PADDLE_MARGIN), map_y(g->p1y),
                    map_w(PADDLE_W), map_h(PADDLE_H), COL_FG);
    video_fill_rect(map_x(FIELD_W - PADDLE_MARGIN - PADDLE_W), map_y(g->p2y),
                    map_w(PADDLE_W), map_h(PADDLE_H), COL_FG);
    video_fill_rect(map_x(g->bx), map_y(g->by),
                    map_w(BALL_SIZE), map_h(BALL_SIZE), COL_FG);

    if (g->paused) {
        font_draw_center(FIELD_W / 2, FIELD_H / 2 - 40, 6,
                         "DURAKLATILDI", COL_DIM);
        font_draw_center(FIELD_W / 2, FIELD_H / 2 + 40, 2,
                         "START / SELECT: devam    O: menü", COL_HINT);
    }
}

void draw_result(const Game *g, int p1_won)
{
    char buf[32];
    const char *kazanan;

    video_clear(COL_BG);

    if (p1_won)
        kazanan = "Oyuncu 1";
    else
        kazanan = (g->mode == MODE_BOT) ? "Bot" : "Oyuncu 2";

    font_draw_center(FIELD_W / 2, 220, 6, "KAZANAN", COL_SCORE);
    font_draw_center(FIELD_W / 2, 320, 6, kazanan, COL_DIM);

    snprintf(buf, sizeof(buf), "%d - %d", g->score1, g->score2);
    font_draw_center(FIELD_W / 2, 430, 5, buf, COL_FG);

    font_draw_center(FIELD_W / 2, 600, 2, "X: menüye dön", COL_HINT);
}
