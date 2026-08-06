#include <stdio.h>
#include <math.h>

#include "hud.h"
#include "overlay.h"
#include "font.h"

#define EMPTY_WEIGHT_KG  3800.0f    /* yakitsiz agirlik */
#define FUEL_FULL_KG      900.0f    /* dolu yakitin agirligi */
#define FUEL_BURN_PCT     0.25f     /* saniyede tuketilen yuzde */

#define COL_LABEL  RGB(150, 190, 220)
#define COL_VALUE  RGB(245, 250, 255)
#define COL_PANEL  RGB(4, 10, 20)
#define COL_WARN   RGB(255, 120, 90)

void hud_init(Hud *h)
{
    h->fuel_pct = 100.0f;
    h->weight_kg = EMPTY_WEIGHT_KG + FUEL_FULL_KG;
    h->speed_kmh = 0.0f;
    h->has_prev = 0;
}

void hud_update(Hud *h, const Camera *cam, float dt)
{
    if (dt <= 0.0f)
        return;

    if (h->has_prev) {
        float dx = cam->pos[0] - h->prev_pos[0];
        float dy = cam->pos[1] - h->prev_pos[1];
        float dz = cam->pos[2] - h->prev_pos[2];
        float dist = sqrtf(dx * dx + dy * dy + dz * dz);
        float inst = (dist / dt) * 3.6f;    /* birim/s -> km/s benzeri olcek */

        /* ani sicramalari yumusat */
        h->speed_kmh += (inst - h->speed_kmh) * 0.15f;
    }

    h->prev_pos[0] = cam->pos[0];
    h->prev_pos[1] = cam->pos[1];
    h->prev_pos[2] = cam->pos[2];
    h->has_prev = 1;

    h->fuel_pct -= FUEL_BURN_PCT * dt;
    if (h->fuel_pct < 0.0f)
        h->fuel_pct = 0.0f;

    h->weight_kg = EMPTY_WEIGHT_KG + FUEL_FULL_KG * (h->fuel_pct / 100.0f);
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
    row(30, "AĞIRLIK", buf, COL_VALUE);

    snprintf(buf, sizeof(buf), "%%%d", (int)(h->fuel_pct + 0.5f));
    row(62, "YAKIT", buf, h->fuel_pct < 20.0f ? COL_WARN : COL_VALUE);

    heading = cam->yaw * 180.0f / 3.14159265f;
    while (heading < 0.0f)   heading += 360.0f;
    while (heading >= 360.0f) heading -= 360.0f;
    snprintf(buf, sizeof(buf), "%d derece", (int)(heading + 0.5f));
    row(94, "YÖN", buf, COL_VALUE);
}
