/* Host-side unit tests for mat4 and camera (no PS3 needed).
 * Run: ./build.sh test */
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

    printf("test: multiplying by identity changes nothing\n");
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

    printf("test: the transpose of the transpose is itself\n");
    for (k = 0; k < 16; k++)
        if (!CLOSE(tt.m[k], a.m[k]))
            ayni = 0;
    CHECK(ayni, "transpose(transpose(M)) == M");
    CHECK(CLOSE(t.m[1 * 4 + 0], a.m[0 * 4 + 1]), "rows/columns really swapped");
}

static void test_perspektif(void)
{
    float fovy = 45.0f * 3.14159265f / 180.0f;
    float aspect = 16.0f / 9.0f;
    float znear = 1.0f, zfar = 3000.0f;
    Mat4 p = mat4_perspective(fovy, aspect, znear, zfar);
    float f = 1.0f / tanf(fovy * 0.5f);
    float yakin[4] = { 0.0f, 0.0f, -1.0f, 1.0f };   /* on the near plane */
    float uzak[4]  = { 0.0f, 0.0f, -3000.0f, 1.0f };
    float o1[4], o2[4];

    printf("test: the perspective matrix produces known values\n");
    CHECK(CLOSE(p.m[0], f / aspect), "m00 = f/aspect");
    CHECK(CLOSE(p.m[5], f), "m11 = f");
    CHECK(CLOSE(p.m[14], -1.0f), "m32 = -1 (w = -z)");

    mat4_transform(&p, yakin, o1);
    mat4_transform(&p, uzak, o2);

    /* NDC z: near plane -1, far plane +1 */
    CHECK(CLOSE(o1[2] / o1[3], -1.0f), "near plane NDC z = -1");
    CHECK(CLOSE(o2[2] / o2[3], 1.0f), "far plane NDC z = +1");
    CHECK(o1[3] > 0.0f, "a point in front gives positive w");
}

static void test_lookat_ortonormal(void)
{
    float eye[3] = { 3.0f, 20.0f, -7.0f };
    float target[3] = { 40.0f, 5.0f, 60.0f };
    float up[3] = { 0.0f, 1.0f, 0.0f };
    Mat4 v = mat4_look_at(eye, target, up);
    float x[3], y[3], z[3];

    printf("test: look_at produces an orthonormal basis\n");
    vec3_set(x, v.m[0], v.m[1], v.m[2]);
    vec3_set(y, v.m[4], v.m[5], v.m[6]);
    vec3_set(z, v.m[8], v.m[9], v.m[10]);

    CHECK(CLOSE(vec3_dot(x, x), 1.0f), "x axis has unit length");
    CHECK(CLOSE(vec3_dot(y, y), 1.0f), "y axis has unit length");
    CHECK(CLOSE(vec3_dot(z, z), 1.0f), "z axis has unit length");
    CHECK(CLOSE(vec3_dot(x, y), 0.0f), "x and y perpendicular");
    CHECK(CLOSE(vec3_dot(x, z), 0.0f), "x and z perpendicular");
    CHECK(CLOSE(vec3_dot(y, z), 0.0f), "y and z perpendicular");
}

static void test_lookat_kamerayi_merkeze_alir(void)
{
    float eye[3] = { 10.0f, 5.0f, 12.0f };
    float target[3] = { 0.0f, 0.0f, 0.0f };
    float up[3] = { 0.0f, 1.0f, 0.0f };
    Mat4 v = mat4_look_at(eye, target, up);
    float p[4] = { 10.0f, 5.0f, 12.0f, 1.0f };
    float o[4];

    printf("test: look_at moves the camera position to the origin\n");
    mat4_transform(&v, p, o);

    CHECK(CLOSE(o[0], 0.0f) && CLOSE(o[1], 0.0f) && CLOSE(o[2], 0.0f),
          "the camera's own position is (0,0,0) in view space");
}

