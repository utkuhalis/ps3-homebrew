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

/* Greying: as the weather worsens, colours lose saturation and drift to grey */
static void desaturate(float v[3], float amount)
{
    float gray = (v[0] + v[1] + v[2]) / 3.0f;
    int i;

    for (i = 0; i < 3; i++)
        v[i] += (gray - v[i]) * amount;
}

void atmosphere_compute(Atmosphere *a, Weather w, TimeOfDay t)
{
    /* --- time of day first: sun direction and base colours --- */
    switch (t) {
    case TIME_SUNSET:
        set3(a->sun_dir, 0.62f, 0.06f, -0.78f);     /* very close to the horizon */
        set3(a->sun_color, 1.00f, 0.62f, 0.32f);    /* orange */
        set3(a->horizon, 0.95f, 0.62f, 0.38f);
        set3(a->zenith, 0.22f, 0.26f, 0.55f);
        a->water_dark = 0.85f;
        break;

    case TIME_NIGHT:
        set3(a->sun_dir, 0.30f, 0.42f, -0.86f);     /* moon */
        set3(a->sun_color, 0.62f, 0.70f, 0.92f);    /* cold white */
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

    /* --- defaults (sunny weather) --- */
    a->cloud_low = 0.50f;
    a->cloud_high = 0.78f;
    a->cloud_bright = 1.00f;
    a->fog_distance = 1500.0f;
    a->wave_scale = 1.00f;
    a->wave_length = 1.00f;
    a->rain = 0.0f;
    a->lightning = 0.0f;

    /* --- weather: modifies the base values --- */
    switch (w) {
    case WEATHER_CLOUDY:
        a->cloud_low = 0.30f;           /* lower threshold -> more overcast sky */
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
        a->fog_distance = 320.0f;       /* the main effect is here */
        desaturate(a->horizon, 0.70f);
        desaturate(a->zenith, 0.75f);
        /* fog pulls the sky towards the horizon colour */
        a->zenith[0] = a->zenith[0] * 0.4f + a->horizon[0] * 0.6f;
        a->zenith[1] = a->zenith[1] * 0.4f + a->horizon[1] * 0.6f;
        a->zenith[2] = a->zenith[2] * 0.4f + a->horizon[2] * 0.6f;
        a->wave_scale = 0.70f;          /* the sea is calm in fog */
        a->wave_length = 1.00f;
        break;

    case WEATHER_STORMY:
        a->cloud_low = 0.10f;
        a->cloud_high = 0.42f;
        a->cloud_bright = 0.42f;        /* dark clouds */
        a->fog_distance = 600.0f;
        desaturate(a->horizon, 0.65f);
        desaturate(a->zenith, 0.60f);
        scale3(a->horizon, 0.48f);
        scale3(a->zenith, 0.40f);
        a->wave_scale = 1.85f;          /* rough sea */
        a->wave_length = 1.75f;   /* long, high waves */
        a->water_dark *= 0.55f;
        a->rain = 1.0f;
        a->lightning = 1.0f;
        break;

    case WEATHER_SUNNY:
    default:
        break;
    }
}
