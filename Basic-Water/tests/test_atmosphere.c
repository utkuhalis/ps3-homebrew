/* atmosphere icin host tarafi birim testleri */
#include <stdio.h>
#include <math.h>
#include "../source/atmosphere.h"

static int failures = 0, checks = 0;
#define CHECK(cond, msg) do { checks++; if (!(cond)) { \
    printf("  FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); failures++; } } while (0)

static float len3(const float v[3])
{
    return sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

static int in01(const float v[3])
{
    int i;
    for (i = 0; i < 3; i++)
        if (v[i] < 0.0f || v[i] > 1.5f)
            return 0;
    return 1;
}

static void test_tum_kombinasyonlar_gecerli(void)
{
    Atmosphere a;
    int w, t;

    printf("test: her hava/saat kombinasyonu gecerli degerler uretiyor\n");
    for (w = 0; w < WEATHER_COUNT; w++) {
        for (t = 0; t < TIME_COUNT; t++) {
            atmosphere_compute(&a, (Weather)w, (TimeOfDay)t);

            CHECK(fabsf(len3(a.sun_dir) - 1.0f) < 0.01f, "gunes yonu birim vektor");
            CHECK(in01(a.horizon), "ufuk rengi gecerli aralikta");
            CHECK(in01(a.zenith), "tepe rengi gecerli aralikta");
            CHECK(in01(a.sun_color), "gunes rengi gecerli aralikta");
            CHECK(a.fog_distance > 0.0f, "sis mesafesi pozitif");
            CHECK(a.wave_scale > 0.0f, "dalga carpani pozitif");
            CHECK(a.cloud_low < a.cloud_high, "bulut esikleri tutarli");
            CHECK(a.rain >= 0.0f && a.rain <= 1.0f, "yagmur 0..1");
        }
    }
}

static void test_gece_karanlik(void)
{
    Atmosphere gunduz, gece;
    float g, n;

    printf("test: gece gunduzden belirgin karanlik\n");
    atmosphere_compute(&gunduz, WEATHER_SUNNY, TIME_DAY);
    atmosphere_compute(&gece, WEATHER_SUNNY, TIME_NIGHT);

    g = gunduz.zenith[0] + gunduz.zenith[1] + gunduz.zenith[2];
    n = gece.zenith[0] + gece.zenith[1] + gece.zenith[2];

    CHECK(n < g * 0.5f, "gece gokyuzu cok daha koyu");
    CHECK(gece.water_dark < gunduz.water_dark, "gece su daha koyu");
}

static void test_sis_gorusu_kisaltiyor(void)
{
    Atmosphere sunny, foggy;

    printf("test: sisli havada gorus mesafesi ciddi kisaliyor\n");
    atmosphere_compute(&sunny, WEATHER_SUNNY, TIME_DAY);
    atmosphere_compute(&foggy, WEATHER_FOGGY, TIME_DAY);

    CHECK(foggy.fog_distance < sunny.fog_distance * 0.4f, "sis mesafesi kisaldi");
    CHECK(foggy.wave_scale < sunny.wave_scale, "sisli havada deniz durgun");
}

static void test_firtina_en_siddetli(void)
{
    Atmosphere a[WEATHER_COUNT];
    int i;
    int en_buyuk_dalga = 0;

    printf("test: firtina en buyuk dalgalari ve yagmuru uretiyor\n");
    for (i = 0; i < WEATHER_COUNT; i++) {
        atmosphere_compute(&a[i], (Weather)i, TIME_DAY);
        if (a[i].wave_scale > a[en_buyuk_dalga].wave_scale)
            en_buyuk_dalga = i;
    }

    CHECK(en_buyuk_dalga == WEATHER_STORMY, "en buyuk dalga firtinada");
    CHECK(a[WEATHER_STORMY].rain > a[WEATHER_RAINY].rain, "firtinada yagmur daha siddetli");
    CHECK(a[WEATHER_STORMY].lightning > 0.0f, "firtinada simsek var");
    CHECK(a[WEATHER_SUNNY].rain == 0.0f, "gunesli havada yagmur yok");
    CHECK(a[WEATHER_SUNNY].lightning == 0.0f, "gunesli havada simsek yok");
    CHECK(a[WEATHER_STORMY].cloud_bright < a[WEATHER_SUNNY].cloud_bright,
          "firtina bulutlari daha koyu");
}

static void test_bulutlu_daha_kapali(void)
{
    Atmosphere sunny, cloudy;

    printf("test: bulutlu havada gokyuzu daha kapali\n");
    atmosphere_compute(&sunny, WEATHER_SUNNY, TIME_DAY);
    atmosphere_compute(&cloudy, WEATHER_CLOUDY, TIME_DAY);

    /* esik dustukce daha cok bolge bulut sayilir */
    CHECK(cloudy.cloud_low < sunny.cloud_low, "bulut esigi dustu");
}

int main(void)
{
    test_tum_kombinasyonlar_gecerli();
    test_gece_karanlik();
    test_sis_gorusu_kisaltiyor();
    test_firtina_en_siddetli();
    test_bulutlu_daha_kapali();

    printf("\n%d kontrol, %d hata\n", checks, failures);
    return failures ? 1 : 0;
}
