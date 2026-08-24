#include <stdio.h>
#include <math.h>

#include "hud.h"
#include "autopilot.h"
#include "profiler.h"
#include "overlay.h"
#include "font.h"


#define COL_LABEL  RGB(150, 190, 220)
#define COL_VALUE  RGB(245, 250, 255)
#define COL_PANEL  RGB(4, 10, 20)
#define COL_WARN   RGB(255, 120, 90)

void hud_init(Hud *h)
{
    h->fuel_pct = 100.0f;
    h->weight_kg = EMPTY_MASS_KG + FUEL_FULL_KG;
    h->speed_kmh = 0.0f;
    h->has_prev = 0;
}

void hud_update(Hud *h, const Flight *f, float dt)
{
    (void)dt;

    /* Artik gostermelik degil: degerler dogrudan ucus modelinden geliyor. */
    h->speed_kmh = flight_speed_kmh(f);
    h->weight_kg = f->mass_kg;
    h->fuel_pct = flight_fuel_pct(f);
}

static void row(int y, const char *label, const char *value, color_t vcol)
{
    font_draw_text(28, y, 2, label, COL_LABEL);
    font_draw_text(184, y, 2, value, vcol);
}

void hud_draw(const Hud *h, const Camera *cam)
{
    char buf[32];
    float heading;

    /* Hiz ve yukseklik analog gostergelerde; burada yalnizca onlarda
     * bulunmayan degerler var. */
    overlay_blend_rect(16, 16, 300, 112, COL_PANEL, 165);
    overlay_fill_rect(16, 16, 300, 1, RGB(150, 158, 170));

    snprintf(buf, sizeof(buf), "%d kg", (int)(h->weight_kg + 0.5f));
    row(30, "WEIGHT", buf, COL_VALUE);

    snprintf(buf, sizeof(buf), "%%%d", (int)(h->fuel_pct + 0.5f));
    row(62, "FUEL", buf, h->fuel_pct < 20.0f ? COL_WARN : COL_VALUE);

    heading = cam->yaw * 180.0f / 3.14159265f;
    while (heading < 0.0f)   heading += 360.0f;
    while (heading >= 360.0f) heading -= 360.0f;
    snprintf(buf, sizeof(buf), "%d deg", (int)(heading + 0.5f));
    row(94, "HEADING", buf, COL_VALUE);
}

/* Olcum paneli: kare suresinin nereye gittigini bolum bolum gosterir.
 *
 * FLIP buyukse darbogaz GPU'dadir (CPU, RSX'in kareyi bitirmesini bekliyor);
 * digerleri buyukse is CPU tarafindadir. Bu ayrim olmadan neyi
 * iyilestirdigimizi bilemeyiz. */
void hud_draw_profiler(void)
{
    int x = 940, y = 60, w = 320, h = 190;
    int i;
    char buf[64];
    float total = prof_frame_us();
    color_t label = RGB(150, 165, 185);
    color_t value = RGB(230, 240, 252);

    overlay_blend_rect(x, y, w, h, RGB(6, 10, 18), 205);
    overlay_fill_rect(x, y, w, 2, RGB(120, 190, 255));

    snprintf(buf, sizeof(buf), "%.1f ms   %.0f fps",
             total / 1000.0f, prof_fps());
    font_draw_text(x + 10, y + 10, 2, buf, RGB(140, 235, 170));

    for (i = 0; i < PROF_COUNT; i++) {
        float us = prof_avg_us((ProfSection)i);
        int ry = y + 40 + i * 17;
        int bar;

        font_draw_text(x + 10, ry, 1, prof_name((ProfSection)i), label);

        snprintf(buf, sizeof(buf), "%5.2f", us / 1000.0f);
        font_draw_text(x + 70, ry, 1, buf, value);

        /* oransal cubuk: kare suresinin yuzdesi */
        bar = (total > 1.0f) ? (int)(us / total * 180.0f) : 0;
        if (bar > 180)
            bar = 180;
        overlay_fill_rect(x + 120, ry + 1, bar, 6,
                          (i == PROF_FLIP) ? RGB(230, 170, 70)
                                           : RGB(90, 170, 240));
    }

    snprintf(buf, sizeof(buf), "tri %u   rect %u",
             prof_triangles(), prof_rects());
    font_draw_text(x + 10, y + h - 20, 1, buf, label);
}

/* Gaz kolu: yandan gorunen fiziksel bir kol.
 *
 * Rakam tek basina yetmiyordu - kolun nerede oldugunu gormek gerekiyor. */
