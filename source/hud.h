#ifndef HUD_H
#define HUD_H

#include "autopilot.h"
#include "camera.h"
#include "flight.h"

/* Flight instruments.
 *
 * All values - speed, altitude, weight and fuel - come from the flight
 * model; fuel burns over time and the weight drops with it. */

typedef struct {
    float fuel_pct;     /* 0..100 */
    float weight_kg;
    float speed_kmh;
    float prev_pos[3];
    int   has_prev;
} Hud;

void hud_init(Hud *h);

/* The values come from the flight model: speed, weight and fuel are real. */
void hud_update(Hud *h, const Flight *f, float dt);

void hud_draw(const Hud *h, const Camera *cam);

/* Throttle, flap, spoiler and landing gear state */
void hud_draw_controls(const Flight *f);

/* Profiler panel: frame time section by section */
void hud_draw_profiler(void);

/* Throttle (physical lever look) */
void hud_draw_throttle_lever(const Flight *f, const Autopilot *ap);

/* System warnings: STALL, OVER G, LOW FUEL */
void hud_draw_warnings(const Flight *f, unsigned long frame);

/* Control help shown until takeoff */
void hud_draw_help(const Flight *f);

#endif
