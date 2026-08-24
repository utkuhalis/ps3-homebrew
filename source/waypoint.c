#include <math.h>
#include <stdio.h>

#include "waypoint.h"
#include "overlay.h"
#include "font.h"
#include "runway.h"

#define EDGE_TOP     90
#define EDGE_BOTTOM 470
#define MARGIN 46.0f

#define COL_MARK   RGB(240, 205, 95)
#define COL_TEXT   RGB(245, 235, 200)
#define COL_EDGE   RGB(255, 170, 60)

void waypoint_project(const Mat4 *vp, const Camera *cam, const float world[3],
                      WaypointView *out)
{
    float in[4], clip[4];
    float dx, dy, dz;

    in[0] = world[0]; in[1] = world[1]; in[2] = world[2]; in[3] = 1.0f;
    mat4_transform(vp, in, clip);

    dx = world[0] - cam->pos[0];
    dy = world[1] - cam->pos[1];
    dz = world[2] - cam->pos[2];
    out->distance = sqrtf(dx * dx + dy * dy + dz * dz);

    out->behind = (clip[3] <= 0.001f);

    if (out->behind) {
        /* Arkadaysa yansitma anlamsiz; yalnizca yon bilgisi uretilir. */
        float rel = atan2f(dx, -dz) - cam->yaw;

        while (rel < -3.14159265f) rel += 6.28318531f;
        while (rel >  3.14159265f) rel -= 6.28318531f;

        out->sx = rel > 0.0f ? OVL_W - MARGIN : MARGIN;
        out->sy = OVL_H * 0.5f;
        out->on_screen = 0;
        out->edge_angle = rel > 0.0f ? 0.0f : 3.14159265f;
        return;
    }

    out->sx = (clip[0] / clip[3] * 0.5f + 0.5f) * OVL_W;
    out->sy = (1.0f - (clip[1] / clip[3] * 0.5f + 0.5f)) * OVL_H;

    out->on_screen = (out->sx > MARGIN && out->sx < OVL_W - MARGIN &&
                      out->sy > MARGIN && out->sy < OVL_H - MARGIN);

    if (!out->on_screen) {
        float cx = OVL_W * 0.5f, cy = OVL_H * 0.5f;

        out->edge_angle = atan2f(out->sy - cy, out->sx - cx);

        if (out->sx < MARGIN)          out->sx = MARGIN;
        if (out->sx > OVL_W - MARGIN)  out->sx = OVL_W - MARGIN;
        /* Kenar oklari HUD panellerinin uzerine binmesin: alt serit
         * gostergelere, en ust serit gorev listesine ayrilmistir. */
        if (out->sy < EDGE_TOP)     out->sy = EDGE_TOP;
        if (out->sy > EDGE_BOTTOM)  out->sy = EDGE_BOTTOM;
    } else {
        out->edge_angle = 0.0f;
    }
}

static void draw_marker(const WaypointView *w, const char *name)
{
    char buf[32];

    if (w->on_screen) {
        /* hedef gorus alaninda: halka + ince artı + etiket */
        overlay_ring(w->sx, w->sy, 20.0f, 2.5f, COL_MARK, 235);
        overlay_fill_rect((int)(w->sx - 1), (int)(w->sy - 28), 2, 10, COL_MARK);
        overlay_fill_rect((int)(w->sx - 1), (int)(w->sy + 18), 2, 10, COL_MARK);
        overlay_fill_rect((int)(w->sx - 28), (int)(w->sy - 1), 10, 2, COL_MARK);
        overlay_fill_rect((int)(w->sx + 18), (int)(w->sy - 1), 10, 2, COL_MARK);

        font_draw_center((int)w->sx, (int)(w->sy + 30), 1, name, COL_TEXT);
        snprintf(buf, sizeof(buf), "%d", (int)w->distance);
        font_draw_center((int)w->sx, (int)(w->sy + 44), 1, buf, COL_TEXT);
    } else {
        /* ekran disinda: kenarda ok */
        overlay_rot_rect(w->sx, w->sy, 24.0f, 5.0f, w->edge_angle, COL_EDGE, 235);
        overlay_rot_rect(w->sx - cosf(w->edge_angle) * 9.0f,
                         w->sy - sinf(w->edge_angle) * 9.0f,
                         10.0f, 12.0f, w->edge_angle, COL_EDGE, 200);
        font_draw_center((int)w->sx, (int)(w->sy + 22), 1, name, COL_TEXT);
    }
}

void waypoint_draw_all(const Camera *cam, const Mat4 *proj)
{
    Mat4 view = camera_view_matrix(cam);
    Mat4 vp = mat4_mul(proj, &view);
    int i;

    for (i = 0; i < RUNWAY_COUNT; i++) {
        float world[3];
        WaypointView wv;

        world[0] = RUNWAY_POS[i][0];
        world[1] = RUNWAY_DECK_Y + 30.0f;   /* pistin biraz uzerinde */
        world[2] = RUNWAY_POS[i][1];

        waypoint_project(&vp, cam, world, &wv);
        draw_marker(&wv, runway_name(i));
    }
}
