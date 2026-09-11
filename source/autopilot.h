#ifndef AUTOPILOT_H
#define AUTOPILOT_H

#include "flight.h"

/* Autopilot: holds altitude, heading and speed.
 *
 * Hardware independent and pure (unit tested). It never touches the
 * aircraft directly; it produces control inputs, just like the player.
 * That way the flight model runs from one place and the autopilot obeys
 * the same physics. */

#define AP_MAX_BANK_RAD    0.42f    /* ~24 degrees, airliner comfort */
#define AP_MAX_PITCH_RAD   0.22f
#define AP_VS_LIMIT_MS      8.0f    /* limit on target climb/descent rate */

typedef struct {
    int   engaged;
    float target_alt;       /* metres */
    float target_heading;   /* radians */
    float target_speed;     /* m/s */
} Autopilot;

void autopilot_init(Autopilot *ap);

/* Engages: locks the current altitude, heading and speed as targets. */
void autopilot_engage(Autopilot *ap, const Flight *f);
void autopilot_disengage(Autopilot *ap);
void autopilot_toggle(Autopilot *ap, const Flight *f);

/* Nudges the targets by hand (menu or buttons) */
void autopilot_adjust_alt(Autopilot *ap, float delta);
void autopilot_adjust_heading(Autopilot *ap, float delta);

/* Produces control inputs. Does not touch the aircraft unless engaged. */
void autopilot_update(const Autopilot *ap, Flight *f, float dt);

/* Wraps to -pi..pi (used for heading differences) */
float autopilot_wrap_angle(float a);

#endif
