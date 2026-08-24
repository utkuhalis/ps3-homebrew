#include <math.h>

#include "atmosphere.h"

static void set3(float v[3], float a, float b, float c)
{
    v[0] = a; v[1] = b; v[2] = c;
}

static void normalize3(float v[3])
{
    float l = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);

    if (l > 1e-6f) {
        v[0] /= l; v[1] /= l; v[2] /= l;
    }
}

static void scale3(float v[3], float s)
{
    v[0] *= s; v[1] *= s; v[2] *= s;
}

/* Renk grileşmesi: hava bozdukca renkler doygunlugunu kaybedip gri tona kayar */
static void desaturate(float v[3], float amount)
{
    float gray = (v[0] + v[1] + v[2]) / 3.0f;
    int i;

    for (i = 0; i < 3; i++)
        v[i] += (gray - v[i]) * amount;
}

void atmosphere_compute(Atmosphere *a, Weather w, TimeOfDay t)
{
    /* --- once gun saati: gunes yonu ve temel renkler --- */
    switch (t) {
    case TIME_SUNSET:
        set3(a->sun_dir, 0.62f, 0.06f, -0.78f);     /* ufka cok yakin */
        set3(a->sun_color, 1.00f, 0.62f, 0.32f);    /* turuncu */
        set3(a->horizon, 0.95f, 0.62f, 0.38f);
        set3(a->zenith, 0.22f, 0.26f, 0.55f);
        a->water_dark = 0.85f;
        break;

    case TIME_NIGHT:
        set3(a->sun_dir, 0.30f, 0.42f, -0.86f);     /* ay */
        set3(a->sun_color, 0.62f, 0.70f, 0.92f);    /* soguk beyaz */
        set3(a->horizon, 0.07f, 0.10f, 0.19f);
        set3(a->zenith, 0.010f, 0.020f, 0.070f);
        a->water_dark = 0.30f;
        break;

    case TIME_DAY:
    default:
        set3(a->sun_dir, 0.34f, 0.30f, -0.89f);
        set3(a->sun_color, 1.00f, 0.92f, 0.72f);
        set3(a->horizon, 0.72f, 0.82f, 0.92f);
        set3(a->zenith, 0.16f, 0.38f, 0.78f);
        a->water_dark = 1.00f;
        break;
    }

    normalize3(a->sun_dir);

    /* --- varsayilanlar (guneşli hava) --- */
    a->cloud_low = 0.50f;
    a->cloud_high = 0.78f;
    a->cloud_bright = 1.00f;
    a->fog_distance = 1500.0f;
    a->wave_scale = 1.00f;
    a->wave_length = 1.00f;
    a->rain = 0.0f;
    a->lightning = 0.0f;

    /* --- hava durumu: temel degerleri degistirir --- */
    switch (w) {
    case WEATHER_CLOUDY:
        a->cloud_low = 0.30f;           /* esik dusuk -> gokyuzu daha kapali */
        a->cloud_high = 0.62f;
        a->cloud_bright = 0.88f;
        a->fog_distance = 1200.0f;
        desaturate(a->horizon, 0.35f);
        desaturate(a->zenith, 0.30f);
        scale3(a->zenith, 0.85f);
        a->wave_scale = 1.25f;
        a->wave_length = 1.15f;
        break;

    case WEATHER_RAINY:
        a->cloud_low = 0.18f;
        a->cloud_high = 0.50f;
        a->cloud_bright = 0.62f;
        a->fog_distance = 800.0f;
        desaturate(a->horizon, 0.55f);
        desaturate(a->zenith, 0.50f);
        scale3(a->horizon, 0.70f);
        scale3(a->zenith, 0.60f);
        a->wave_scale = 1.45f;
        a->wave_length = 1.35f;
        a->water_dark *= 0.75f;
        a->rain = 0.65f;
        break;

    case WEATHER_FOGGY:
        a->cloud_low = 0.35f;
        a->cloud_high = 0.70f;
        a->cloud_bright = 0.92f;
        a->fog_distance = 320.0f;       /* asil etki burada */
        desaturate(a->horizon, 0.70f);
        desaturate(a->zenith, 0.75f);
        /* sis gokyuzunu ufuk rengine yaklastirir */
        a->zenith[0] = a->zenith[0] * 0.4f + a->horizon[0] * 0.6f;
        a->zenith[1] = a->zenith[1] * 0.4f + a->horizon[1] * 0.6f;
        a->zenith[2] = a->zenith[2] * 0.4f + a->horizon[2] * 0.6f;
        a->wave_scale = 0.70f;          /* sisli havada deniz durgun */
        a->wave_length = 1.00f;
        break;

    case WEATHER_STORMY:
        a->cloud_low = 0.10f;
        a->cloud_high = 0.42f;
        a->cloud_bright = 0.42f;        /* koyu bulutlar */
        a->fog_distance = 600.0f;
        desaturate(a->horizon, 0.65f);
        desaturate(a->zenith, 0.60f);
        scale3(a->horizon, 0.48f);
        scale3(a->zenith, 0.40f);
        a->wave_scale = 1.85f;          /* azgin deniz */
        a->wave_length = 1.75f;   /* uzun ve yuksek dalgalar */
        a->water_dark *= 0.55f;
        a->rain = 1.0f;
        a->lightning = 1.0f;
        break;

    case WEATHER_SUNNY:
    default:
        break;
    }
}
