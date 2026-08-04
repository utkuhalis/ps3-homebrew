/* mat4 ve camera icin host tarafi birim testleri (PS3 gerekmez).
 * Calistirma: ./build.sh test */
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "../source/mat4.h"
#include "../source/camera.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond, msg) do {                                        \
        checks++;                                                    \
        if (!(cond)) {                                               \
            printf("  FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            failures++;                                              \
        }                                                            \
    } while (0)

#define CLOSE(a, b) (fabsf((a) - (b)) < 1e-4f)

static void test_birim_matris(void)
{
    Mat4 i = mat4_identity();
    Mat4 p = mat4_perspective(1.0f, 16.0f / 9.0f, 1.0f, 1000.0f);
    Mat4 r = mat4_mul(&p, &i);
    int k;
    int ayni = 1;

    printf("test: birim matrisle carpim degistirmiyor\n");
    for (k = 0; k < 16; k++)
        if (!CLOSE(r.m[k], p.m[k]))
            ayni = 0;
    CHECK(ayni, "M * I == M");
}

static void test_transpoze(void)
{
    Mat4 a = mat4_perspective(1.0f, 1.5f, 0.5f, 500.0f);
    Mat4 t = mat4_transpose(&a);
    Mat4 tt = mat4_transpose(&t);
    int k, ayni = 1;

    printf("test: transpoze'nin transpozesi kendisi\n");
    for (k = 0; k < 16; k++)
        if (!CLOSE(tt.m[k], a.m[k]))
            ayni = 0;
    CHECK(ayni, "transpose(transpose(M)) == M");
    CHECK(CLOSE(t.m[1 * 4 + 0], a.m[0 * 4 + 1]), "satir/sutun gercekten yer degistirdi");
}

static void test_perspektif(void)
{
    float fovy = 45.0f * 3.14159265f / 180.0f;
    float aspect = 16.0f / 9.0f;
    float znear = 1.0f, zfar = 3000.0f;
    Mat4 p = mat4_perspective(fovy, aspect, znear, zfar);
    float f = 1.0f / tanf(fovy * 0.5f);
    float yakin[4] = { 0.0f, 0.0f, -1.0f, 1.0f };   /* yakin duzlem uzerinde */
    float uzak[4]  = { 0.0f, 0.0f, -3000.0f, 1.0f };
    float o1[4], o2[4];

    printf("test: perspektif matrisi bilinen degerleri uretiyor\n");
    CHECK(CLOSE(p.m[0], f / aspect), "m00 = f/aspect");
    CHECK(CLOSE(p.m[5], f), "m11 = f");
    CHECK(CLOSE(p.m[14], -1.0f), "m32 = -1 (w = -z)");

    mat4_transform(&p, yakin, o1);
    mat4_transform(&p, uzak, o2);

    /* NDC z: yakin duzlem -1, uzak duzlem +1 */
    CHECK(CLOSE(o1[2] / o1[3], -1.0f), "yakin duzlem NDC z = -1");
    CHECK(CLOSE(o2[2] / o2[3], 1.0f), "uzak duzlem NDC z = +1");
    CHECK(o1[3] > 0.0f, "onumuzdeki nokta pozitif w veriyor");
}

static void test_lookat_ortonormal(void)
{
    float eye[3] = { 3.0f, 20.0f, -7.0f };
    float target[3] = { 40.0f, 5.0f, 60.0f };
    float up[3] = { 0.0f, 1.0f, 0.0f };
    Mat4 v = mat4_look_at(eye, target, up);
    float x[3], y[3], z[3];

    printf("test: look_at ortonormal bir taban uretiyor\n");
    vec3_set(x, v.m[0], v.m[1], v.m[2]);
    vec3_set(y, v.m[4], v.m[5], v.m[6]);
    vec3_set(z, v.m[8], v.m[9], v.m[10]);

    CHECK(CLOSE(vec3_dot(x, x), 1.0f), "x ekseni birim uzunlukta");
    CHECK(CLOSE(vec3_dot(y, y), 1.0f), "y ekseni birim uzunlukta");
    CHECK(CLOSE(vec3_dot(z, z), 1.0f), "z ekseni birim uzunlukta");
    CHECK(CLOSE(vec3_dot(x, y), 0.0f), "x ile y dik");
    CHECK(CLOSE(vec3_dot(x, z), 0.0f), "x ile z dik");
    CHECK(CLOSE(vec3_dot(y, z), 0.0f), "y ile z dik");
}

static void test_lookat_kamerayi_merkeze_alir(void)
{
    float eye[3] = { 10.0f, 5.0f, 12.0f };
    float target[3] = { 0.0f, 0.0f, 0.0f };
    float up[3] = { 0.0f, 1.0f, 0.0f };
    Mat4 v = mat4_look_at(eye, target, up);
    float p[4] = { 10.0f, 5.0f, 12.0f, 1.0f };
    float o[4];

    printf("test: look_at kamera konumunu orijine tasiyor\n");
    mat4_transform(&v, p, o);

    CHECK(CLOSE(o[0], 0.0f) && CLOSE(o[1], 0.0f) && CLOSE(o[2], 0.0f),
          "kameranin kendi konumu goruntu uzayinda (0,0,0)");
}

static void test_hedef_onumuzde(void)
{
    float eye[3] = { 0.0f, 10.0f, 0.0f };
    float target[3] = { 0.0f, 10.0f, -50.0f };
    float up[3] = { 0.0f, 1.0f, 0.0f };
    Mat4 v = mat4_look_at(eye, target, up);
    float p[4] = { 0.0f, 10.0f, -50.0f, 1.0f };
    float o[4];

    printf("test: baktigimiz nokta goruntu uzayinda onumuzde (-z)\n");
    mat4_transform(&v, p, o);
    CHECK(o[2] < 0.0f, "hedef negatif z'de, yani kameranin onunde");
}

static void test_kamera_baslangic(void)
{
    Camera c;

    printf("test: kamera suyun ustunde ve ufka bakarak basliyor\n");
    camera_init(&c);
    CHECK(c.pos[1] > WATER_LEVEL + MIN_ALTITUDE, "baslangicta su seviyesinin ustunde");
    CHECK(CLOSE(c.pitch, 0.0f), "baslangicta ufka bakiyor");
}

static void test_pitch_sinirli(void)
{
    Camera c;
    int i;

    printf("test: pitch +-85 derecede kirpiliyor (tepetaklak olunmuyor)\n");
    camera_init(&c);
    for (i = 0; i < 600; i++)
        camera_update(&c, 0, 0, 0, 0, 1.0f, 1.0f / 60.0f);
    CHECK(c.pitch <= MAX_PITCH + 1e-4f, "yukari bakis sinirlandi");

    for (i = 0; i < 1200; i++)
        camera_update(&c, 0, 0, 0, 0, -1.0f, 1.0f / 60.0f);
    CHECK(c.pitch >= -MAX_PITCH - 1e-4f, "asagi bakis sinirlandi");
}

static void test_suya_girilemiyor(void)
{
    Camera c;
    int i;
    int battı = 0;

    printf("test: hicbir girdi kombinasyonu kamerayi suyun altina indiremiyor\n");
    camera_init(&c);

    /* asagi bak + tam gaz ileri + surekli alcal: en kotu durum */
    for (i = 0; i < 20000; i++) {
        camera_update(&c, 1.0f, 0.0f, -1.0f, 0.0f, -1.0f, 1.0f / 60.0f);
        if (c.pos[1] < WATER_LEVEL + MIN_ALTITUDE - 1e-4f)
            battı = 1;
    }
    CHECK(!battı, "kamera hicbir karede su seviyesinin altina inmedi");
    CHECK(CLOSE(c.pos[1], WATER_LEVEL + MIN_ALTITUDE), "su yuzeyinin hemen ustunde duruyor");
}

static void test_ileri_hareket_bakis_yonunde(void)
{
    Camera c;
    float once[3], fwd[3];

    printf("test: ileri hareket bakis yonuyle tutarli\n");
    camera_init(&c);
    memcpy(once, c.pos, sizeof(once));
    camera_forward(&c, fwd);

    camera_update(&c, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f / 60.0f);

    /* yaw=0, pitch=0 iken ileri = -z */
    CHECK(c.pos[2] < once[2], "yaw=0 iken ileri gitmek -z yonunde");
    CHECK(CLOSE(c.pos[0], once[0]), "yana kaymadi");

    /* 90 derece saga don, sonra ileri git: +x yonune gitmeli */
    camera_init(&c);
    c.yaw = 1.5707963f;
    memcpy(once, c.pos, sizeof(once));
    camera_update(&c, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f / 60.0f);
    CHECK(c.pos[0] > once[0], "90 derece donunce ileri = +x");
}

static void test_yana_kayma_dik(void)
{
    Camera c;
    float once[3];

    printf("test: yana kayma bakis yonune dik\n");
    camera_init(&c);
    memcpy(once, c.pos, sizeof(once));

    camera_update(&c, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f / 60.0f);

    CHECK(c.pos[0] > once[0], "yaw=0 iken saga kayma +x yonunde");
    CHECK(CLOSE(c.pos[2], once[2]), "ileri/geri konum degismedi");
    CHECK(CLOSE(c.pos[1], once[1]), "yukseklik degismedi");
}

static void test_yaw_sarmasi(void)
{
    Camera c;
    int i;

    printf("test: uzun sure donunce yaw makul aralikta kaliyor\n");
    camera_init(&c);
    for (i = 0; i < 100000; i++)
        camera_update(&c, 0, 0, 0, 1.0f, 0, 1.0f / 60.0f);

    CHECK(c.yaw >= -3.15f && c.yaw <= 3.15f, "yaw -pi..pi araliginda sarmali");
}

int main(void)
{
    test_birim_matris();
    test_transpoze();
    test_perspektif();
    test_lookat_ortonormal();
    test_lookat_kamerayi_merkeze_alir();
    test_hedef_onumuzde();
    test_kamera_baslangic();
    test_pitch_sinirli();
    test_suya_girilemiyor();
    test_ileri_hareket_bakis_yonunde();
    test_yana_kayma_dik();
    test_yaw_sarmasi();

    printf("\n%d kontrol, %d hata\n", checks, failures);
    return failures ? 1 : 0;
}
