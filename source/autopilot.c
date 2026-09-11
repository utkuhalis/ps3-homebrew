#include <math.h>
#include <string.h>

#include "autopilot.h"

static float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

float autopilot_wrap_angle(float a)
{
    while (a >  3.14159265f) a -= 6.28318531f;
    while (a < -3.14159265f) a += 6.28318531f;
    return a;
}

void autopilot_init(Autopilot *ap)
{
    memset(ap, 0, sizeof(*ap));
}

void autopilot_engage(Autopilot *ap, const Flight *f)
{
    ap->engaged = 1;
    ap->target_alt = f->pos[1];
    ap->target_heading = f->yaw;
    ap->target_speed = f->airspeed;

    /* Locking in at very low speed would make the aircraft sink;
     * round up to a sensible cruise speed. */
    /* Lowest sensible cruise values for the 737; the old thresholds tuned for
     * the light jet (70 m/s) were below the stall for this airframe. */
    if (ap->target_speed < 160.0f)
        ap->target_speed = 160.0f;
    if (ap->target_alt < 300.0f)
        ap->target_alt = 300.0f;
}

void autopilot_disengage(Autopilot *ap)
{
    ap->engaged = 0;
}

void autopilot_toggle(Autopilot *ap, const Flight *f)
{
    if (ap->engaged)
        autopilot_disengage(ap);
    else
        autopilot_engage(ap, f);
}

void autopilot_adjust_alt(Autopilot *ap, float delta)
{
    ap->target_alt += delta;
    if (ap->target_alt < 30.0f)
        ap->target_alt = 30.0f;
    if (ap->target_alt > 4000.0f)
        ap->target_alt = 4000.0f;
}

void autopilot_adjust_heading(Autopilot *ap, float delta)
{
    ap->target_heading = autopilot_wrap_angle(ap->target_heading + delta);
}

/* Two-stage control:
 *   altitude error -> target vertical speed -> pitch input
 *   heading error  -> target bank          -> roll input
 * Every stage is limited, so the autopilot never manoeuvres hard. */
void autopilot_update(const Autopilot *ap, Flight *f, float dt)
{
    float alt_err, vs_target, vs_now, pitch_cmd;
    float head_err, bank_target, roll_cmd;
    float spd_err;

    (void)dt;

    if (!ap->engaged)
        return;

    /* --- altitude --- */
    alt_err = ap->target_alt - f->pos[1];
    vs_target = clampf(alt_err * 0.06f, -AP_VS_LIMIT_MS, AP_VS_LIMIT_MS);
    vs_now = f->vel[1];
    pitch_cmd = clampf((vs_target - vs_now) * 0.10f - f->p_rate * 2.40f,
                       -1.0f, 1.0f);

    /* Does not force the nose near the stall: the autopilot must not drop the aircraft. */
    if (f->stalled && pitch_cmd > 0.0f)
        pitch_cmd = -0.35f;

    f->in_pitch = pitch_cmd;

    /* --- heading: error -> target bank -> roll input --- */
    head_err = autopilot_wrap_angle(ap->target_heading - f->yaw);
    bank_target = clampf(head_err * 1.10f, -AP_MAX_BANK_RAD, AP_MAX_BANK_RAD);

    /* Without the damping term (angular rate) the autopilot overshot the
     * target bank: the aircraft has inertia and keeps rolling after the input
     * stops. */
    roll_cmd = clampf((bank_target - f->roll) * 4.00f - f->r_rate * 3.20f,
                      -1.0f, 1.0f);
    f->in_roll = roll_cmd;
    f->in_yaw = 0.0f;

    /* --- speed: throttle --- */
    spd_err = ap->target_speed - f->airspeed;
    f->throttle = clampf(f->throttle + spd_err * 0.004f * dt * 60.0f,
                         0.0f, 1.0f);
}
