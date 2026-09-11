#include <math.h>
#include <stdio.h>

#include "gauges.h"
#include "overlay.h"
#include "font.h"

#define PI 3.14159265f

/* The dial draws a 270-degree arc from the 7 o'clock to the 5 o'clock position */
#define SWEEP_START (PI * 0.75f)
#define SWEEP_TOTAL (PI * 1.5f)

#define COL_FACE    RGB(18, 20, 24)
#define COL_RIM     RGB(150, 158, 170)
#define COL_TICK    RGB(215, 222, 232)
#define COL_NEEDLE  RGB(245, 248, 255)
#define COL_HUB     RGB(90, 96, 108)
#define COL_TEXT    RGB(225, 232, 242)
#define COL_LABEL   RGB(140, 150, 165)
#define COL_BOX     RGB(10, 12, 16)
#define COL_SKY     RGB(74, 132, 196)
#define COL_GROUND  RGB(122, 88, 48)

static float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

float gauge_speed_angle(float kmh)
{
    float t = clampf(kmh / SPEED_MAX_KMH, 0.0f, 1.0f);
    return SWEEP_START + t * SWEEP_TOTAL;
}

float gauge_alt_angle(float meters)
{
    float t = clampf(meters / ALT_MAX_M, 0.0f, 1.0f);
    return SWEEP_START + t * SWEEP_TOTAL;
}

/* Dial body: dark face + metal bezel + ticks */
static void draw_dial(float cx, float cy, float r, int ticks)
{
    int i;

    overlay_disc(cx, cy, r, COL_FACE, 225);
    overlay_ring(cx, cy, r, 3.0f, COL_RIM, 235);

    for (i = 0; i <= ticks; i++) {
        float t = (float)i / ticks;
        float ang = SWEEP_START + t * SWEEP_TOTAL;
        int major = (i % 2 == 0);
        float len = major ? 10.0f : 6.0f;
        float rr = r - 6.0f - len * 0.5f;

        overlay_rot_rect(cx + cosf(ang) * rr, cy + sinf(ang) * rr,
                         len, major ? 3.0f : 2.0f, ang,
                         COL_TICK, major ? 230 : 160);
    }
}

static void draw_needle(float cx, float cy, float r, float ang)
{
    float len = r * 0.78f;

    overlay_rot_rect(cx + cosf(ang) * len * 0.5f,
                     cy + sinf(ang) * len * 0.5f,
                     len, 3.5f, ang, COL_NEEDLE, 245);
    overlay_disc(cx, cy, 5.0f, COL_HUB, 245);
}

/* Number box under the gauge */
static void draw_value_box(float cx, float cy, const char *value,
                           const char *label)
{
    int w = 92, h = 22;
    int x = (int)(cx - w / 2);
    int y = (int)cy;

    overlay_blend_rect(x, y, w, h, COL_BOX, 210);
    overlay_fill_rect(x, y, w, 1, COL_RIM);
    font_draw_center((int)cx, y + 5, 2, value, COL_TEXT);
    font_draw_center((int)cx, y + h + 6, 1, label, COL_LABEL);
}

/* Signed distance of a point from the horizon line.
 *
 * The horizon is a line shifted by `shift` from the gauge centre and
 * rotated by `roll`. m = (sin roll, -cos roll) is its "up" normal; with
 * roll = 0, m points up on screen (screen y is positive downwards).
 *
 * d > 0 -> sky side,  d < 0 -> ground side.
 * Pure function: unit tested. */
float gauge_horizon_dist(float dx, float dy, float roll, float shift)
{
    return dx * sinf(roll) - dy * cosf(roll) - shift;
}

/* How far the horizon shifts inside the gauge for a given pitch.
 * With the nose up (pitch > 0) the horizon moves DOWN on screen, hence the
 * reversed sign. With this sign wrong in an earlier version, the ground
 * was on top and the sky at the bottom. */
float gauge_horizon_shift(float pitch, float r)
{
    return -clampf(pitch / (PI * 0.5f), -1.0f, 1.0f) * r * 0.9f;
}

/* Artificial horizon: the sky/ground split rotates and shifts with the
 * aircraft's bank and pitch.
 *
 * The fill is drawn by scanlines, not rotated rectangles: for every
 * horizontal line the circle's width is found first, then where the
 * horizon crosses that line, and the pieces are filled separately. A
 * rotated rectangle cannot be clipped to the circle, so in an earlier
 * version the fill spilled outside the gauge; this way spilling is
 * mathematically impossible. */
