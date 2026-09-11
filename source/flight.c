#include <math.h>
#include <string.h>

#include "flight.h"

#define AIR_DENSITY      1.225f
#define CL_SLOPE         5.2f    /* lift coefficient per radian of angle of attack */
#define CL_FLAP          0.85f   /* lift added by full flaps */
#define CD_BASE          0.028f
#define CD_INDUCED       0.055f
#define CD_FLAP          0.10f
#define CD_SPOILER       0.16f
#define SPOILER_LIFT_LOSS 0.55f  /* full spoilers remove this fraction of lift */

/* Rotation rates at full input (radians/second). The first values
 * (1.25 / 2.10) were 71 and 120 degrees per second; the aircraft turned
 * like a toy. 30-50 degrees per second is realistic for a jet. */
#define PITCH_RATE       0.34f
#define ROLL_RATE        0.85f
#define YAW_RATE         0.26f

/* Static stability: the nose turns into the airflow by itself.
 * This is what makes a real aircraft "fly itself". */
#define PITCH_STABILITY  0.85f
#define FUEL_BURN_KGS    2.20f   /* 737 at full power ~2.2 kg/s */

static float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static float len3(const float v[3])
{
    return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

/* Nose direction (from yaw/pitch). Roll does not change the nose direction. */
static void forward_vec(const Flight *f, float out[3])
{
    float cp = cosf(f->pitch);

    out[0] =  cp * sinf(f->yaw);
    out[1] =  sinf(f->pitch);
    out[2] = -cp * cosf(f->yaw);
}

/* Up direction: the body's (0,1,0) axis in the world.
 * Derived from the attitude matrix, so the lift direction and the model's
 * visible bank can never disagree. */
static void up_vec(const Flight *f, float out[3])
{
    float m[3][3];

    flight_orientation_matrix(f, m);
    out[0] = m[0][1];
    out[1] = m[1][1];
    out[2] = m[2][1];
}

void flight_orientation_matrix(const Flight *f, float m[3][3]);

/* Lift coefficient: grows linearly with angle of attack and collapses
 * quickly past the stall angle. Flaps raise both the coefficient and the
 * critical angle. */
static float lift_coefficient(float aoa, float flap)
{
    float stall = STALL_ANGLE_RAD + flap * 0.06f;
    float cl = aoa * CL_SLOPE + flap * CL_FLAP;
    float mag = aoa < 0.0f ? -aoa : aoa;
    float over;

    /* The wing stalls in both directions. Only the positive side used to be
     * limited; in a nose-down dive the angle of attack went to -1.3 rad, the
     * lift coefficient reached -6, and the aircraft was flung sideways. */
    if (mag <= stall)
        return cl;

    over = (mag - stall) / 0.20f;
    if (over > 1.0f)
        over = 1.0f;
    cl *= (1.0f - 0.85f * over);

    /* At very large angles a wing is no longer a wing: lift goes to zero. */
    if (mag > 1.05f) {
        float fade = (1.57f - mag) / 0.52f;

        if (fade < 0.0f)
            fade = 0.0f;
        cl *= fade;
    }

    return cl;
}

float flight_lift_force(float airspeed, float aoa, float flap, float spoiler)
{
    float cl = lift_coefficient(aoa, flap);
    float q = 0.5f * AIR_DENSITY * airspeed * airspeed * WING_AREA_M2;

    cl *= (1.0f - SPOILER_LIFT_LOSS * spoiler);
    return cl * q;
}

float flight_drag_force(float airspeed, float aoa, float flap, float spoiler)
{
    return flight_drag_force_full(airspeed, aoa, flap, spoiler, 0);
}

float flight_drag_force_full(float airspeed, float aoa, float flap,
                             float spoiler, int gear_down)
{
    float cl = lift_coefficient(aoa, flap);
    float cd = CD_BASE + CD_INDUCED * cl * cl
               + CD_FLAP * flap + CD_SPOILER * spoiler
               + (gear_down ? GEAR_DRAG : 0.0f);
    float q = 0.5f * AIR_DENSITY * airspeed * airspeed * WING_AREA_M2;

    return cd * q;
}

/* Moves a body coordinate into the world: roll (Z) first, then pitch (X),
 * then yaw (Y).
 *
 * Yaw must follow the same convention as the nose direction: local -Z
 * maps to (sin yaw, 0, -cos yaw) in the world - exactly like forward_vec.
 * With the X component written backwards the model faced a different way
 * from where the aircraft was actually going, and sat crooked on the
 * runway. */
void flight_body_to_world(const Flight *f, const float in[3], float out[3])
{
    float cr = cosf(f->roll),  sr = sinf(f->roll);
    float cp = cosf(f->pitch), sp = sinf(f->pitch);
    float cy = cosf(f->yaw),   sy = sinf(f->yaw);
    float x1, y1, z1, x2, y2, z2;

    /* Positive roll is a bank to the RIGHT: the right wing goes down. The sign
     * was reversed in the first version; the model banked left while the
     * physics (yaw += sin(roll)) turned right, so the controls looked
     * inverted. */
    x1 = in[0] * cr + in[1] * sr;
    y1 = -in[0] * sr + in[1] * cr;
    z1 = in[2];

    y2 = y1 * cp - z1 * sp;
    z2 = y1 * sp + z1 * cp;
    x2 = x1;

    out[0] = f->pos[0] + (x2 * cy - z2 * sy);
    out[1] = f->pos[1] + y2;
    out[2] = f->pos[2] + (x2 * sy + z2 * cy);
}

/* Attitude matrix (3x3, row-major): flight_body_to_world without the
 * translation. Split out so that drawing a model with many vertices
 * computes the trigonometry once per frame, not once per vertex.
 *
 * The coefficients were derived by expanding flight_body_to_world; a test
 * checks that both give exactly the same result. */
void flight_orientation_matrix(const Flight *f, float m[3][3])
{
    float cr = cosf(f->roll),  sr = sinf(f->roll);
    float cp = cosf(f->pitch), sp = sinf(f->pitch);
    float cy = cosf(f->yaw),   sy = sinf(f->yaw);

    m[0][0] =  cr * cy + sr * sp * sy;
    m[0][1] =  sr * cy - cr * sp * sy;
    m[0][2] = -cp * sy;

    m[1][0] = -sr * cp;
    m[1][1] =  cr * cp;
    m[1][2] = -sp;

    m[2][0] =  cr * sy - sr * sp * cy;
    m[2][1] =  sr * sy + cr * sp * cy;
    m[2][2] =  cp * cy;
}

/* Is the aircraft over the runway rectangle?
 *
 * dx, dz are the world offset from the runway centre. They are rotated by
 * the runway heading first and split into length and width axes. Pure
 * function: unit tested. */
int flight_over_runway(const Flight *f, float dx, float dz)
{
    float ch = cosf(f->ground_heading);
    float sh = sinf(f->ground_heading);

    /* components along the runway axis (nose direction) and across it */
    float along = dx * sh - dz * ch;
    float across = dx * ch + dz * sh;

    if (along < 0.0f) along = -along;
    if (across < 0.0f) across = -across;

    return (along <= f->ground_half_len && across <= f->ground_half_wid);
}

void flight_init_on_runway(Flight *f, const float runway_xz[2], float heading,
                           float start_offset)
{
    memset(f, 0, sizeof(*f));

    /* At the START of the runway, standing on its wheels. It used to be placed
     * at the runway centre, leaving only half the runway for the takeoff run. */
    f->pos[0] = runway_xz[0] - sinf(heading) * start_offset;
    f->pos[1] = DECK_Y + GEAR_HEIGHT;
    f->pos[2] = runway_xz[1] + cosf(heading) * start_offset;

    f->yaw = heading;
    f->throttle = 0.0f;         /* engine at idle; the player makes the takeoff */
    f->fuel_kg = FUEL_FULL_KG;
    f->mass_kg = EMPTY_MASS_KG + FUEL_FULL_KG;

    f->gear_down = 1;
    f->gear_pos = 1.0f;
    f->flap = 0.34f;            /* one notch of flap for takeoff */
    f->flap_target = 0.34f;
    f->on_ground = 1;
    f->airborne = 0;

    f->ground_ref[0] = runway_xz[0];
    f->ground_ref[1] = DECK_Y;
    f->ground_ref[2] = runway_xz[1];
    f->ground_heading = heading;
    f->ground_half_len = RUNWAY_HALF_LEN;
    f->ground_half_wid = RUNWAY_HALF_WID;
}

void flight_init(Flight *f, const float start_pos[3], float start_yaw)
{
    memset(f, 0, sizeof(*f));

    f->pos[0] = start_pos[0];
    f->pos[1] = start_pos[1];
    f->pos[2] = start_pos[2];

    f->yaw = start_yaw;
    f->throttle = 0.35f;
    f->fuel_kg = FUEL_FULL_KG;
    f->mass_kg = EMPTY_MASS_KG + FUEL_FULL_KG;
    f->gear_down = 0;
    f->gear_pos = 0.0f;
    f->airborne = 1;

    /* starts in the air: some speed along the nose */
    {
        float fwd[3];
        forward_vec(f, fwd);
        /* start near 737 cruise speed; 70 m/s is below the stall for this airframe */
        f->vel[0] = fwd[0] * 190.0f;
        f->vel[1] = fwd[1] * 190.0f;
        f->vel[2] = fwd[2] * 190.0f;
    }
}

void flight_update(Flight *f, float dt)
{
    float fwd[3], up[3];
    float speed, lift, drag, thrust;
    float speed_for_turn;
    float acc[3];
    int i;

    if (dt <= 0.0f)
        return;

    speed_for_turn = len3(f->vel);

    /* --- control inputs change the attitude ---
     *
     * The input sets a target ANGULAR RATE, not the angle; the actual rate
     * approaches the target with an inertia time constant. That is what makes
     * the aircraft feel heavy: it does not respond instantly, and it does not
     * stop rotating the moment the input is released.
     *
     * Authority also scales with dynamic pressure (speed squared): an aircraft
     * standing on the ground does not respond, and controls are heavy in slow
     * flight. */
    {
        float v = len3(f->vel);
        float q = (v * v) / (CONTROL_REF_MS * CONTROL_REF_MS);
        float kp, kr, ky;

        if (q > 1.35f)
            q = 1.35f;

        kp = dt / PITCH_INERTIA_S;
        kr = dt / ROLL_INERTIA_S;
        ky = dt / YAW_INERTIA_S;
        if (kp > 1.0f) kp = 1.0f;
        if (kr > 1.0f) kr = 1.0f;
        if (ky > 1.0f) ky = 1.0f;

        f->p_rate += (f->in_pitch * PITCH_RATE * q - f->p_rate) * kp;
        f->r_rate += (f->in_roll  * ROLL_RATE  * q - f->r_rate) * kr;
        f->y_rate += (f->in_yaw   * YAW_RATE   * q - f->y_rate) * ky;

        /* Convert body angular rates to Euler angle rates.
         *
         * p_rate/q_rate/r_rate are defined in BODY axes; adding them straight to
         * the Euler angles is only correct in level flight. In a bank, a pitch
         * input TURNS the aircraft (yaw), because the body pitch axis is no longer
         * level with the world. Without this conversion the heavy airframe dropped
         * its nose in every bank and stalled; the light jet had so much control
         * authority that the symptom never showed. */
        {
            float sr = sinf(f->roll), cr = cosf(f->roll);
            float cp2 = cosf(f->pitch);
            float tp = sinf(f->pitch) / (cp2 > 0.15f ? cp2 :
                                         (cp2 < -0.15f ? cp2 : 0.15f));
            float cpd = (cp2 > 0.15f) ? cp2 : 0.15f;

            float coupled = f->p_rate * sr + f->y_rate * cr;

            f->roll  += (f->r_rate + coupled * tp) * dt;
            f->pitch += (f->p_rate * cr - f->y_rate * sr) * dt;
            f->yaw   += (coupled / cpd) * dt;
        }
    }

    /* Coordinated turn: in a bank the horizontal component of lift turns the
     * aircraft. This is SEPARATE from the control-driven Euler conversion - it
     * keeps the turn going even with the controls left alone. */
    if (speed_for_turn > 1.0f) {
        float turn = (GRAVITY * tanf(clampf(f->roll, -1.2f, 1.2f)))
                     / speed_for_turn;

        f->yaw += turn * dt;
    }

    f->pitch = clampf(f->pitch, -1.30f, 1.30f);

    while (f->roll >  3.14159265f) f->roll -= 6.28318531f;
    while (f->roll < -3.14159265f) f->roll += 6.28318531f;
    while (f->yaw  >  3.14159265f) f->yaw  -= 6.28318531f;
    while (f->yaw  < -3.14159265f) f->yaw  += 6.28318531f;

    forward_vec(f, fwd);
    up_vec(f, up);

    speed = len3(f->vel);
    f->airspeed = speed;

    /* --- angle of attack ---
     *
     * Taking the TOTAL angle between the velocity and the nose was wrong:
     * sideslip got mixed into the angle of attack, so every turn produced a
     * false STALL warning. The right way is to rotate the velocity into body
     * axes and use only the VERTICAL component. */
    if (speed > 1.0f) {
        float m[3][3];
        float vbx, vby, vbz;

        flight_orientation_matrix(f, m);

        /* v_body = M^T * v_world */
        vbx = m[0][0] * f->vel[0] + m[1][0] * f->vel[1] + m[2][0] * f->vel[2];
        vby = m[0][1] * f->vel[0] + m[1][1] * f->vel[1] + m[2][1] * f->vel[2];
        vbz = m[0][2] * f->vel[0] + m[1][2] * f->vel[1] + m[2][2] * f->vel[2];

        (void)vbx;                  /* sideslip does not affect lift */
        f->aoa = atan2f(-vby, -vbz);
    } else {
        f->aoa = 0.0f;
    }

    /* Stall warning: raised not the instant the critical angle is crossed but
     * after staying above it for a while. In a manoeuvre the angle may touch
     * the limit briefly; sounding the horn on every touch made the warning
     * meaningless. */
    {
        float crit = STALL_ANGLE_RAD + f->flap * 0.06f;

        if (f->aoa > crit)
            f->stall_timer += dt;
        else
            f->stall_timer -= dt * 2.0f;    /* clears faster */

        f->stall_timer = clampf(f->stall_timer, 0.0f, 1.0f);
        f->stalled = (f->stall_timer > 0.28f);
    }

    /* Static stability and trim.
     *
     * The aircraft turns its angle of attack not towards ZERO but towards the
     * TRIM angle that carries its weight. Pulling it to zero was wrong: at
     * zero angle of attack the wing makes no lift, the aircraft sinks, the
     * angle grows and it eventually flipped over. On a real aircraft the tail
     * and trim set this balance; the result is that the aircraft flies level
     * by itself when the controls are released. */
    if (speed > 12.0f) {
        float q = 0.5f * AIR_DENSITY * speed * speed * WING_AREA_M2;
        float cl_needed;
        float trim_aoa;
        float k = PITCH_STABILITY * dt;
        float damp = speed / CONTROL_REF_MS;

        /* In a bank the VERTICAL component of lift shrinks by cos(roll); to hold
         * altitude the aircraft must trim to a higher angle of attack. Without
         * this term the heavy airframe dropped its nose in every bank and stalled -
         * at light-jet mass it went unnoticed because control authority was far
         * higher than the mass. */
        {
            float cr = cosf(f->roll);
            float lift_factor = (cr > 0.20f) ? cr : 0.20f;

            cl_needed = (f->mass_kg * GRAVITY)
                        / ((q > 1.0f ? q : 1.0f) * lift_factor);
        }

        trim_aoa = (cl_needed - f->flap * CL_FLAP) / CL_SLOPE;
        trim_aoa = clampf(trim_aoa, -0.05f, STALL_ANGLE_RAD * 0.85f);

        if (damp > 1.2f)
            damp = 1.2f;
        f->pitch -= (f->aoa - trim_aoa) * k * damp;
    }

    /* --- forces --- */
    thrust = f->throttle * MAX_THRUST_N;
    if (f->fuel_kg <= 0.0f)
        thrust = 0.0f;                      /* out of fuel: gliding */

    lift = flight_lift_force(speed, f->aoa, f->flap, f->spoiler);

    /* Wing loading: in a hard turn lift becomes a multiple of the weight.
     * Past the structural limit lift is clipped - the aircraft "won't pull". */
    f->g_load = lift / (f->mass_kg * GRAVITY);

    /* The limit applies in both directions. Only positive G used to be
     * clipped; negative lift of up to -30 G was left free and flung the
     * aircraft. */
    if (f->g_load > G_LIMIT) {
        lift *= G_LIMIT / f->g_load;
        f->g_load = G_LIMIT;
    } else if (f->g_load < -G_NEG_LIMIT) {
        lift *= -G_NEG_LIMIT / f->g_load;
        f->g_load = -G_NEG_LIMIT;
    }
    drag = flight_drag_force_full(speed, f->aoa, f->flap, f->spoiler,
                                  f->gear_down);

    f->mass_kg = EMPTY_MASS_KG + (f->fuel_kg > 0.0f ? f->fuel_kg : 0.0f);

    for (i = 0; i < 3; i++)
        acc[i] = 0.0f;

    /* thrust: along the nose */
    for (i = 0; i < 3; i++)
        acc[i] += fwd[i] * thrust / f->mass_kg;

    /* lift: perpendicular to the wing plane (up direction) */
    for (i = 0; i < 3; i++)
        acc[i] += up[i] * lift / f->mass_kg;

    /* drag: opposite to the motion */
    if (speed > 0.1f) {
        for (i = 0; i < 3; i++)
            acc[i] -= (f->vel[i] / speed) * drag / f->mass_kg;
    }

    /* weight */
    acc[1] -= GRAVITY;

    for (i = 0; i < 3; i++) {
        f->vel[i] += acc[i] * dt;
        f->pos[i] += f->vel[i] * dt;
    }

    /* --- control surface animation ---
     * Surfaces move towards their target at a fixed rate; no jumps. */
    {
        float d = f->flap_target - f->flap;
        float step = FLAP_RATE * dt;

        if (d > step)       f->flap += step;
        else if (d < -step) f->flap -= step;
        else                f->flap = f->flap_target;

        d = f->spoiler_target - f->spoiler;
        step = SPOILER_RATE * dt;

        if (d > step)       f->spoiler += step;
        else if (d < -step) f->spoiler -= step;
        else                f->spoiler = f->spoiler_target;

        f->flap = clampf(f->flap, 0.0f, 1.0f);
        f->spoiler = clampf(f->spoiler, 0.0f, 1.0f);
    }

    /* --- wheel animation --- */
    {
        float target = f->gear_down ? 1.0f : 0.0f;
        float step = dt * 1.2f;

        if (f->gear_pos < target) {
            f->gear_pos += step;
            if (f->gear_pos > target) f->gear_pos = target;
        } else if (f->gear_pos > target) {
            f->gear_pos -= step;
            if (f->gear_pos < target) f->gear_pos = target;
        }
    }

    /* --- contact with the runway surface ---
     * Over the runway area and low enough, the wheels touch down: vertical
     * speed is cut and friction slows the horizontal speed. The nose can only
     * lift above a certain speed (rotate speed), which keeps the takeoff
     * realistic. */
    {
        float deck_top = DECK_Y + (f->gear_down ? GEAR_HEIGHT : 0.4f);
        float dx = f->pos[0] - f->ground_ref[0];
        float dz = f->pos[2] - f->ground_ref[2];
        int over_runway = flight_over_runway(f, dx, dz);

        f->on_ground = 0;

        if (over_runway && f->pos[1] <= deck_top) {
            float horiz;

            f->pos[1] = deck_top;
            if (f->vel[1] < 0.0f)
                f->vel[1] = 0.0f;
            f->on_ground = 1;

            /* on the ground the nose does not go down and the bank levels out */
            if (f->pitch < 0.0f)
                f->pitch = 0.0f;
            f->roll -= f->roll * (dt * 4.0f);

            /* without enough speed the nose stays down */
            horiz = sqrtf(f->vel[0] * f->vel[0] + f->vel[2] * f->vel[2]);
            if (horiz < ROTATE_SPEED_MS && f->pitch > 0.06f)
                f->pitch = 0.06f;

            /* Wheel friction and brakes.
             * Friction produces a constant deceleration (mu * g); written as a
             * multiplier it would be proportional to speed and the aircraft would
             * never reach takeoff speed - the first version had exactly this bug. */
            {
                float mu = ROLL_FRICTION
                           + ((f->brakes || f->spoiler > 0.5f) ? BRAKE_FRICTION : 0.0f);
                float decel = mu * GRAVITY * dt;
                float horiz = sqrtf(f->vel[0] * f->vel[0] + f->vel[2] * f->vel[2]);

                if (horiz > 0.001f) {
                    float nh = horiz - decel;

                    if (nh < 0.0f)
                        nh = 0.0f;
                    f->vel[0] *= nh / horiz;
                    f->vel[2] *= nh / horiz;
                }
            }
        } else if (f->pos[1] < WATER_SAFE_ALT) {
            /* Sea surface: cannot go below it. Water resists far more than wheels -
             * the aircraft cannot slide freely here. */
            float horiz = sqrtf(f->vel[0] * f->vel[0] + f->vel[2] * f->vel[2]);

            f->pos[1] = WATER_SAFE_ALT;
            if (f->vel[1] < 0.0f)
                f->vel[1] = 0.0f;
            f->on_ground = 1;

            if (horiz > 0.001f) {
                float nh = horiz - WATER_DRAG * GRAVITY * dt;

                if (nh < 0.0f)
                    nh = 0.0f;
                f->vel[0] *= nh / horiz;
                f->vel[2] *= nh / horiz;
            }
        }

        if (!f->on_ground && f->pos[1] > DECK_Y + 6.0f)
            f->airborne = 1;
    }

    /* --- wheel rotation (for the animation) ---
     * On the ground the wheels spin with speed; in the air they slow down. */
    {
        float horiz = sqrtf(f->vel[0] * f->vel[0] + f->vel[2] * f->vel[2]);

        if (f->on_ground)
            f->wheel_spin += (horiz / WHEEL_RADIUS_M) * dt;
        else
            f->wheel_spin += (horiz / WHEEL_RADIUS_M) * dt * 0.35f;

        while (f->wheel_spin > 6.28318531f)
            f->wheel_spin -= 6.28318531f;
    }

    /* --- fuel --- */
    f->fuel_kg -= f->throttle * FUEL_BURN_KGS * dt;
    if (f->fuel_kg < 0.0f)
        f->fuel_kg = 0.0f;
}

float flight_speed_kmh(const Flight *f)
{
    return f->airspeed * 3.6f;
}

float flight_altitude(const Flight *f)
{
    return f->pos[1];
}

float flight_fuel_pct(const Flight *f)
{
    return f->fuel_kg / FUEL_FULL_KG * 100.0f;
}
