#include <math.h>

#include "skycolor.h"

/* Gunes: sag ust arka tarafta, ufkun bir miktar uzerinde */
const float SUN_DIR[3] = { 0.34f, 0.30f, -0.89f };

#define CLOUD_HEIGHT   900.0f   /* bulut katmaninin yuksekligi (birim) */
#define CLOUD_SCALE    0.0016f  /* bulut deseninin buyuklugu */
#define WIND_X         9.0f     /* ruzgar hizi (birim/saniye) */
#define WIND_Z         3.0f

static float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static float mixf(float a, float b, float t)
{
    return a + (b - a) * t;
}

static float smoothstepf(float e0, float e1, float x)
{
    float t = clampf((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

/* Bulut deseni - shaders/sky.fcg ve shaders/water.fcg ile BIREBIR ayni.
 *
 * Hash tabanli gurultu yerine ic ice sinusler kullaniliyor: RSX'in fragment
 * birimi dusuk hassasiyetle calistigi icin buyuk sabitli hash formulu
 * konsolda sabite donuyor ve bulutlar gorunmuyordu. */
static float cloud_pattern(float px, float py)
{
    float a = sinf(px * 0.90f + sinf(py * 0.70f) * 1.6f);
    float b = sinf(py * 1.10f + sinf(px * 0.62f) * 1.4f);
    float cc = sinf((px + py) * 0.55f + sinf(px * 0.31f) * 1.2f);
    float d = sinf((px - py) * 1.70f) * 0.5f;

    return (a + b * 0.85f + cc * 0.65f + d) * 0.17f + 0.5f;
}

float sky_clouds(const float dir[3], float time)
{
    float yy = dir[1] > 0.05f ? dir[1] : 0.05f;
    float t = CLOUD_HEIGHT / yy;
    float px = (dir[0] * t + WIND_X * time) * CLOUD_SCALE;
    float pz = (dir[2] * t + WIND_Z * time) * CLOUD_SCALE;

    float cover = smoothstepf(0.50f, 0.78f, cloud_pattern(px, pz));

    /* Ufka dogru bulutlar incelip kaybolur (atmosfer etkisi) */
    cover *= smoothstepf(0.02f, 0.30f, dir[1]);

    return cover;
}

void sky_color(const float dir[3], float time, float out[3])
{
    float up = clampf(dir[1], 0.0f, 1.0f);
    float sun_dot, disk, glow, cloud, cl;
    int i;

    /* Ufuktan zenite gradyan */
    float horizon[3] = { 0.72f, 0.82f, 0.92f };
    float zenith[3]  = { 0.16f, 0.38f, 0.78f };
    float grad = powf(up, 0.55f);

    for (i = 0; i < 3; i++)
        out[i] = mixf(horizon[i], zenith[i], grad);

    /* Gunes: keskin disk + genis hale */
    sun_dot = dir[0] * SUN_DIR[0] + dir[1] * SUN_DIR[1] + dir[2] * SUN_DIR[2];
    sun_dot = clampf(sun_dot, 0.0f, 1.0f);

    disk = smoothstepf(0.9987f, 0.9994f, sun_dot);
    glow = powf(sun_dot, 220.0f) * 0.55f + powf(sun_dot, 12.0f) * 0.16f;

    out[0] += glow * 1.00f;
    out[1] += glow * 0.92f;
    out[2] += glow * 0.72f;

    out[0] = mixf(out[0], 1.00f, disk);
    out[1] = mixf(out[1], 0.98f, disk);
    out[2] = mixf(out[2], 0.90f, disk);

    /* Bulutlar: gunese yakin olanlar hafif sarimsi aydinlanir */
    cloud = sky_clouds(dir, time);
    if (cloud > 0.0f) {
        float lit = 0.82f + 0.18f * powf(sun_dot, 4.0f);
        float cloud_col[3];

        cloud_col[0] = 1.00f * lit;
        cloud_col[1] = 0.99f * lit;
        cloud_col[2] = 0.97f * lit;

        for (i = 0; i < 3; i++)
            out[i] = mixf(out[i], cloud_col[i], cloud);
    }

    for (i = 0; i < 3; i++) {
        cl = clampf(out[i], 0.0f, 1.0f);
        out[i] = cl;
    }
}
