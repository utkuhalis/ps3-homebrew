#ifndef FLIGHT_H
#define FLIGHT_H

/* Flight model: thrust, lift, drag, weight and stall.
 *
 * Hardware independent (unit tested). Not real aerodynamics but a model
 * simplified for a game; the goal is for it to feel like an aircraft. */

/* Boeing 737-800 values. The model is full size (39.4 m long, 34.8 m
 * span), so the physics must match it; with the 4.7 t business jet's
 * constants a 70 t airframe flew like a toy. */
#define EMPTY_MASS_KG   41000.0f    /* operating empty weight */
#define FUEL_FULL_KG    18000.0f
/* Thrust: T/W ~ 0.45 for a 4700 kg business jet. The first value (42 kN)
 * gave 0.91 - fighter territory; the aircraft shot off like a toy. */
#define MAX_THRUST_N   235000.0f    /* 2 x 117 kN */
#define WING_AREA_M2      125.0f
#define GRAVITY             9.81f

#define STALL_ANGLE_RAD     0.28f   /* ~16 degrees */
#define WATER_SAFE_ALT       2.0f   /* stays above sea level */

/* Runway surface and ground contact */
#define DECK_Y               9.0f   /* surface of the runway platform */
/* The model origin is at the bottom of the wheels (measured: lowest point
 * Y = 0.01), so the body needs no extra lift. At 3.9 the aircraft hung
 * four metres above the runway. */
#define GEAR_HEIGHT          0.05f   /* body height with the gear down */
#define ROLL_FRICTION        0.030f /* wheel friction */
#define BRAKE_FRICTION       0.240f /* brakes (together with the spoilers) */
#define ROTATE_SPEED_MS     75.0f   /* above this speed the nose can lift */

/* Angular inertia: a control input produces a MOMENT and the aircraft
 * accelerates into it. The input used to change the angle directly; the
 * aircraft turned instantly like paper and stopped the moment the input
 * was released. These time constants set how heavy the aircraft feels:
 * the larger, the heavier. */
#define PITCH_INERTIA_S   2.30f
#define ROLL_INERTIA_S    1.15f
#define YAW_INERTIA_S     2.80f

/* Lift grows during a manoeuvre and so does induced drag: the aircraft
 * loses speed in a hard turn. */
#define G_LIMIT           2.5f    /* airliner structural limit */
#define G_NEG_LIMIT       1.0f   /* the structural limit is lower in the negative direction */
#define WATER_DRAG        1.35f  /* friction coefficient on the water surface */

/* Control authority grows with dynamic pressure: below this speed the
 * controls get heavy, above it they are fully effective. As on a real
 * aircraft, a stationary one cannot change direction. */
#define CONTROL_REF_MS     125.0f
#define GEAR_DRAG            0.055f
/* Surface travel rates (units/second). On the 737 full flap takes ~15 s,
 * the speed brake ~2 s. */
#define FLAP_RATE            0.09f
#define SPOILER_RATE         0.55f

#define WHEEL_RADIUS_M       0.55f  /* wheel radius (spin animation) */

/* Runway surface dimensions (must match runway.h). Defined again here so
 * the flight module does not depend on the drawing module; a test checks
 * that the two stay the same. */
#define RUNWAY_HALF_LEN   1300.0f
#define RUNWAY_HALF_WID     30.0f /* drag contribution of the extended gear */

/* Control surface limits (radians) */
#define FLAP_MAX_RAD     0.70f      /* ~40 degrees */
#define SPOILER_MAX_RAD  0.96f      /* ~55 degrees */
#define AILERON_MAX_RAD  0.35f
#define ELEVATOR_MAX_RAD 0.44f
#define RUDDER_MAX_RAD   0.44f

typedef struct {
    float pos[3];
    float vel[3];

    float yaw, pitch, roll;     /* radians */

    float throttle;             /* 0..1 */
    float fuel_kg;

    /* control inputs, -1..+1 (flap and spoiler 0..1) */
    float in_pitch, in_roll, in_yaw;
    /* Control surfaces: TARGET and ACTUAL position are separate. The pilot
     * puts the lever in a detent and the surfaces follow at motor speed - on a
     * real aircraft full flap takes seconds. */
    float flap, spoiler;                /* actual position 0..1 */
    float flap_target, spoiler_target;  /* detent the lever is in */
    int   gear_down;            /* 1: gear down */
    float gear_pos;             /* 0..1 animation position */
    int   brakes;               /* wheel brake on the ground */

    /* angular rates (radians/second) - inertia works through these */
    float p_rate, r_rate, y_rate;

    /* last computed values - for the HUD and audio */
    float airspeed;             /* m/s */
    float aoa;                  /* angle of attack, radians */
    int   stalled;
    float stall_timer;          /* how long it has been above the critical angle */
    float mass_kg;
    float g_load;               /* wing loading, 1.0 = level flight */
    float wheel_spin;           /* wheel rotation angle (radians) */
    int   on_ground;            /* on the runway surface */
    int   airborne;             /* has it been airborne yet (takeoff done) */

    /* The RUNWAY RECTANGLE where ground contact applies.
     * A circle with a radius was used before; the runway being 1100x96 units,
     * the aircraft could touch down on the sea beside it as if on asphalt. */
    float ground_ref[3];        /* runway centre */
    float ground_heading;       /* runway heading (radians) */
    float ground_half_len;      /* half length */
    float ground_half_wid;      /* half width */
} Flight;

/* Starts in the air (free flight) */
void  flight_init(Flight *f, const float start_pos[3], float start_yaw);

/* Starts on the runway, engine at idle, gear down.
 * The player makes the takeoff: throttle up, build speed, lift the nose
 * above ROTATE_SPEED_MS. */
/* start_offset: shift back from the runway centre (towards the tail) so
 * the takeoff run starts at the runway threshold. */
void  flight_init_on_runway(Flight *f, const float runway_xz[2], float heading,
                            float start_offset);

float flight_drag_force_full(float airspeed, float aoa, float flap,
                             float spoiler, int gear_down);
void  flight_update(Flight *f, float dt);

/* Runway rectangle test (dx, dz relative to the runway centre) */
int   flight_over_runway(const Flight *f, float dx, float dz);

/* Moves a body coordinate into the world (model drawing and camera placement) */
void  flight_body_to_world(const Flight *f, const float in[3], float out[3]);

/* Attitude matrix: body_to_world without the translation */
void  flight_orientation_matrix(const Flight *f, float m[3][3]);

/* Derived values */
float flight_speed_kmh(const Flight *f);
float flight_altitude(const Flight *f);
float flight_fuel_pct(const Flight *f);

/* Lift produced in a given state (exposed for tests and tuning) */
float flight_lift_force(float airspeed, float aoa, float flap, float spoiler);
float flight_drag_force(float airspeed, float aoa, float flap, float spoiler);

#endif
