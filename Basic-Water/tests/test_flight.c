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

    for (i = 0; i < 12000; i++)
        flight_update(&f, 1.0f / 60.0f);

    CHECK(f.fuel_kg < FUEL_FULL_KG, "yakit azaldi");
    CHECK(f.mass_kg < ilk_agirlik, "agirlik azaldi");

    /* yakiti tamamen bitir */
    for (i = 0; i < 900000 && f.fuel_kg > 0.0f; i++)
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

static void test_pistte_basliyor(void)
{
    Flight f;
    float rw[2] = { -1500.0f, 1200.0f };

    printf("test: ucak pistte, duruyor ve motor rolantide basliyor\n");
    flight_init_on_runway(&f, rw, 0.0f, 460.0f);

    CHECK(f.on_ground, "yerde");
    CHECK(f.airspeed < 0.1f, "duruyor");
    CHECK(f.throttle < 0.01f, "motor rolantide");
    CHECK(f.gear_down, "inis takimi acik");
    CHECK(f.pos[1] > DECK_Y, "pist yuzeyinin uzerinde");
}

static void test_gaz_vermeden_kalkamaz(void)
{
    Flight f;
    float rw[2] = { 0.0f, 0.0f };
    int i;

    printf("test: gaz verilmeden ucak pistte kaliyor\n");
    flight_init_on_runway(&f, rw, 0.0f, 460.0f);
    f.in_pitch = 1.0f;              /* burnu kaldirmayi dene */

    for (i = 0; i < 600; i++)
        flight_update(&f, 1.0f / 60.0f);

    CHECK(f.on_ground, "hala yerde");
    CHECK(!f.airborne, "havalanmadi");
}

static void test_kalkis_yapilabiliyor(void)
{
    Flight f;
    float rw[2] = { 0.0f, 0.0f };
    int i;
    float max_speed = 0.0f;

    printf("test: tam gazla hizlanip burun kaldirilinca kalkis oluyor\n");
    flight_init_on_runway(&f, rw, 0.0f, 460.0f);
    f.throttle = 1.0f;

    /* Once hizlan (burun yerde), sonra burnu kaldir.
     * 30 saniye: gercekci itkiyle kalkis kosusu ~14 saniye suruyor,
     * tirmanis icin de zaman gerekiyor. Test onceden 15 saniyeydi ve
     * itki yariya indirildiginde yetmez oldu. */
    for (i = 0; i < 1800; i++) {
        flight_update(&f, 1.0f / 60.0f);
        if (f.airspeed > max_speed)
            max_speed = f.airspeed;
        /* rotate hizina ulasinca burnu kaldir */
        if (f.airspeed > ROTATE_SPEED_MS)
            f.in_pitch = 0.55f;
    }

    CHECK(max_speed > ROTATE_SPEED_MS, "kalkis hizina ulasildi");
    CHECK(f.airborne, "ucak havalandi");
    CHECK(f.pos[1] > DECK_Y + 10.0f, "yukselmeye devam etti");
}

static void test_gear_surukleme(void)
{
    float acik, kapali;

    printf("test: acik inis takimi suruklemeyi artiriyor\n");
    acik   = flight_drag_force_full(90.0f, 0.08f, 0.0f, 0.0f, 1);
    kapali = flight_drag_force_full(90.0f, 0.08f, 0.0f, 0.0f, 0);

    CHECK(acik > kapali, "takim acikken surukleme daha yuksek");
}

/* Duz ucusta ve donuste STALL uyarisi CIKMAMALI.
 *
 * Hucum acisi once hiz vektoru ile burun arasindaki toplam aci olarak
 * hesaplaniyordu; yan kayma da bu aciya karistigi icin her donuste sahte
 * stall uyarisi veriliyordu. */
