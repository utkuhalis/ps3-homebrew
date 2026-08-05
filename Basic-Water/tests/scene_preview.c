/* Gokyuzu ve su formullerini host'ta isin atarak render eder.
 *
 * Amac: shader'lara cevirmeden once "gunes dogru yerde mi, bulutlar nasil
 * duruyor, dalga cok mu sert, yansima inandirici mi" sorularini yanitlamak.
 * PS3 veya emulator gerekmez.
 *
 * Calistirma: tests/scene_preview.sh */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "../source/skycolor.h"
#include "../source/waves.h"
#include "../source/camera.h"
#include "../source/mat4.h"

#define W 800
#define H 450

static unsigned char fb[H][W][3];

static float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static float mixf(float a, float b, float t) { return a + (b - a) * t; }

static void normalize3(float v[3])
{
    float l = sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
    if (l > 1e-6f) { v[0] /= l; v[1] /= l; v[2] /= l; }
}

/* Deniz yuzeyi ile kesisim: once duz duzlemle kaba kesisim, sonra dalga
 * yuksekligini hesaba katarak birkac adim iyilestirme. */
static int trace_water(const float eye[3], const float dir[3], float time,
                       float hit[3])
{
    float t, y;
    int i;

    if (dir[1] >= -0.0005f)
        return 0;                       /* yukari veya ufka paralel bakis */

    t = -eye[1] / dir[1];               /* y = 0 duzlemi */
    if (t <= 0.0f || t > 20000.0f)
        return 0;

    for (i = 0; i < 4; i++) {
        float px = eye[0] + dir[0] * t;
        float pz = eye[2] + dir[2] * t;
        float h = wave_height(px, pz, time);

        y = eye[1] + dir[1] * t;
        t += (y - h) / (-dir[1]) * 0.9f;
        if (t <= 0.0f) return 0;
    }

    hit[0] = eye[0] + dir[0] * t;
    hit[1] = eye[1] + dir[1] * t;
    hit[2] = eye[2] + dir[2] * t;
    return 1;
}

static void water_color(const float eye[3], const float dir[3],
                        const float hit[3], float time, float out[3])
{
    float n[3], refl[3], sky[3], spec_dir[3];
    float ndotv, fres, dist, fog, spec, sun_dot;
    float deep[3] = { 0.015f, 0.075f, 0.135f };
    int i;

    wave_normal(hit[0], hit[2], time, n);

    /* Yansima yonu: r = d - 2(d.n)n */
    ndotv = dir[0]*n[0] + dir[1]*n[1] + dir[2]*n[2];
    for (i = 0; i < 3; i++)
        refl[i] = dir[i] - 2.0f * ndotv * n[i];
    normalize3(refl);
    if (refl[1] < 0.0f) refl[1] = -refl[1];   /* asagi yansimayi yukari kivir */

    sky_color(refl, time, sky);

    /* Fresnel (Schlick): sig acida yansima baskin, dik bakista su rengi */
    fres = 0.02f + 0.98f * powf(1.0f - clampf(-ndotv, 0.0f, 1.0f), 5.0f);
    fres = clampf(fres, 0.0f, 1.0f);

    for (i = 0; i < 3; i++)
        out[i] = mixf(deep[i], sky[i], fres);

    /* Gunes parlamasi (Blinn-Phong): dalgalar uzerinde kipirdayan isik yolu */
    for (i = 0; i < 3; i++)
        spec_dir[i] = SUN_DIR[i] - dir[i];
    normalize3(spec_dir);
    sun_dot = clampf(spec_dir[0]*n[0] + spec_dir[1]*n[1] + spec_dir[2]*n[2],
                     0.0f, 1.0f);
    spec = powf(sun_dot, 180.0f) * 1.6f + powf(sun_dot, 24.0f) * 0.10f;

    out[0] += spec * 1.00f;
    out[1] += spec * 0.95f;
    out[2] += spec * 0.80f;

    /* Mesafeyle ufuk rengine karisma */
    dist = sqrtf((hit[0]-eye[0])*(hit[0]-eye[0]) +
                 (hit[1]-eye[1])*(hit[1]-eye[1]) +
                 (hit[2]-eye[2])*(hit[2]-eye[2]));
    fog = clampf(dist / 3000.0f, 0.0f, 1.0f);
    fog = fog * fog;
    {
        float horizon_dir[3] = { dir[0], 0.02f, dir[2] };
        float hcol[3];
        normalize3(horizon_dir);
        sky_color(horizon_dir, time, hcol);
        for (i = 0; i < 3; i++)
            out[i] = mixf(out[i], hcol[i], fog);
    }

    for (i = 0; i < 3; i++)
        out[i] = clampf(out[i], 0.0f, 1.0f);
}

static void render(const Camera *cam, float time, const char *path)
{
    float fwd[3], right[3], up[3];
    float aspect = (float)W / H;
    float tan_half = tanf(60.0f * 3.14159265f / 180.0f * 0.5f);
    FILE *f;
    int px, py, i;

    camera_forward(cam, fwd);
    camera_right(cam, right);
    vec3_cross(up, right, fwd);
    normalize3(up);

    for (py = 0; py < H; py++) {
        float sy = (1.0f - 2.0f * (py + 0.5f) / H) * tan_half;

        for (px = 0; px < W; px++) {
            float sx = (2.0f * (px + 0.5f) / W - 1.0f) * tan_half * aspect;
            float dir[3], col[3], hit[3];

            for (i = 0; i < 3; i++)
                dir[i] = fwd[i] + right[i] * sx + up[i] * sy;
            normalize3(dir);

            if (trace_water(cam->pos, dir, time, hit))
                water_color(cam->pos, dir, hit, time, col);
            else
                sky_color(dir, time, col);

            for (i = 0; i < 3; i++)
                fb[py][px][i] = (unsigned char)(clampf(col[i], 0.0f, 1.0f) * 255.0f);
        }
    }

    f = fopen(path, "wb");
    if (f == NULL) { fprintf(stderr, "yazilamadi: %s\n", path); exit(1); }
    fprintf(f, "P6\n%d %d\n255\n", W, H);
    fwrite(fb, 1, sizeof(fb), f);
    fclose(f);
    printf("  yazildi: %s\n", path);
}

int main(void)
{
    Camera c;

    /* 1) deniz seviyesinden ufka bakis */
    camera_init(&c);
    c.pos[1] = 12.0f;
    render(&c, 3.0f, "build-test/sahne-01-ufuk.ppm");

    /* 2) yuksekten hafif asagi bakis */
    camera_init(&c);
    c.pos[1] = 90.0f;
    c.pitch = -0.30f;
    render(&c, 7.0f, "build-test/sahne-02-yuksek.ppm");

    /* 3) gunese dogru bakis (parlama yolu gorunmeli) */
    camera_init(&c);
    c.pos[1] = 25.0f;
    c.yaw = atan2f(SUN_DIR[0], -SUN_DIR[2]);
    c.pitch = -0.05f;
    render(&c, 11.0f, "build-test/sahne-03-gunes.ppm");

    /* 4) yukari bakis (gokyuzu ve bulutlar) */
    camera_init(&c);
    c.pos[1] = 30.0f;
    c.pitch = 0.55f;
    render(&c, 5.0f, "build-test/sahne-04-gokyuzu.ppm");

    return 0;
}