void gauges_draw_attitude(float cx, float cy, float r, float pitch, float roll)
{
    float shift = gauge_horizon_shift(pitch, r);
    float sa = sinf(roll);
    float inner = r - 3.0f;         /* inset by the bezel thickness */
    int i, iy;

    for (iy = -(int)inner; iy <= (int)inner; iy++) {
        float dy = (float)iy;
        float hw = sqrtf(inner * inner - dy * dy);
        int y = (int)(cy + dy);
        int xl = (int)(cx - hw);
        int w = (int)(hw * 2.0f);
        float xc;

        if (w <= 0)
            continue;

        /* Without bank the whole row is one colour: no crossing. */
        if (sa > -0.02f && sa < 0.02f) {
            float d = gauge_horizon_dist(0.0f, dy, roll, shift);

            overlay_fill_rect(xl, y, w, 1, d > 0.0f ? COL_SKY : COL_GROUND);
            continue;
        }

        /* d(x) = (x-cx)*sa - dy*ca - shift = 0  ->  crossing point */
        xc = cx + (dy * cosf(roll) + shift) / sa;

        if (xc <= (float)xl) {
            /* the whole row is right of the crossing */
            overlay_fill_rect(xl, y, w, 1, sa > 0.0f ? COL_SKY : COL_GROUND);
        } else if (xc >= (float)(xl + w)) {
            overlay_fill_rect(xl, y, w, 1, sa > 0.0f ? COL_GROUND : COL_SKY);
        } else {
            int lw = (int)(xc - xl);

            overlay_fill_rect(xl, y, lw, 1,
                              sa > 0.0f ? COL_GROUND : COL_SKY);
            overlay_fill_rect(xl + lw, y, w - lw, 1,
                              sa > 0.0f ? COL_SKY : COL_GROUND);
        }
    }

    /* horizon line and pitch ladder: short enough to stay inside the circle */
    {
        float ca = cosf(roll);

        overlay_rot_rect(cx + sa * shift, cy - ca * shift,
                         inner * 1.9f, 2.0f, -roll, COL_TICK, 245);

        for (i = -2; i <= 2; i++) {
            float off = shift + i * r * 0.22f;
            float len = (i % 2 == 0) ? r * 0.40f : r * 0.22f;

            if (i == 0)
                continue;
            if (off > inner * 0.8f || off < -inner * 0.8f)
                continue;       /* ladder rungs that would leave the gauge are not drawn */
            overlay_rot_rect(cx + sa * off, cy - ca * off, len, 1.5f, -roll,
                             COL_TICK, 170);
        }
    }

    /* circular bezel */
    overlay_ring(cx, cy, r, 3.0f, COL_RIM, 235);

    /* fixed aircraft symbol */
    overlay_fill_rect((int)(cx - 22), (int)(cy - 1), 16, 3, COL_NEEDLE);
    overlay_fill_rect((int)(cx + 6), (int)(cy - 1), 16, 3, COL_NEEDLE);
    overlay_disc(cx, cy, 3.0f, COL_NEEDLE, 250);
}

void gauges_draw(float speed_kmh, float altitude_m, float pitch_rad,
                 float roll_rad)
{
    float r = 52.0f;
    float y = 560.0f;
    float x1 = 940.0f, x2 = 1080.0f, x3 = 1220.0f;
    char buf[24];

    /* speed */
    draw_dial(x1, y, r, 8);
    draw_needle(x1, y, r, gauge_speed_angle(speed_kmh));
    snprintf(buf, sizeof(buf), "%d", (int)(speed_kmh + 0.5f));
    draw_value_box(x1, y + r + 10.0f, buf, "SPEED km/h");

    /* artificial horizon */
    gauges_draw_attitude(x2, y, r, pitch_rad, roll_rad);
    snprintf(buf, sizeof(buf), "%d", (int)(pitch_rad * 180.0f / PI));
    draw_value_box(x2, y + r + 10.0f, buf, "PITCH");

    /* altitude */
    draw_dial(x3, y, r, 8);
    draw_needle(x3, y, r, gauge_alt_angle(altitude_m));
    snprintf(buf, sizeof(buf), "%d", (int)(altitude_m + 0.5f));
    draw_value_box(x3, y + r + 10.0f, buf, "ALTITUDE m");
}
