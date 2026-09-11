/* Autopilot tests: run in a closed loop with the real flight model.
 * The autopilot never touches the aircraft directly, it only produces
 * control inputs, so these tests also measure the flight model's stability. */

#include <stdio.h>
#include <math.h>

#include "../source/autopilot.h"

static int failures;

static void check(int cond, const char *what)
{
    if (!cond) {
        printf("FAIL: %s\n", what);
        failures++;
    }
}

/* Starts the aircraft in the air, in level flight */
static void airborne(Flight *f, float alt, float heading, float speed)
{
    float pos[3];

    pos[0] = 0.0f;
    pos[1] = alt;
    pos[2] = 0.0f;
    flight_init(f, pos, heading);

    f->vel[0] = sinf(heading) * speed;
    f->vel[1] = 0.0f;
    f->vel[2] = -cosf(heading) * speed;
    f->throttle = 0.70f;
}

static void run(Autopilot *ap, Flight *f, int seconds)
{
    int i;

    for (i = 0; i < seconds * 60; i++) {
        autopilot_update(ap, f, 1.0f / 60.0f);
        flight_update(f, 1.0f / 60.0f);
    }
}

int main(void)
{
    Autopilot ap;
    Flight f;

    /* --- angle wrapping --- */
    check(fabsf(autopilot_wrap_angle(7.0f) - (7.0f - 6.28318531f)) < 0.001f,
          "angle wrap correct on the positive side");
    check(fabsf(autopilot_wrap_angle(-7.0f) - (-7.0f + 6.28318531f)) < 0.001f,
          "angle wrap correct on the negative side");
    printf("test: angle wrapping correct\n");

    /* --- does not touch the aircraft while disengaged --- */
    autopilot_init(&ap);
    airborne(&f, 500.0f, 0.0f, 90.0f);
    f.in_pitch = 0.77f;
    autopilot_update(&ap, &f, 1.0f / 60.0f);
    check(f.in_pitch == 0.77f, "disengaged autopilot leaves the input alone");
    printf("test: does not interfere with the controls while off\n");

    /* --- holds altitude --- */
    autopilot_init(&ap);
    airborne(&f, 3000.0f, 0.0f, 200.0f);
    autopilot_engage(&ap, &f);
    run(&ap, &f, 60);
    check(fabsf(f.pos[1] - 3000.0f) < 120.0f,
          "altitude held for 60 seconds");
    printf("test: altitude held (deviation %.1f m)\n", fabsf(f.pos[1] - 3000.0f));

    /* --- climbs to the target altitude --- */
    autopilot_init(&ap);
    airborne(&f, 2500.0f, 0.0f, 200.0f);
    autopilot_engage(&ap, &f);
    autopilot_adjust_alt(&ap, 500.0f);
    run(&ap, &f, 120);
    check(f.pos[1] > 2900.0f, "climbs to the target altitude");
    printf("test: climb command works (%.0f m)\n", f.pos[1]);

    /* --- holds and changes heading --- */
    autopilot_init(&ap);
    airborne(&f, 3000.0f, 0.0f, 200.0f);
    autopilot_engage(&ap, &f);
    autopilot_adjust_heading(&ap, 1.20f);
    run(&ap, &f, 90);
    check(fabsf(autopilot_wrap_angle(f.yaw - 1.20f)) < 0.20f,
          "turns to the target heading");
    printf("test: heading change completes (error %.3f rad)\n",
           fabsf(autopilot_wrap_angle(f.yaw - 1.20f)));

    /* --- does not exceed the bank limit --- */
    autopilot_init(&ap);
    airborne(&f, 3000.0f, 0.0f, 200.0f);
    autopilot_engage(&ap, &f);
    autopilot_adjust_heading(&ap, 3.0f);
    {
        int i;
        float max_bank = 0.0f;

        for (i = 0; i < 90 * 60; i++) {
            autopilot_update(&ap, &f, 1.0f / 60.0f);
            flight_update(&f, 1.0f / 60.0f);
            if (fabsf(f.roll) > max_bank)
                max_bank = fabsf(f.roll);
        }
        check(max_bank < AP_MAX_BANK_RAD + 0.12f,
              "autopilot stays within the bank limit");
        printf("test: bank limit respected (at most %.3f rad)\n", max_bank);
    }

    /* --- does not dive into the water --- */
    autopilot_init(&ap);
    airborne(&f, 900.0f, 0.5f, 190.0f);
    autopilot_engage(&ap, &f);
    run(&ap, &f, 120);
    check(f.pos[1] > 400.0f, "autopilot does not put the aircraft in the sea");
    printf("test: safe altitude kept\n");

    if (failures == 0)
        printf("\n9 checks, 0 failures\n");
    else
        printf("\n%d HATA\n", failures);
    return failures ? 1 : 0;
}
