/* Host-side unit tests for the flight model (no PS3 needed). */
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "../source/flight.h"

static int failures = 0, checks = 0;
#define CHECK(cond, msg) do { checks++; if (!(cond)) { \
    printf("  FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); failures++; } } while (0)

static void test_tasima_hizla_artiyor(void)
{
    float l1, l2;

    printf("test: lift grows with the square of speed\n");
    l1 = flight_lift_force(50.0f, 0.08f, 0.0f, 0.0f);
    l2 = flight_lift_force(100.0f, 0.08f, 0.0f, 0.0f);

    CHECK(l2 > l1 * 3.5f && l2 < l1 * 4.5f, "double speed -> ~four times the lift");
}

static void test_stall(void)
{
    float normal, asiri;

    printf("test: lift collapses past the critical angle (stall)\n");
    normal = flight_lift_force(80.0f, STALL_ANGLE_RAD - 0.02f, 0.0f, 0.0f);
    asiri  = flight_lift_force(80.0f, STALL_ANGLE_RAD + 0.25f, 0.0f, 0.0f);

    CHECK(asiri < normal * 0.6f, "lift dropped clearly after the stall");
}

static void test_flap_kritik_aciyi_artiriyor(void)
{
    float aoa = STALL_ANGLE_RAD + 0.03f;
    float flapsiz, flapli;

    printf("test: flaps raise the critical angle\n");
    flapsiz = flight_lift_force(70.0f, aoa, 0.0f, 0.0f);
    flapli  = flight_lift_force(70.0f, aoa, 1.0f, 0.0f);

    CHECK(flapli > flapsiz, "flaps give more lift at the same angle");
}

static void test_flap_dusuk_hizda_ucmayi_sagliyor(void)
{
    float agirlik = (EMPTY_MASS_KG + FUEL_FULL_KG) * GRAVITY;
    float hiz;
    float flapsiz_min = 0.0f, flapli_min = 0.0f;

    printf("test: with flaps the aircraft stays airborne at a lower speed\n");

    for (hiz = 20.0f; hiz < 200.0f; hiz += 0.5f) {
        if (flapsiz_min == 0.0f &&
            flight_lift_force(hiz, 0.20f, 0.0f, 0.0f) >= agirlik)
            flapsiz_min = hiz;
        if (flapli_min == 0.0f &&
            flight_lift_force(hiz, 0.20f, 1.0f, 0.0f) >= agirlik)
            flapli_min = hiz;
    }

    CHECK(flapsiz_min > 0.0f && flapli_min > 0.0f, "flies in both cases");
    CHECK(flapli_min < flapsiz_min, "flaps lowered the minimum flying speed");
}

static void test_spoiler_frenliyor(void)
{
    float d0, d1, l0, l1;

    printf("test: spoilers cut lift and add drag\n");
    l0 = flight_lift_force(90.0f, 0.10f, 0.0f, 0.0f);
    l1 = flight_lift_force(90.0f, 0.10f, 0.0f, 1.0f);
    d0 = flight_drag_force(90.0f, 0.10f, 0.0f, 0.0f);
    d1 = flight_drag_force(90.0f, 0.10f, 0.0f, 1.0f);

    CHECK(l1 < l0 * 0.6f, "lift dropped clearly");
    CHECK(d1 > d0 * 1.5f, "drag rose clearly");
}

static void test_yakit_ve_agirlik(void)
{
    Flight f;
    float start[3] = { 0.0f, 500.0f, 0.0f };
    float ilk_agirlik;
    int i;

    printf("test: fuel burns, weight drops, then thrust cuts out\n");
    flight_init(&f, start, 0.0f);
    ilk_agirlik = f.mass_kg;
    f.throttle = 1.0f;

    for (i = 0; i < 12000; i++)
        flight_update(&f, 1.0f / 60.0f);

    CHECK(f.fuel_kg < FUEL_FULL_KG, "fuel went down");
    CHECK(f.mass_kg < ilk_agirlik, "weight went down");

    /* burn all the fuel */
    for (i = 0; i < 900000 && f.fuel_kg > 0.0f; i++)
        flight_update(&f, 1.0f / 60.0f);

    CHECK(f.fuel_kg == 0.0f, "out of fuel");
    CHECK(fabsf(f.mass_kg - EMPTY_MASS_KG) < 0.01f, "weight equals empty weight");
}

static void test_denizin_altina_inilemiyor(void)
{
    Flight f;
    float start[3] = { 0.0f, 300.0f, 0.0f };
    int i, batti = 0;

    printf("test: no input can put the aircraft below the sea\n");
    flight_init(&f, start, 0.0f);
    f.throttle = 1.0f;
    f.in_pitch = -1.0f;             /* nose down, full throttle */

    for (i = 0; i < 20000; i++) {
        flight_update(&f, 1.0f / 60.0f);
        if (f.pos[1] < WATER_SAFE_ALT - 0.01f)
            batti = 1;
    }

    CHECK(!batti, "never went below sea level in any frame");
}

static void test_gaz_hizlandiriyor(void)
{
    Flight a, b;
    float start[3] = { 0.0f, 800.0f, 0.0f };
    int i;

    printf("test: the aircraft accelerates with throttle\n");
    flight_init(&a, start, 0.0f);
    flight_init(&b, start, 0.0f);
    a.throttle = 0.0f;
    b.throttle = 1.0f;

    for (i = 0; i < 300; i++) {
        flight_update(&a, 1.0f / 60.0f);
        flight_update(&b, 1.0f / 60.0f);
    }

    CHECK(b.airspeed > a.airspeed, "full throttle gives a higher speed");
}

static void test_yonelim_sinirli(void)
{
    Flight f;
    float start[3] = { 0.0f, 600.0f, 0.0f };
    int i;

    printf("test: pitch is limited, yaw and roll wrap\n");
    flight_init(&f, start, 0.0f);
    f.in_pitch = 1.0f;
    f.in_roll = 1.0f;

    for (i = 0; i < 3000; i++)
        flight_update(&f, 1.0f / 60.0f);

    CHECK(f.pitch <= 1.31f && f.pitch >= -1.31f, "pitch angle limited");
    CHECK(f.roll >= -3.15f && f.roll <= 3.15f, "roll angle wraps");
    CHECK(f.yaw >= -3.15f && f.yaw <= 3.15f, "yaw angle wraps");
}

static void test_pistte_basliyor(void)
{
    Flight f;
    float rw[2] = { -1500.0f, 1200.0f };

    printf("test: starts on the runway, stationary, engine at idle\n");
    flight_init_on_runway(&f, rw, 0.0f, 1150.0f);

    CHECK(f.on_ground, "yerde");
    CHECK(f.airspeed < 0.1f, "duruyor");
    CHECK(f.throttle < 0.01f, "engine at idle");
    CHECK(f.gear_down, "landing gear down");
    CHECK(f.pos[1] > DECK_Y, "over the runway surface");
}

static void test_gaz_vermeden_kalkamaz(void)
{
    Flight f;
    float rw[2] = { 0.0f, 0.0f };
    int i;

    printf("test: without throttle the aircraft stays on the runway\n");
    flight_init_on_runway(&f, rw, 0.0f, 1150.0f);
    f.in_pitch = 1.0f;              /* try to lift the nose */

    for (i = 0; i < 600; i++)
        flight_update(&f, 1.0f / 60.0f);

    CHECK(f.on_ground, "still on the ground");
    CHECK(!f.airborne, "havalanmadi");
}

static void test_kalkis_yapilabiliyor(void)
{
    Flight f;
    float rw[2] = { 0.0f, 0.0f };
    int i;
    float max_speed = 0.0f;

    printf("test: accelerating at full throttle and rotating makes a takeoff\n");
    flight_init_on_runway(&f, rw, 0.0f, 1150.0f);
    f.throttle = 1.0f;

    /* Accelerate first (nose on the ground), then rotate.
     * 30 seconds: with realistic thrust the takeoff run takes ~14 seconds and
     * the climb needs time too. The test used to be 15 seconds and became too
     * short when the thrust was halved. */
    for (i = 0; i < 1800; i++) {
        flight_update(&f, 1.0f / 60.0f);
        if (f.airspeed > max_speed)
            max_speed = f.airspeed;
        /* lift the nose on reaching rotate speed */
        if (f.airspeed > ROTATE_SPEED_MS)
            f.in_pitch = 0.55f;
    }

    CHECK(max_speed > ROTATE_SPEED_MS, "takeoff speed reached");
    CHECK(f.airborne, "aircraft got airborne");
    CHECK(f.pos[1] > DECK_Y + 10.0f, "kept climbing");
}

static void test_gear_surukleme(void)
{
    float acik, kapali;

    printf("test: extended landing gear adds drag\n");
    acik   = flight_drag_force_full(90.0f, 0.08f, 0.0f, 0.0f, 1);
    kapali = flight_drag_force_full(90.0f, 0.08f, 0.0f, 0.0f, 0);

    CHECK(acik > kapali, "drag higher with the gear down");
}

/* NO STALL warning in level flight or in a turn.
 *
 * The angle of attack used to be computed as the total angle between the
 * velocity and the nose; sideslip got mixed into it, so every turn raised
 * a false stall warning. */
static void test_donuste_sahte_stall_yok(void)
{
    Flight f;
    float pos[3];
    int i;
    int stall_frames = 0;
    float max_aoa = 0.0f;

    printf("test: no false stall in level flight or turns\n");

    pos[0] = 0.0f; pos[1] = 3000.0f; pos[2] = 0.0f;
    flight_init(&f, pos, 0.0f);
    f.throttle = 0.80f;

    /* settle in level flight first */
    for (i = 0; i < 60 * 10; i++)
        flight_update(&f, 1.0f / 60.0f);

    /* Then 30 seconds of coordinated turn: the bank is HELD at 26 degrees.
     * A constant aileron input rolls the aircraft over - a real aircraft does
     * that too, so like a pilot we hold the bank steady. */
    for (i = 0; i < 60 * 30; i++) {
        float bank_target = 0.45f;

        f.in_roll  = (bank_target - f.roll) * 6.0f - f.r_rate * 3.0f;
        if (f.in_roll >  1.0f) f.in_roll =  1.0f;
        if (f.in_roll < -1.0f) f.in_roll = -1.0f;

        /* Altitude is held. A constant nose-up input kept the aircraft climbing,
         * bleeding speed until it went steep - a real pilot holds altitude too
         * rather than leaving the input fixed. */
        {
            float alt_err = 3000.0f - f.pos[1];
            float vs = f.vel[1];

            f.in_pitch = alt_err * 0.004f - vs * 0.16f - f.p_rate * 2.6f;
            if (f.in_pitch >  1.0f) f.in_pitch =  1.0f;
            if (f.in_pitch < -1.0f) f.in_pitch = -1.0f;
        }
        flight_update(&f, 1.0f / 60.0f);

        if (f.stalled)
            stall_frames++;
        if (f.aoa > max_aoa)
            max_aoa = f.aoa;
    }

    printf("       highest angle of attack in the turn %.3f rad, stall %d frames\n",
           max_aoa, stall_frames);
    CHECK(stall_frames == 0, "no stall warning in a coordinated turn");
    CHECK(f.airspeed > 120.0f, "aircraft holds its speed in the turn");
    CHECK(f.pos[1] > 2000.0f, "aircraft does not descend in the turn");
}

/* A real stall must still work: pulling the nose too far must warn */
static void test_gercek_stall_calisiyor(void)
{
    Flight f;
    float pos[3];
    int i;
    int stalled_once = 0;

    printf("test: pulling the nose too far gives a real stall\n");

    pos[0] = 0.0f; pos[1] = 4000.0f; pos[2] = 0.0f;
    flight_init(&f, pos, 0.0f);
    f.throttle = 0.0f;              /* throttle closed: speed drops */

    for (i = 0; i < 60 * 40; i++) {
        f.in_pitch = 1.0f;          /* pull the nose all the way back */
        flight_update(&f, 1.0f / 60.0f);
        if (f.stalled)
            stalled_once = 1;
    }

    CHECK(stalled_once, "stall detected at excessive angle of attack");
}

/* The runway collider must be a RECTANGLE: the runway is 1100x96 units, and
 * a circle let the aircraft touch down on the sea beside it as if on asphalt. */
static void test_pist_dikdortgen_collider(void)
{
    Flight f;
    float rw[2] = { 0.0f, 0.0f };

    printf("test: runway collider is a rectangle\n");

    /* runway heading 0: nose at -Z, so the runway runs along the Z axis */
    flight_init_on_runway(&f, rw, 0.0f, 0.0f);

    CHECK(flight_over_runway(&f, 0.0f, 0.0f), "centre is on the runway");
    CHECK(flight_over_runway(&f, 0.0f, -1200.0f), "runway start is on the runway");
    CHECK(flight_over_runway(&f, 0.0f, 1200.0f), "runway end is on the runway");
    CHECK(flight_over_runway(&f, 25.0f, 0.0f), "within the runway width");

    CHECK(!flight_over_runway(&f, 200.0f, 0.0f),
          "200 units beside the runway is OUTSIDE");
    CHECK(!flight_over_runway(&f, 0.0f, 1500.0f),
          "1500 units past the runway is OUTSIDE");
    CHECK(!flight_over_runway(&f, 40.0f, 0.0f),
          "beyond the width limit is OUTSIDE");

    /* rotated runway: heading 90 degrees -> the runway runs along the X axis */
    flight_init_on_runway(&f, rw, 1.5708f, 0.0f);
    CHECK(flight_over_runway(&f, 1200.0f, 0.0f),
          "long axis correct on a rotated runway");
    CHECK(!flight_over_runway(&f, 0.0f, 200.0f),
          "width limit correct on a rotated runway");
}

/* An aircraft coming down on the sea beside the runway must not behave as if on asphalt */
static void test_pist_yaninda_su(void)
{
    Flight f;
    float rw[2] = { 0.0f, 0.0f };
    int i;

    printf("test: coming down beside the runway behaves like water\n");

    flight_init_on_runway(&f, rw, 0.0f, 0.0f);
    f.pos[0] = 300.0f;              /* 300 units beside the runway: sea */
    f.pos[1] = DECK_Y + 20.0f;
    f.vel[1] = -8.0f;

    for (i = 0; i < 600; i++)
        flight_update(&f, 1.0f / 60.0f);

    CHECK(f.pos[1] < DECK_Y, "beside the runway it goes down to sea level");
}

/* Control surfaces must not jump to the target INSTANTLY: the lever is
 * put in a detent and the surfaces follow at motor speed. */
static void test_yuzey_animasyonu(void)
{
    Flight f;
    float rw[2] = { 0.0f, 0.0f };
    int i;
    float onceki;

    printf("test: flaps and speed brake move to the target gradually\n");

    flight_init_on_runway(&f, rw, 0.0f, 1150.0f);

    /* put the flap lever in the fully extended detent */
    f.flap = 0.0f;
    f.flap_target = 1.0f;
    flight_update(&f, 1.0f / 60.0f);
    CHECK(f.flap > 0.0f, "flaps started moving");
    CHECK(f.flap < 0.05f, "flaps do not jump to the target in one frame");

    /* a few seconds should get part of the way, but not finish */
    for (i = 0; i < 60 * 3; i++)
        flight_update(&f, 1.0f / 60.0f);
    CHECK(f.flap > 0.15f && f.flap < 0.95f,
          "flaps part of the way after a few seconds");

    /* after enough time they must settle on the target and STAY there */
    for (i = 0; i < 60 * 20; i++)
        flight_update(&f, 1.0f / 60.0f);
    CHECK(f.flap > 0.99f, "flaps eventually reach the target");
    onceki = f.flap;
    for (i = 0; i < 60; i++)
        flight_update(&f, 1.0f / 60.0f);
    CHECK(f.flap == onceki, "stops at the target, no oscillation");

    /* the speed brake is faster, but still not instant */
    f.spoiler = 0.0f;
    f.spoiler_target = 1.0f;
    flight_update(&f, 1.0f / 60.0f);
    CHECK(f.spoiler > 0.0f && f.spoiler < 0.2f,
          "speed brake also extends gradually");

    for (i = 0; i < 60 * 4; i++)
        flight_update(&f, 1.0f / 60.0f);
    CHECK(f.spoiler > 0.99f, "speed brake fully out within a few seconds");

    /* retracting is gradual as well */
    f.spoiler_target = 0.0f;
    flight_update(&f, 1.0f / 60.0f);
    CHECK(f.spoiler > 0.8f, "retracting is not instant either");
}

int main(void)
{
    test_pistte_basliyor();
    test_gaz_vermeden_kalkamaz();
    test_kalkis_yapilabiliyor();
    test_gear_surukleme();
    test_donuste_sahte_stall_yok();
    test_gercek_stall_calisiyor();
    test_pist_dikdortgen_collider();
    test_pist_yaninda_su();
    test_yuzey_animasyonu();
    test_tasima_hizla_artiyor();
    test_stall();
    test_flap_kritik_aciyi_artiriyor();
    test_flap_dusuk_hizda_ucmayi_sagliyor();
    test_spoiler_frenliyor();
    test_yakit_ve_agirlik();
    test_denizin_altina_inilemiyor();
    test_gaz_hizlandiriyor();
    test_yonelim_sinirli();

    printf("\n%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
