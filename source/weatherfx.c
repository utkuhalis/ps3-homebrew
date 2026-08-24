#include <math.h>

#include "weatherfx.h"
#include "overlay.h"

#define RAIN_MAX     150     /* dusen yagmur cizgisi sayisi */
#define DROPS_MAX     34     /* ekranda (camda) duran damlacik sayisi */
#define RAIN_SPEED  1400.0f  /* dusme hizi (sanal piksel/saniye) */
#define DROP_H        22
#define DROP_W         2

#define COL_RAIN   RGB(200, 220, 245)
#define COL_FLASH  RGB(255, 255, 255)

/* Konuma bagli sozde-rastgele deger (0..1). Deterministik oldugu icin
 * damlalar kare kare zıplamaz, duzgun akar. */
static float hash1(int i, int salt)
{
    float n = sinf((float)i * 12.9898f + (float)salt * 78.233f) * 43758.5453f;
    return n - floorf(n);
}

static void draw_rain(float amount, float time)
{
    int count = (int)(RAIN_MAX * amount);
    int i;

    for (i = 0; i < count; i++) {
        float x = hash1(i, 1) * OVL_W;
        float phase = hash1(i, 2);
        float speed = RAIN_SPEED * (0.75f + 0.5f * hash1(i, 3));
        float span = OVL_H + DROP_H * 2.0f;
        float y = fmodf(phase * span + time * speed, span) - DROP_H;
        /* hafif egik dusus */
        float drift = (y / OVL_H) * 18.0f;

        overlay_soft_blob((int)(x + drift), (int)y, DROP_W + 2, DROP_H,
                          COL_RAIN, 120);
    }
}

/* Simsek: duzensiz araliklarla kisa parlama. Iki asamali (on carpma +
 * asil carpma) oldugu icin gercekci hissettiriyor. */
static void draw_lightning(float amount, float time)
{
    float cycle = 5.3f;
    float t = fmodf(time, cycle);
    int alpha = 0;

    if (t < 0.06f)
        alpha = 150;
    else if (t >= 0.13f && t < 0.17f)
        alpha = 90;
    else if (t >= 0.20f && t < 0.30f)
        alpha = 210;

    if (alpha > 0)
        overlay_blend_rect(0, 0, OVL_W, OVL_H, COL_FLASH,
                           (int)(alpha * amount));
}

/* Ekrana (kokpit camina) carpip kalan damlalar.
 *
 * Her damla kendi omru boyunca belirir, buyur, agirlastikca hizlanarak
 * asagi suzulur ve arkasinda ince bir iz birakir. Sekil, cizim katmanindaki
 * "yumusak leke" ile olusturulur: dortgen fragment tarafinda merkezden disa
 * sonduruldugu icin kenarlar kare gorunmez. */
static void draw_droplets(float amount, float time)
{
    int count = (int)(DROPS_MAX * amount);
    int i;

    for (i = 0; i < count; i++) {
        float life = 2.4f + hash1(i, 11) * 3.6f;
        float t0 = hash1(i, 12) * life;
        float age = fmodf(time + t0, life);
        float k = age / life;

        float x = hash1(i, 13) * OVL_W;
        float y0 = hash1(i, 14) * (OVL_H - 140.0f) + 30.0f;
        float y = y0 + k * k * 110.0f;

        float grow = k < 0.18f ? k / 0.18f : 1.0f;
        float fade = k > 0.78f ? (1.0f - k) / 0.22f : 1.0f;
        float base = 14.0f + hash1(i, 15) * 20.0f;
        float s = base * (0.55f + 0.45f * grow);
        int   a = (int)(120.0f * grow * fade * amount);

        if (a <= 2 || s < 4.0f)
            continue;

        /* damla: hafif dikey oval, ust ust iki katman (ortasi daha parlak) */
        overlay_soft_blob((int)(x - s * 0.5f), (int)(y - s * 0.6f),
                          (int)s, (int)(s * 1.25f), COL_RAIN, a);
        overlay_soft_blob((int)(x - s * 0.28f), (int)(y - s * 0.32f),
                          (int)(s * 0.55f), (int)(s * 0.7f), COL_RAIN,
                          a * 3 / 4);

        /* suzulme izi */
        if (k > 0.4f)
            overlay_soft_blob((int)(x - s * 0.16f), (int)(y - s * 1.5f),
                              (int)(s * 0.32f), (int)(s * 1.6f),
                              COL_RAIN, a / 3);
    }
}

void weatherfx_draw(const Atmosphere *atm, float time)
{
    if (atm->rain > 0.0f) {
        draw_rain(atm->rain, time);
        draw_droplets(atm->rain, time);
    }

    if (atm->lightning > 0.0f)
        draw_lightning(atm->lightning, time);
}