static void test_hedef_onumuzde(void)
{
    float eye[3] = { 0.0f, 10.0f, 0.0f };
    float target[3] = { 0.0f, 10.0f, -50.0f };
    float up[3] = { 0.0f, 1.0f, 0.0f };
    Mat4 v = mat4_look_at(eye, target, up);
    float p[4] = { 0.0f, 10.0f, -50.0f, 1.0f };
    float o[4];

    printf("test: the target point is in front of us in view space (-z)\n");
    mat4_transform(&v, p, o);
    CHECK(o[2] < 0.0f, "target at negative z, i.e. in front of the camera");
}

static void test_kamera_baslangic(void)
{
    Camera c;

    printf("test: the camera starts above the water looking at the horizon\n");
    camera_init(&c);
    CHECK(c.pos[1] > WATER_LEVEL + MIN_ALTITUDE, "starts above water level");
    CHECK(CLOSE(c.pitch, 0.0f), "starts looking at the horizon");
}

static void test_pitch_sinirli(void)
{
    Camera c;
    int i;

    printf("test: pitch clipped at +-85 degrees (no flipping over)\n");
    camera_init(&c);
    for (i = 0; i < 600; i++)
        camera_update(&c, 0, 0, 0, 0, 1.0f, 1.0f / 60.0f);
    CHECK(c.pitch <= MAX_PITCH + 1e-4f, "looking up limited");

    for (i = 0; i < 1200; i++)
        camera_update(&c, 0, 0, 0, 0, -1.0f, 1.0f / 60.0f);
    CHECK(c.pitch >= -MAX_PITCH - 1e-4f, "looking down limited");
}

static void test_suya_girilemiyor(void)
{
    Camera c;
    int i;
    int battı = 0;

    printf("test: no input combination can put the camera under water\n");
    camera_init(&c);

    /* look down + full forward + constant descent: the worst case */
    for (i = 0; i < 20000; i++) {
        camera_update(&c, 1.0f, 0.0f, -1.0f, 0.0f, -1.0f, 1.0f / 60.0f);
        if (c.pos[1] < WATER_LEVEL + MIN_ALTITUDE - 1e-4f)
            battı = 1;
    }
    CHECK(!battı, "camera never went below water level in any frame");
    CHECK(CLOSE(c.pos[1], WATER_LEVEL + MIN_ALTITUDE), "sits just above the water surface");
}

static void test_ileri_hareket_bakis_yonunde(void)
{
    Camera c;
    float once[3], fwd[3];

    printf("test: forward movement agrees with the view direction\n");
    camera_init(&c);
    memcpy(once, c.pos, sizeof(once));
    camera_forward(&c, fwd);

    camera_update(&c, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f / 60.0f);

    /* with yaw=0, pitch=0 forward = -z */
    CHECK(c.pos[2] < once[2], "with yaw=0 going forward is towards -z");
    CHECK(CLOSE(c.pos[0], once[0]), "did not slide sideways");

    /* turn 90 degrees right, then go forward: should move towards +x */
    camera_init(&c);
    c.yaw = 1.5707963f;
    memcpy(once, c.pos, sizeof(once));
    camera_update(&c, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f / 60.0f);
    CHECK(c.pos[0] > once[0], "after a 90 degree turn forward = +x");
}

static void test_yana_kayma_dik(void)
{
    Camera c;
    float once[3];

    printf("test: strafing is perpendicular to the view direction\n");
    camera_init(&c);
    memcpy(once, c.pos, sizeof(once));

    camera_update(&c, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f / 60.0f);

    CHECK(c.pos[0] > once[0], "with yaw=0 strafing right is towards +x");
    CHECK(CLOSE(c.pos[2], once[2]), "forward/back position unchanged");
    CHECK(CLOSE(c.pos[1], once[1]), "height unchanged");
}

static void test_yaw_sarmasi(void)
{
    Camera c;
    int i;

    printf("test: yaw stays in a sensible range after long turning\n");
    camera_init(&c);
    for (i = 0; i < 100000; i++)
        camera_update(&c, 0, 0, 0, 1.0f, 0, 1.0f / 60.0f);

    CHECK(c.yaw >= -3.15f && c.yaw <= 3.15f, "yaw wrapped into -pi..pi");
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

    printf("\n%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
