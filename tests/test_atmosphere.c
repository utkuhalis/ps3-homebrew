/* Host-side unit tests for atmosphere */
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

    printf("test: every weather/time combination produces valid values\n");
    for (w = 0; w < WEATHER_COUNT; w++) {
        for (t = 0; t < TIME_COUNT; t++) {
            atmosphere_compute(&a, (Weather)w, (TimeOfDay)t);

            CHECK(fabsf(len3(a.sun_dir) - 1.0f) < 0.01f, "sun direction is a unit vector");
            CHECK(in01(a.horizon), "horizon colour in range");
            CHECK(in01(a.zenith), "zenith colour in range");
            CHECK(in01(a.sun_color), "sun colour in range");
            CHECK(a.fog_distance > 0.0f, "fog distance positive");
            CHECK(a.wave_scale > 0.0f, "wave multiplier positive");
            CHECK(a.cloud_low < a.cloud_high, "cloud thresholds consistent");
            CHECK(a.rain >= 0.0f && a.rain <= 1.0f, "yagmur 0..1");
        }
    }
}

static void test_gece_karanlik(void)
{
    Atmosphere gunduz, gece;
    float g, n;

    printf("test: night is clearly darker than day\n");
    atmosphere_compute(&gunduz, WEATHER_SUNNY, TIME_DAY);
    atmosphere_compute(&gece, WEATHER_SUNNY, TIME_NIGHT);

    g = gunduz.zenith[0] + gunduz.zenith[1] + gunduz.zenith[2];
    n = gece.zenith[0] + gece.zenith[1] + gece.zenith[2];

    CHECK(n < g * 0.5f, "night sky much darker");
    CHECK(gece.water_dark < gunduz.water_dark, "night water darker");
}

static void test_sis_gorusu_kisaltiyor(void)
{
    Atmosphere sunny, foggy;

    printf("test: fog seriously shortens visibility\n");
    atmosphere_compute(&sunny, WEATHER_SUNNY, TIME_DAY);
    atmosphere_compute(&foggy, WEATHER_FOGGY, TIME_DAY);

    CHECK(foggy.fog_distance < sunny.fog_distance * 0.4f, "fog distance shortened");
    CHECK(foggy.wave_scale < sunny.wave_scale, "sea is calm in fog");
}

static void test_firtina_en_siddetli(void)
{
    Atmosphere a[WEATHER_COUNT];
    int i;
    int en_buyuk_dalga = 0;

    printf("test: storm produces the biggest waves and the rain\n");
    for (i = 0; i < WEATHER_COUNT; i++) {
        atmosphere_compute(&a[i], (Weather)i, TIME_DAY);
        if (a[i].wave_scale > a[en_buyuk_dalga].wave_scale)
            en_buyuk_dalga = i;
    }

    CHECK(en_buyuk_dalga == WEATHER_STORMY, "biggest waves in the storm");
    CHECK(a[WEATHER_STORMY].rain > a[WEATHER_RAINY].rain, "heavier rain in the storm");
    CHECK(a[WEATHER_STORMY].lightning > 0.0f, "lightning in the storm");
    CHECK(a[WEATHER_SUNNY].rain == 0.0f, "no rain in sunny weather");
    CHECK(a[WEATHER_SUNNY].lightning == 0.0f, "no lightning in sunny weather");
    CHECK(a[WEATHER_STORMY].cloud_bright < a[WEATHER_SUNNY].cloud_bright,
          "storm clouds darker");
}

static void test_bulutlu_daha_kapali(void)
{
    Atmosphere sunny, cloudy;

    printf("test: cloudy weather gives a more overcast sky\n");
    atmosphere_compute(&sunny, WEATHER_SUNNY, TIME_DAY);
    atmosphere_compute(&cloudy, WEATHER_CLOUDY, TIME_DAY);

    /* the lower the threshold, the more of the sky counts as cloud */
    CHECK(cloudy.cloud_low < sunny.cloud_low, "cloud threshold lowered");
}

int main(void)
{
    test_tum_kombinasyonlar_gecerli();
    test_gece_karanlik();
    test_sis_gorusu_kisaltiyor();
    test_firtina_en_siddetli();
    test_bulutlu_daha_kapali();

    printf("\n%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
