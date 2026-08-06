#include <math.h>
#include <string.h>

#include "flight.h"

#define AIR_DENSITY      1.225f
#define CL_SLOPE         5.2f    /* hucum acisi basina tasima katsayisi */
#define CL_FLAP          0.85f   /* tam flap'in ekledigi tasima */
#define CD_BASE          0.028f
#define CD_INDUCED       0.055f
#define CD_FLAP          0.10f
#define CD_SPOILER       0.16f
#define SPOILER_LIFT_LOSS 0.55f  /* tam spoiler tasimanin bu oranini kirar */

#define PITCH_RATE       1.25f   /* radyan/saniye, tam girdide */
#define ROLL_RATE        2.10f
#define YAW_RATE         0.60f
#define FUEL_BURN_KGS    0.55f   /* tam gazda saniyede yanan yakit */

static float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static float len3(const float v[3])
{
    return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

/* Burun yonu (yaw/pitch'ten). Roll burun yonunu degistirmez. */
static void forward_vec(const Flight *f, float out[3])
{
    float cp = cosf(f->pitch);

    out[0] =  cp * sinf(f->yaw);
    out[1] =  sinf(f->pitch);
    out[2] = -cp * cosf(f->yaw);
}

/* Yukari yon: roll ile yatar */
static void up_vec(const Flight *f, float out[3])
{
    float cr = cosf(f->roll), sr = sinf(f->roll);
    float cy = cosf(f->yaw),  sy = sinf(f->yaw);
    float sp = sinf(f->pitch), cp = cosf(f->pitch);

    out[0] = -sp * sy * cr - cy * sr;
    out[1] =  cp * cr;
    out[2] =  sp * cy * cr - sy * sr;
}

/* Tasima katsayisi: hucum acisiyla dogrusal artar, stall acisindan sonra
 * hizla coker. Flap hem katsayiyi hem kritik aciyi artirir. */
static float lift_coefficient(float aoa, float flap)
{
    float stall = STALL_ANGLE_RAD + flap * 0.06f;
    float cl = aoa * CL_SLOPE + flap * CL_FLAP;
    float over;

    if (aoa <= stall)
        return cl;

    /* stall sonrasi: tasima keskin dusus */
    over = (aoa - stall) / 0.20f;
    if (over > 1.0f)
        over = 1.0f;
    return cl * (1.0f - 0.85f * over);
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
    float cl = lift_coefficient(aoa, flap);
    float cd = CD_BASE + CD_INDUCED * cl * cl
               + CD_FLAP * flap + CD_SPOILER * spoiler;
    float q = 0.5f * AIR_DENSITY * airspeed * airspeed * WING_AREA_M2;

    return cd * q;
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

    /* havada baslar: burun yonunde bir miktar hiz */
    {
        float fwd[3];
        forward_vec(f, fwd);
        f->vel[0] = fwd[0] * 70.0f;
        f->vel[1] = fwd[1] * 70.0f;
        f->vel[2] = fwd[2] * 70.0f;
    }
}

void flight_update(Flight *f, float dt)
{
    float fwd[3], up[3];
    float speed, lift, drag, thrust;
    float acc[3];
    int i;

    if (dt <= 0.0f)
        return;

    /* --- kumanda girdileri yonelimi degistirir --- */
    f->pitch += f->in_pitch * PITCH_RATE * dt;
    f->roll  += f->in_roll  * ROLL_RATE  * dt;
    f->yaw   += f->in_yaw   * YAW_RATE   * dt;

    /* yatis burnu cevirir (koordineli donus) */
    f->yaw += sinf(f->roll) * 0.55f * dt;

    f->pitch = clampf(f->pitch, -1.30f, 1.30f);

    while (f->roll >  3.14159265f) f->roll -= 6.28318531f;
    while (f->roll < -3.14159265f) f->roll += 6.28318531f;
    while (f->yaw  >  3.14159265f) f->yaw  -= 6.28318531f;
    while (f->yaw  < -3.14159265f) f->yaw  += 6.28318531f;

    forward_vec(f, fwd);
    up_vec(f, up);

    speed = len3(f->vel);
    f->airspeed = speed;

    /* --- hucum acisi: burun yonu ile gercek hareket yonu arasindaki fark --- */
    if (speed > 1.0f) {
        float dot = (f->vel[0] * fwd[0] + f->vel[1] * fwd[1] + f->vel[2] * fwd[2]) / speed;
        f->aoa = acosf(clampf(dot, -1.0f, 1.0f));
        /* isaret: hareket yonu burnun altindaysa pozitif hucum acisi */
        if (f->vel[1] / speed > fwd[1])
            f->aoa = -f->aoa;
    } else {
        f->aoa = 0.0f;
    }

    f->stalled = (f->aoa > STALL_ANGLE_RAD + f->flap * 0.06f);

    /* --- kuvvetler --- */
    thrust = f->throttle * MAX_THRUST_N;
    if (f->fuel_kg <= 0.0f)
        thrust = 0.0f;                      /* yakit bitti: suzulme */

    lift = flight_lift_force(speed, f->aoa, f->flap, f->spoiler);
    drag = flight_drag_force(speed, f->aoa, f->flap, f->spoiler);

    f->mass_kg = EMPTY_MASS_KG + (f->fuel_kg > 0.0f ? f->fuel_kg : 0.0f);

    for (i = 0; i < 3; i++)
        acc[i] = 0.0f;

    /* itki: burun yonunde */
    for (i = 0; i < 3; i++)
        acc[i] += fwd[i] * thrust / f->mass_kg;

    /* tasima: kanat duzlemine dik (yukari yon) */
    for (i = 0; i < 3; i++)
        acc[i] += up[i] * lift / f->mass_kg;

    /* surukleme: hareketin tersine */
    if (speed > 0.1f) {
        for (i = 0; i < 3; i++)
            acc[i] -= (f->vel[i] / speed) * drag / f->mass_kg;
    }

    /* agirlik */
    acc[1] -= GRAVITY;

    for (i = 0; i < 3; i++) {
        f->vel[i] += acc[i] * dt;
        f->pos[i] += f->vel[i] * dt;
    }

    /* --- deniz yuzeyi: altina inilemez --- */
    f->on_ground = 0;
    if (f->pos[1] < WATER_SAFE_ALT) {
        f->pos[1] = WATER_SAFE_ALT;
        if (f->vel[1] < 0.0f)
            f->vel[1] = 0.0f;
        f->on_ground = 1;
    }

    /* --- yakit --- */
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
