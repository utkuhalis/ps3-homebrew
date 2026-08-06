/* Ucus modeli icin host tarafi birim testleri (PS3 gerekmez). */
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

    printf("test: tasima hizin karesiyle artiyor\n");
    l1 = flight_lift_force(50.0f, 0.08f, 0.0f, 0.0f);
    l2 = flight_lift_force(100.0f, 0.08f, 0.0f, 0.0f);

    CHECK(l2 > l1 * 3.5f && l2 < l1 * 4.5f, "hiz iki kat -> tasima ~dort kat");
}

static void test_stall(void)
{
    float normal, asiri;

    printf("test: kritik acidan sonra tasima cokuyor (stall)\n");
    normal = flight_lift_force(80.0f, STALL_ANGLE_RAD - 0.02f, 0.0f, 0.0f);
    asiri  = flight_lift_force(80.0f, STALL_ANGLE_RAD + 0.25f, 0.0f, 0.0f);

    CHECK(asiri < normal * 0.6f, "stall sonrasi tasima belirgin dustu");
}

static void test_flap_kritik_aciyi_artiriyor(void)
{
    float aoa = STALL_ANGLE_RAD + 0.03f;
    float flapsiz, flapli;

    printf("test: flap acikken kritik aci yukseliyor\n");
    flapsiz = flight_lift_force(70.0f, aoa, 0.0f, 0.0f);
    flapli  = flight_lift_force(70.0f, aoa, 1.0f, 0.0f);

    CHECK(flapli > flapsiz, "ayni acida flap daha cok tasima veriyor");
}

static void test_flap_dusuk_hizda_ucmayi_sagliyor(void)
{
    float agirlik = (EMPTY_MASS_KG + FUEL_FULL_KG) * GRAVITY;
    float hiz;
    float flapsiz_min = 0.0f, flapli_min = 0.0f;

    printf("test: flap acikken daha dusuk hizda havada kalinabiliyor\n");

    for (hiz = 20.0f; hiz < 200.0f; hiz += 0.5f) {
        if (flapsiz_min == 0.0f &&
            flight_lift_force(hiz, 0.20f, 0.0f, 0.0f) >= agirlik)
            flapsiz_min = hiz;
        if (flapli_min == 0.0f &&
            flight_lift_force(hiz, 0.20f, 1.0f, 0.0f) >= agirlik)
            flapli_min = hiz;
    }

    CHECK(flapsiz_min > 0.0f && flapli_min > 0.0f, "iki durumda da ucabiliyor");
    CHECK(flapli_min < flapsiz_min, "flap asgari ucus hizini dusurdu");
}

static void test_spoiler_frenliyor(void)
{
    float d0, d1, l0, l1;

    printf("test: spoiler tasimayi kiriyor ve suruklemeyi artiriyor\n");
    l0 = flight_lift_force(90.0f, 0.10f, 0.0f, 0.0f);
    l1 = flight_lift_force(90.0f, 0.10f, 0.0f, 1.0f);
    d0 = flight_drag_force(90.0f, 0.10f, 0.0f, 0.0f);
    d1 = flight_drag_force(90.0f, 0.10f, 0.0f, 1.0f);

    CHECK(l1 < l0 * 0.6f, "tasima belirgin dustu");
    CHECK(d1 > d0 * 1.5f, "surukleme belirgin artti");
}

static void test_yakit_ve_agirlik(void)
{
    Flight f;
    float start[3] = { 0.0f, 500.0f, 0.0f };
    float ilk_agirlik;
    int i;

    printf("test: yakit tukeniyor, agirlik azaliyor, sonra itki kesiliyor\n");
    flight_init(&f, start, 0.0f);
    ilk_agirlik = f.mass_kg;
    f.throttle = 1.0f;

    for (i = 0; i < 600; i++)
        flight_update(&f, 1.0f / 60.0f);

    CHECK(f.fuel_kg < FUEL_FULL_KG, "yakit azaldi");
    CHECK(f.mass_kg < ilk_agirlik, "agirlik azaldi");

    /* yakiti tamamen bitir */
    for (i = 0; i < 200000 && f.fuel_kg > 0.0f; i++)
        flight_update(&f, 1.0f / 60.0f);

    CHECK(f.fuel_kg == 0.0f, "yakit bitti");
    CHECK(fabsf(f.mass_kg - EMPTY_MASS_KG) < 0.01f, "agirlik bos agirliga esit");
}

static void test_denizin_altina_inilemiyor(void)
{
    Flight f;
    float start[3] = { 0.0f, 300.0f, 0.0f };
    int i, batti = 0;

    printf("test: hicbir girdi ucagi denizin altina indiremiyor\n");
    flight_init(&f, start, 0.0f);
    f.throttle = 1.0f;
    f.in_pitch = -1.0f;             /* burun asagi, tam gaz */

    for (i = 0; i < 20000; i++) {
        flight_update(&f, 1.0f / 60.0f);
        if (f.pos[1] < WATER_SAFE_ALT - 0.01f)
            batti = 1;
    }

    CHECK(!batti, "hicbir karede deniz seviyesinin altina inmedi");
}

static void test_gaz_hizlandiriyor(void)
{
    Flight a, b;
    float start[3] = { 0.0f, 800.0f, 0.0f };
    int i;

    printf("test: gaz acikken ucak hizlaniyor\n");
    flight_init(&a, start, 0.0f);
    flight_init(&b, start, 0.0f);
    a.throttle = 0.0f;
    b.throttle = 1.0f;

    for (i = 0; i < 300; i++) {
        flight_update(&a, 1.0f / 60.0f);
        flight_update(&b, 1.0f / 60.0f);
    }

    CHECK(b.airspeed > a.airspeed, "tam gaz daha yuksek hiz veriyor");
}

static void test_yonelim_sinirli(void)
{
    Flight f;
    float start[3] = { 0.0f, 600.0f, 0.0f };
    int i;

    printf("test: burun acisi sinirli, yaw ve roll sarmali\n");
    flight_init(&f, start, 0.0f);
    f.in_pitch = 1.0f;
    f.in_roll = 1.0f;

    for (i = 0; i < 3000; i++)
        flight_update(&f, 1.0f / 60.0f);

    CHECK(f.pitch <= 1.31f && f.pitch >= -1.31f, "burun acisi sinirli");
    CHECK(f.roll >= -3.15f && f.roll <= 3.15f, "yatis acisi sarmali");
    CHECK(f.yaw >= -3.15f && f.yaw <= 3.15f, "sapma acisi sarmali");
}

int main(void)
{
    test_tasima_hizla_artiyor();
    test_stall();
    test_flap_kritik_aciyi_artiriyor();
    test_flap_dusuk_hizda_ucmayi_sagliyor();
    test_spoiler_frenliyor();
    test_yakit_ve_agirlik();
    test_denizin_altina_inilemiyor();
    test_gaz_hizlandiriyor();
    test_yonelim_sinirli();

    printf("\n%d kontrol, %d hata\n", checks, failures);
    return failures ? 1 : 0;
}
