#include <math.h>
#include <stdio.h>

#include "minimap.h"
#include "overlay.h"
#include "font.h"
#include "runway.h"

#define MAP_X      24
#define MAP_Y     512
#define MAP_W     250
#define MAP_H     184
#define HEAD_H     22

/* Haritanin kapsadigi dunya alani (birim) */
#define MAP_RANGE  4000.0f

/* Rota uclari: gercek pist konumlari (runway.c'de tanimli) */
#define START_POS RUNWAY_POS[0]
#define END_POS   RUNWAY_POS[1]

#define COL_BG     RGB(12, 20, 28)
#define COL_EDGE   RGB(150, 158, 170)
#define COL_HEAD   RGB(8, 12, 18)
#define COL_TEXT   RGB(220, 230, 240)
#define COL_ROUTE  RGB(240, 200, 90)
#define COL_START  RGB(120, 220, 140)
#define COL_END    RGB(240, 120, 110)
#define COL_PLANE  RGB(255, 255, 255)

/* Dunya konumunu harita kutusu icindeki piksele cevirir */
static void world_to_map(float wx, float wz, float *mx, float *my)
{
    float half = MAP_RANGE * 0.5f;
    float u = (wx + half) / MAP_RANGE;
    float v = (wz + half) / MAP_RANGE;

    if (u < 0.0f) u = 0.0f;
    if (u > 1.0f) u = 1.0f;
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;

    *mx = MAP_X + 8.0f + u * (MAP_W - 16.0f);
    *my = MAP_Y + HEAD_H + 8.0f + v * (MAP_H - HEAD_H - 16.0f);
}

void minimap_draw(const Camera *cam)
{
    float sx, sy, ex, ey, px, py;
    float dx, dy, len, ang;
    char buf[32];
    int i, steps;

    /* kutu ve baslik seridi */
    overlay_blend_rect(MAP_X, MAP_Y, MAP_W, MAP_H, COL_BG, 200);
    overlay_blend_rect(MAP_X, MAP_Y, MAP_W, HEAD_H, COL_HEAD, 225);
    overlay_fill_rect(MAP_X, MAP_Y, MAP_W, 1, COL_EDGE);
    overlay_fill_rect(MAP_X, MAP_Y + MAP_H - 1, MAP_W, 1, COL_EDGE);
    overlay_fill_rect(MAP_X, MAP_Y, 1, MAP_H, COL_EDGE);
    overlay_fill_rect(MAP_X + MAP_W - 1, MAP_Y, 1, MAP_H, COL_EDGE);

    font_draw_text(MAP_X + 8, MAP_Y + 7, 1, "DEPARTURE - ARRIVAL", COL_TEXT);

    world_to_map(START_POS[0], START_POS[1], &sx, &sy);
    world_to_map(END_POS[0], END_POS[1], &ex, &ey);
    world_to_map(cam->pos[0], cam->pos[2], &px, &py);

    /* rota: kisa parcalarla cizilen kesikli cizgi */
    dx = ex - sx;
    dy = ey - sy;
    len = sqrtf(dx * dx + dy * dy);
    steps = (int)(len / 9.0f);
    if (steps > 40) steps = 40;

    for (i = 0; i <= steps; i++) {
        float t = (float)i / (steps > 0 ? steps : 1);
        if ((i & 1) == 0)
            overlay_fill_rect((int)(sx + dx * t - 1.5f),
                              (int)(sy + dy * t - 1.5f), 3, 3, COL_ROUTE);
    }

    /* uclar */
    overlay_disc(sx, sy, 4.5f, COL_START, 240);
    overlay_disc(ex, ey, 4.5f, COL_END, 240);

    /* ucak: bakis yonune donmus ucgen yerine kisa bir ok */
    ang = cam->yaw;
    overlay_rot_rect(px, py, 14.0f, 3.0f, ang - 1.5708f, COL_PLANE, 250);
    overlay_rot_rect(px, py, 7.0f, 3.0f, ang, COL_PLANE, 250);

    /* kalan mesafe */
    dx = END_POS[0] - cam->pos[0];
    dy = END_POS[1] - cam->pos[2];
    snprintf(buf, sizeof(buf), "%d units", (int)sqrtf(dx * dx + dy * dy));
    font_draw_text(MAP_X + MAP_W - 92, MAP_Y + 7, 1, buf, COL_ROUTE);
}
