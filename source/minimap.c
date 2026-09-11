#include <math.h>
#include <stdio.h>

#include "minimap.h"
#include "overlay.h"
#include "font.h"
#include "runway.h"
#include "flight.h"

#define MAP_X      24
#define MAP_Y     512
#define MAP_W     250
#define MAP_H     184
#define HEAD_H     22

/* World area covered by the map (units) */
#define MAP_RANGE  4000.0f

/* Route ends: the actual runway positions (defined in runway.c) */
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

/* Converts a world position to a pixel inside the map box */
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

void minimap_draw(const Flight *f)
{
    float sx, sy, ex, ey, px, py;
    float dx, dy, len, ang;
    char buf[32];
    int i, steps;

    /* box and title strip */
    overlay_blend_rect(MAP_X, MAP_Y, MAP_W, MAP_H, COL_BG, 200);
    overlay_blend_rect(MAP_X, MAP_Y, MAP_W, HEAD_H, COL_HEAD, 225);
    overlay_fill_rect(MAP_X, MAP_Y, MAP_W, 1, COL_EDGE);
    overlay_fill_rect(MAP_X, MAP_Y + MAP_H - 1, MAP_W, 1, COL_EDGE);
    overlay_fill_rect(MAP_X, MAP_Y, 1, MAP_H, COL_EDGE);
    overlay_fill_rect(MAP_X + MAP_W - 1, MAP_Y, 1, MAP_H, COL_EDGE);

    font_draw_text(MAP_X + 8, MAP_Y + 7, 1, "DEPARTURE - ARRIVAL", COL_TEXT);

    world_to_map(START_POS[0], START_POS[1], &sx, &sy);
    world_to_map(END_POS[0], END_POS[1], &ex, &ey);
    world_to_map(f->pos[0], f->pos[2], &px, &py);

    /* route: dashed line drawn in short segments */
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

    /* ends */
    overlay_disc(sx, sy, 4.5f, COL_START, 240);
    overlay_disc(ex, ey, 4.5f, COL_END, 240);

    /* Aircraft marker: shows the AIRCRAFT's nose direction.
     *
     * It used to use the CAMERA's yaw; orbiting the camera around the
     * aircraft turned the map arrow too, and the aircraft looked as if it was
     * flying sideways.
     *
     * World -> map: X right, Z down. The nose direction is (sin yaw, -cos yaw)
     * in the world and has the same components on the map; the screen angle
     * comes from atan2(dy, dx). */
    {
        float hx = sinf(f->yaw);
        float hy = -cosf(f->yaw);

        ang = atan2f(hy, hx);

        /* fuselage: a long bar along the nose, a small nose mark at the tip */
        overlay_rot_rect(px, py, 15.0f, 3.0f, ang, COL_PLANE, 250);
        overlay_rot_rect(px + hx * 7.0f, py + hy * 7.0f, 5.0f, 5.0f,
                         ang, COL_PLANE, 250);
        /* wings: perpendicular to the nose */
        overlay_rot_rect(px, py, 11.0f, 2.5f, ang + 1.5708f, COL_PLANE, 220);
    }

    /* remaining distance */
    dx = END_POS[0] - f->pos[0];
    dy = END_POS[1] - f->pos[2];
    snprintf(buf, sizeof(buf), "%d units", (int)sqrtf(dx * dx + dy * dy));
    font_draw_text(MAP_X + MAP_W - 92, MAP_Y + 7, 1, buf, COL_ROUTE);
}