void hud_draw_throttle_lever(const Flight *f, const Autopilot *ap)
{
    int x = 26, y = 300, w = 34, h = 190;
    int knob_y;
    color_t rail = RGB(52, 58, 70);
    color_t frame = RGB(150, 158, 172);
    char buf[24];

    overlay_blend_rect(x - 8, y - 26, w + 78, h + 62, RGB(8, 12, 20), 175);
    font_draw_text(x - 2, y - 22, 1, "THROTTLE", RGB(150, 165, 185));

    /* kizak */
    overlay_fill_rect(x + w / 2 - 3, y, 6, h, rail);

    /* kademe cizgileri: %0, 25, 50, 75, 100 */
    {
        int i;

        for (i = 0; i <= 4; i++) {
            int ty = y + h - (h * i) / 4;

            overlay_fill_rect(x + w / 2 + 6, ty - 1, 10, 2,
                              RGB(110, 120, 135));
        }
    }

    /* dolu kisim: alttan kolun bulundugu yere kadar */
    knob_y = y + h - (int)(f->throttle * h);
    overlay_blend_rect(x + w / 2 - 3, knob_y, 6, y + h - knob_y,
                       RGB(90, 200, 120), 220);

    /* kol basi */
    overlay_fill_rect(x, knob_y - 7, w, 14, RGB(226, 232, 242));
    overlay_fill_rect(x, knob_y - 7, w, 2, frame);
    overlay_fill_rect(x, knob_y + 5, w, 2, RGB(120, 128, 142));

    snprintf(buf, sizeof(buf), "%d%%", (int)(f->throttle * 100.0f + 0.5f));
    font_draw_text(x + w + 10, knob_y - 4, 1, buf, RGB(235, 242, 252));

    /* otopilot gaz kolunu kendisi suruyorsa belirt */
    if (ap != NULL && ap->engaged)
        font_draw_text(x - 2, y + h + 12, 1, "AUTOPILOT", RGB(120, 210, 255));
}

/* Sistem uyarilari: stall, asiri G, yakit.
 * Yanip sonme kare sayacindan gelir; ses tarafi ayni durumu kullanir. */
void hud_draw_warnings(const Flight *f, unsigned long frame)
{
    int blink = ((frame / 16) & 1);
    int y = 150;

    if (f->stalled) {
        if (blink) {
            overlay_blend_rect(490, y, 300, 46, RGB(120, 0, 0), 210);
            font_draw_center(640, y + 12, 3, "STALL", RGB(255, 235, 235));
        }
        y += 56;
    }

    if (f->g_load > 2.9f) {
        if (blink)
            font_draw_center(640, y, 2, "OVER G", RGB(255, 180, 90));
        y += 34;
    }

    if (flight_fuel_pct(f) < 12.0f) {
        if (blink)
            font_draw_center(640, y, 2, "LOW FUEL", RGB(255, 200, 90));
    }
}

/* Kalkis yapilana kadar ekranin altinda ne yapilacagini gosteren serit.
 * Ucak pistte duruyorken hicbir sey olmamasi kafa karistirici oluyordu. */
void hud_draw_help(const Flight *f)
{
    int y = 636;
    color_t c = RGB(240, 225, 170);
    color_t bg = RGB(8, 12, 20);

    if (f->airborne)
        return;                 /* havalandiktan sonra kaybolur */

    overlay_blend_rect(330, y, 620, 62, bg, 180);
    overlay_fill_rect(330, y, 620, 1, RGB(150, 158, 170));

    if (f->throttle < 0.05f) {
        font_draw_center(640, y + 12, 2, "R1 / R2: APPLY THROTTLE", c);
        font_draw_center(640, y + 38, 1,
                         "build up speed before rotating", RGB(170, 180, 195));
    } else if (f->airspeed < ROTATE_SPEED_MS) {
        char buf[48];

        snprintf(buf, sizeof(buf), "ACCELERATING   %d / %d",
                 (int)(f->airspeed * 3.6f), (int)(ROTATE_SPEED_MS * 3.6f));
        font_draw_center(640, y + 12, 2, buf, c);
        font_draw_center(640, y + 38, 1,
                         "rotate at sufficient speed", RGB(170, 180, 195));
    } else {
        font_draw_center(640, y + 12, 2, "ROTATE", RGB(140, 240, 160));
        font_draw_center(640, y + 38, 1,
                         "pull back on the right stick", RGB(170, 180, 195));
    }
}

/* Ucus yuzeyleri ve motor durumu - kumandanin ne yaptigini gorunur kilar */
void hud_draw_controls(const Flight *f)
{
    char buf[40];
    int y = 140;
    color_t on = RGB(120, 220, 140), off = RGB(130, 140, 155);

    overlay_blend_rect(16, y, 300, 118, COL_PANEL, 165);
    overlay_fill_rect(16, y, 300, 1, RGB(150, 158, 170));

    snprintf(buf, sizeof(buf), "%%%d", (int)(f->throttle * 100.0f + 0.5f));
    font_draw_text(28, y + 14, 2, "THROTTLE", COL_LABEL);
    font_draw_text(184, y + 14, 2, buf, COL_VALUE);

    snprintf(buf, sizeof(buf), "%d", (int)(f->flap * 3.0f + 0.5f));
    font_draw_text(28, y + 42, 2, "FLAPS", COL_LABEL);
    font_draw_text(184, y + 42, 2, buf, f->flap > 0.01f ? on : off);

    font_draw_text(28, y + 70, 2, "SPOILER", COL_LABEL);
    font_draw_text(184, y + 70, 2, f->spoiler > 0.5f ? "OUT" : "IN",
                   f->spoiler > 0.5f ? on : off);

    font_draw_text(28, y + 96, 2, "GEAR", COL_LABEL);
    font_draw_text(184, y + 96, 2, f->gear_down ? "DOWN" : "UP",
                   f->gear_down ? on : off);
}
