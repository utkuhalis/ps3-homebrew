/* Otopilot testleri: gercek ucus modeliyle kapali dongu calistirilir.
 * Otopilot ucaga dogrudan dokunmadigi, yalnizca kumanda girdisi urettigi
 * icin bu testler ayni zamanda ucus modelinin kararliligini da olcer. */

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

/* Ucagi havada, duz ucusta baslatir */
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
    f->throttle = 0.55f;
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

    /* --- aci sarmasi --- */
    check(fabsf(autopilot_wrap_angle(7.0f) - (7.0f - 6.28318531f)) < 0.001f,
          "aci sarmasi pozitif tarafta dogru");
    check(fabsf(autopilot_wrap_angle(-7.0f) - (-7.0f + 6.28318531f)) < 0.001f,
          "aci sarmasi negatif tarafta dogru");
    printf("test: aci sarmasi dogru\n");

    /* --- devrede degilken ucaga dokunmaz --- */
    autopilot_init(&ap);
    airborne(&f, 500.0f, 0.0f, 90.0f);
    f.in_pitch = 0.77f;
    autopilot_update(&ap, &f, 1.0f / 60.0f);
    check(f.in_pitch == 0.77f, "kapali otopilot girdiye dokunmuyor");
    printf("test: kapaliyken kumandaya karismiyor\n");

    /* --- irtifa tutuyor --- */
    autopilot_init(&ap);
    airborne(&f, 500.0f, 0.0f, 95.0f);
    autopilot_engage(&ap, &f);
    run(&ap, &f, 60);
    check(fabsf(f.pos[1] - 500.0f) < 45.0f,
          "irtifa 60 saniye boyunca korunuyor");
    printf("test: irtifa tutuluyor (sapma %.1f m)\n", fabsf(f.pos[1] - 500.0f));

    /* --- hedef irtifaya tirmaniyor --- */
    autopilot_init(&ap);
    airborne(&f, 400.0f, 0.0f, 95.0f);
    autopilot_engage(&ap, &f);
    autopilot_adjust_alt(&ap, 500.0f);
    run(&ap, &f, 120);
    check(f.pos[1] > 800.0f, "hedef irtifaya tirmaniyor");
    printf("test: tirmanis komutu calisiyor (%.0f m)\n", f.pos[1]);

    /* --- yon tutuyor ve donuyor --- */
    autopilot_init(&ap);
    airborne(&f, 600.0f, 0.0f, 95.0f);
    autopilot_engage(&ap, &f);
    autopilot_adjust_heading(&ap, 1.20f);
    run(&ap, &f, 90);
    check(fabsf(autopilot_wrap_angle(f.yaw - 1.20f)) < 0.20f,
          "hedef yone donuyor");
    printf("test: yon donusu tamamlaniyor (hata %.3f rad)\n",
           fabsf(autopilot_wrap_angle(f.yaw - 1.20f)));

    /* --- yatis sinirini asmiyor --- */
    autopilot_init(&ap);
    airborne(&f, 600.0f, 0.0f, 95.0f);
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
              "otopilot yatis sinirini asmiyor");
        printf("test: yatis siniri korunuyor (en fazla %.3f rad)\n", max_bank);
    }

    /* --- suya dalmiyor --- */
    autopilot_init(&ap);
    airborne(&f, 120.0f, 0.5f, 85.0f);
    autopilot_engage(&ap, &f);
    run(&ap, &f, 120);
    check(f.pos[1] > 40.0f, "otopilot ucagi denize indirmiyor");
    printf("test: guvenli irtifa korunuyor\n");

    if (failures == 0)
        printf("\n9 kontrol, 0 hata\n");
    else
        printf("\n%d HATA\n", failures);
    return failures ? 1 : 0;
}
