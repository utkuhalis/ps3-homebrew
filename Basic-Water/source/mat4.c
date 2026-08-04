#include <math.h>
#include <string.h>

#include "mat4.h"

Mat4 mat4_identity(void)
{
    Mat4 r;
    int i;

    memset(&r, 0, sizeof(r));
    for (i = 0; i < 4; i++)
        r.m[i * 4 + i] = 1.0f;
    return r;
}

Mat4 mat4_mul(const Mat4 *a, const Mat4 *b)
{
    Mat4 r;
    int i, j, k;

    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            float s = 0.0f;
            for (k = 0; k < 4; k++)
                s += a->m[i * 4 + k] * b->m[k * 4 + j];
            r.m[i * 4 + j] = s;
        }
    }
    return r;
}

Mat4 mat4_transpose(const Mat4 *a)
{
    Mat4 r;
    int i, j;

    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            r.m[i * 4 + j] = a->m[j * 4 + i];
    return r;
}

Mat4 mat4_translation(float x, float y, float z)
{
    Mat4 r = mat4_identity();

    r.m[0 * 4 + 3] = x;
    r.m[1 * 4 + 3] = y;
    r.m[2 * 4 + 3] = z;
    return r;
}

Mat4 mat4_perspective(float fovy_rad, float aspect, float znear, float zfar)
{
    Mat4 r;
    float f = 1.0f / tanf(fovy_rad * 0.5f);

    memset(&r, 0, sizeof(r));
    r.m[0 * 4 + 0] = f / aspect;
    r.m[1 * 4 + 1] = f;
    r.m[2 * 4 + 2] = (zfar + znear) / (znear - zfar);
    r.m[2 * 4 + 3] = (2.0f * zfar * znear) / (znear - zfar);
    r.m[3 * 4 + 2] = -1.0f;
    return r;
}

Mat4 mat4_look_at(const float eye[3], const float target[3], const float up[3])
{
    Mat4 r;
    float zax[3], xax[3], yax[3];

    /* z ekseni kameranin ARKASINA bakar (OpenGL konvansiyonu) */
    vec3_sub(zax, eye, target);
    vec3_normalize(zax);

    vec3_cross(xax, up, zax);
    vec3_normalize(xax);

    vec3_cross(yax, zax, xax);

    r.m[0]  = xax[0];  r.m[1]  = xax[1];  r.m[2]  = xax[2];  r.m[3]  = -vec3_dot(xax, eye);
    r.m[4]  = yax[0];  r.m[5]  = yax[1];  r.m[6]  = yax[2];  r.m[7]  = -vec3_dot(yax, eye);
    r.m[8]  = zax[0];  r.m[9]  = zax[1];  r.m[10] = zax[2];  r.m[11] = -vec3_dot(zax, eye);
    r.m[12] = 0.0f;    r.m[13] = 0.0f;    r.m[14] = 0.0f;    r.m[15] = 1.0f;

    return r;
}

void mat4_transform(const Mat4 *a, const float v[4], float out[4])
{
    int i, j;

    for (i = 0; i < 4; i++) {
        float s = 0.0f;
        for (j = 0; j < 4; j++)
            s += a->m[i * 4 + j] * v[j];
        out[i] = s;
    }
}

void vec3_set(float out[3], float x, float y, float z)
{
    out[0] = x;
    out[1] = y;
    out[2] = z;
}

void vec3_add_scaled(float out[3], const float v[3], float s)
{
    out[0] += v[0] * s;
    out[1] += v[1] * s;
    out[2] += v[2] * s;
}

void vec3_sub(float out[3], const float a[3], const float b[3])
{
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
    out[2] = a[2] - b[2];
}

void vec3_cross(float out[3], const float a[3], const float b[3])
{
    float x = a[1] * b[2] - a[2] * b[1];
    float y = a[2] * b[0] - a[0] * b[2];
    float z = a[0] * b[1] - a[1] * b[0];

    out[0] = x;
    out[1] = y;
    out[2] = z;
}

float vec3_dot(const float a[3], const float b[3])
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

void vec3_normalize(float v[3])
{
    float len = sqrtf(vec3_dot(v, v));

    if (len > 1e-6f) {
        v[0] /= len;
        v[1] /= len;
        v[2] /= len;
    }
}
