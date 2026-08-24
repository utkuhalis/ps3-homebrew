#include "objectives.h"
#include "overlay.h"
#include "font.h"
#include "runway.h"
#include <math.h>

#define PANEL_X   890
#define PANEL_Y    24
#define PANEL_W   366
#define PANEL_H   132

#define COL_PANEL  RGB(10, 14, 20)
#define COL_EDGE   RGB(150, 158, 170)
#define COL_TITLE  RGB(240, 200, 90)
#define COL_TEXT   RGB(215, 225, 238)
#define COL_DONE   RGB(120, 220, 140)
#define COL_BOX    RGB(180, 190, 205)

/* Gorev metinleri ve kurallari tek yerde dursun diye ayni siradalar */
static const char *TEXT[OBJ_COUNT] = {
    "Find the departure runway",
    "Climb above 1000 m",
    "Reach 200 km/h",
    "Approach the arrival runway"
};

/* Bir noktaya yatay uzaklik (yukseklik farki sayilmaz) */
static float flat_dist(const Camera *cam, const float p[2])
{
    float dx = cam->pos[0] - p[0];
    float dz = cam->pos[2] - p[1];

    return sqrtf(dx * dx + dz * dz);
}

const char *objective_text(int index)
{
    if (index < 0 || index >= OBJ_COUNT)
        return "";
    return TEXT[index];
}

void objectives_init(Objectives *o)
{
    int i;

    for (i = 0; i < OBJ_COUNT; i++)
        o->done[i] = 0;
    o->completed_count = 0;
}

void objectives_update(Objectives *o, const Camera *cam, float speed_kmh)
{
    int i, n = 0;

    /* Gorevler bir kez tamamlanir, geri alinmaz */
    if (flat_dist(cam, RUNWAY_POS[0]) < 400.0f)
        o->done[0] = 1;

    if (cam->pos[1] > 1000.0f)
        o->done[1] = 1;

    if (speed_kmh > 200.0f)
        o->done[2] = 1;

    /* inis pistine yaklasma: once kalkis pisti bulunmus olmali */
    if (o->done[0] && flat_dist(cam, RUNWAY_POS[1]) < 400.0f)
        o->done[3] = 1;

    for (i = 0; i < OBJ_COUNT; i++)
        n += o->done[i] ? 1 : 0;
    o->completed_count = n;
}

void objectives_draw(const Objectives *o)
{
    int i;

    overlay_blend_rect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, COL_PANEL, 195);
    overlay_fill_rect(PANEL_X, PANEL_Y, PANEL_W, 2, COL_EDGE);
    overlay_fill_rect(PANEL_X, PANEL_Y + PANEL_H - 1, PANEL_W, 1, COL_EDGE);

    font_draw_text(PANEL_X + 14, PANEL_Y + 10, 2, "OBJECTIVES", COL_TITLE);

    for (i = 0; i < OBJ_COUNT; i++) {
        int y = PANEL_Y + 40 + i * 22;
        int bx = PANEL_X + 16;
        color_t c = o->done[i] ? COL_DONE : COL_TEXT;

        /* onay kutusu */
        overlay_fill_rect(bx, y + 1, 12, 12, COL_BOX);
        overlay_fill_rect(bx + 1, y + 2, 10, 10, COL_PANEL);
        if (o->done[i])
            overlay_fill_rect(bx + 3, y + 4, 6, 6, COL_DONE);

        font_draw_text(bx + 20, y, 1, TEXT[i], c);
    }
}
