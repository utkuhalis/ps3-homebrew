#include <stdio.h>
#include <math.h>

#include "hud.h"
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
    row(30, "AĞIRLIK", buf, COL_VALUE);

    snprintf(buf, sizeof(buf), "%%%d", (int)(h->fuel_pct + 0.5f));
    row(62, "YAKIT", buf, h->fuel_pct < 20.0f ? COL_WARN : COL_VALUE);

    heading = cam->yaw * 180.0f / 3.14159265f;
    while (heading < 0.0f)   heading += 360.0f;
    while (heading >= 360.0f) heading -= 360.0f;
    snprintf(buf, sizeof(buf), "%d derece", (int)(heading + 0.5f));
    row(94, "YÖN", buf, COL_VALUE);
}