static void test_donuste_sahte_stall_yok(void)
{
    Flight f;
    float pos[3];
    int i;
    int stall_frames = 0;
    float max_aoa = 0.0f;

    printf("test: duz ucus ve donuste sahte stall cikmiyor\n");

    pos[0] = 0.0f; pos[1] = 3000.0f; pos[2] = 0.0f;
    flight_init(&f, pos, 0.0f);
    f.throttle = 0.80f;

    /* once duz ucusta dengelensin */
    for (i = 0; i < 60 * 10; i++)
        flight_update(&f, 1.0f / 60.0f);

    /* Sonra 30 saniye koordineli donus: yatis 26 derecede TUTULUR.
     * Sabit aileron girdisi vermek ucagi takla attirir - gercek ucakta da
     * oyle olur, o yuzden pilot gibi yatisi sabit tutuyoruz. */
    for (i = 0; i < 60 * 30; i++) {
        float bank_target = 0.45f;

        f.in_roll  = (bank_target - f.roll) * 6.0f - f.r_rate * 3.0f;
        if (f.in_roll >  1.0f) f.in_roll =  1.0f;
        if (f.in_roll < -1.0f) f.in_roll = -1.0f;

        /* Irtifa tutulur. Sabit burun-yukari komutu vermek ucagi surekli
         * tirmandirip hiz kaybettiriyor ve sonunda dikleştiriyordu - gercek
         * pilot da irtifayi tutar, komutu sabit birakmaz. */
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

    printf("       donusteki en yuksek hucum acisi %.3f rad, stall %d kare\n",
           max_aoa, stall_frames);
    CHECK(stall_frames == 0, "koordineli donuste stall uyarisi cikmiyor");
    CHECK(f.airspeed > 120.0f, "donuste ucak hizini koruyor");
    CHECK(f.pos[1] > 2000.0f, "donuste ucak dusmuyor");
}

/* Gercek stall hala calismali: burun asiri kaldirilirsa uyari cikmali */
static void test_gercek_stall_calisiyor(void)
{
    Flight f;
    float pos[3];
    int i;
    int stalled_once = 0;

    printf("test: burun asiri kaldirilinca gercek stall oluyor\n");

    pos[0] = 0.0f; pos[1] = 4000.0f; pos[2] = 0.0f;
    flight_init(&f, pos, 0.0f);
    f.throttle = 0.0f;              /* gaz kesik: hiz duser */

    for (i = 0; i < 60 * 40; i++) {
        f.in_pitch = 1.0f;          /* burnu sonuna kadar cek */
        flight_update(&f, 1.0f / 60.0f);
        if (f.stalled)
            stalled_once = 1;
    }

    CHECK(stalled_once, "asiri hucum acisinda stall algilaniyor");
}

/* Pist collideri DIKDORTGEN olmali: pist 1100x96 birim, daire kullanmak
 * ucagin pistin yanindaki denize asfalta iner gibi konmasina yol aciyordu. */
static void test_pist_dikdortgen_collider(void)
{
    Flight f;
    float rw[2] = { 0.0f, 0.0f };

    printf("test: pist collideri dikdortgen\n");

    /* pist yonu 0: burun -Z, yani pist Z ekseninde uzanir */
    flight_init_on_runway(&f, rw, 0.0f, 0.0f);

    CHECK(flight_over_runway(&f, 0.0f, 0.0f), "merkez pist uzerinde");
    CHECK(flight_over_runway(&f, 0.0f, -500.0f), "pist basi uzerinde");
    CHECK(flight_over_runway(&f, 0.0f, 500.0f), "pist sonu uzerinde");
    CHECK(flight_over_runway(&f, 40.0f, 0.0f), "pist genisligi icinde");

    CHECK(!flight_over_runway(&f, 200.0f, 0.0f),
          "pistin 200 birim yani DISARIDA");
    CHECK(!flight_over_runway(&f, 0.0f, 700.0f),
          "pistin 700 birim otesi DISARIDA");
    CHECK(!flight_over_runway(&f, 60.0f, 0.0f),
          "genislik sinirinin disi DISARIDA");

    /* dondurulmus pist: yon 90 derece -> pist X ekseninde uzanir */
    flight_init_on_runway(&f, rw, 1.5708f, 0.0f);
    CHECK(flight_over_runway(&f, 500.0f, 0.0f),
          "dondurulmus pistte uzun eksen dogru");
    CHECK(!flight_over_runway(&f, 0.0f, 200.0f),
          "dondurulmus pistte enine sinir dogru");
}

/* Pistin yanindaki denize inen ucak, asfaltta gibi davranmamali */
static void test_pist_yaninda_su(void)
{
    Flight f;
    float rw[2] = { 0.0f, 0.0f };
    int i;

    printf("test: pistin yanina inince su gibi davraniyor\n");

    flight_init_on_runway(&f, rw, 0.0f, 0.0f);
    f.pos[0] = 300.0f;              /* pistin 300 birim yani: deniz */
    f.pos[1] = DECK_Y + 20.0f;
    f.vel[1] = -8.0f;

    for (i = 0; i < 600; i++)
        flight_update(&f, 1.0f / 60.0f);

    CHECK(f.pos[1] < DECK_Y, "pist yaninda deniz seviyesine iniyor");
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
