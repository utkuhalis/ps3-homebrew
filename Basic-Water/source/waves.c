#include <math.h>

#include "waves.h"

/* Dort dalga: farkli yon, dalga boyu, genlik ve hiz.
 * Yonler bilerek birbirine paralel degil; aksi halde su "cizgili" gorunur. */
typedef struct {
    float dx, dz;   /* yayilma yonu (birim) */
    float len;      /* dalga boyu (birim) */
    float amp;      /* genlik (birim) */
    float speed;    /* ilerleme hizi */
} Wave;

static const Wave WAVES[4] = {
    { 1.00f,  0.00f, 140.0f, 1.10f, 1.00f },
    { 0.62f,  0.78f,  74.0f, 0.62f, 1.35f },
    { -0.45f, 0.89f,  38.0f, 0.30f, 1.70f },
    { 0.86f, -0.51f,  19.0f, 0.14f, 2.20f },
};

#define TWO_PI 6.28318531f

float wave_height(float x, float z, float time)
{
    float h = 0.0f;
    int i;

    for (i = 0; i < 4; i++) {
        const Wave *w = &WAVES[i];
        float k = TWO_PI / w->len;
        float phase = (x * w->dx + z * w->dz) * k + time * w->speed * k * 12.0f;

        h += sinf(phase) * w->amp;
    }
    return h;
}

void wave_normal(float x, float z, float time, float out[3])
{
    float dhdx = 0.0f, dhdz = 0.0f;
    float len;
    int i;

    /* Egim analitik turevden: yaklasik fark yerine dogrudan cosinus */
    for (i = 0; i < 4; i++) {
        const Wave *w = &WAVES[i];
        float k = TWO_PI / w->len;
        float phase = (x * w->dx + z * w->dz) * k + time * w->speed * k * 12.0f;
        float c = cosf(phase) * w->amp * k;

        dhdx += c * w->dx;
        dhdz += c * w->dz;
    }

    /* Yuzey normali: (-dh/dx, 1, -dh/dz) normalize */
    out[0] = -dhdx;
    out[1] = 1.0f;
    out[2] = -dhdz;

    len = sqrtf(out[0] * out[0] + out[1] * out[1] + out[2] * out[2]);
    if (len > 1e-6f) {
        out[0] /= len;
        out[1] /= len;
        out[2] /= len;
    }
}
