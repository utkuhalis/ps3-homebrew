#ifndef MAT4_H
#define MAT4_H

/* 4x4 matrix maths. Hardware independent, unit tested.
 *
 * Storage: ROW-MAJOR, m[row * 4 + column].
 * Vectors are column vectors: v' = M * v.
 *
 * Note: this array goes to the RSX shader constants as is. The PSL1GHT
 * samples use Vectormath (column-major storage) and transpose before
 * handing it to RSX; transposed column-major = row-major, so our layout is
 * already the right one and no extra transpose is needed. */

typedef struct {
    float m[16];
} Mat4;

Mat4 mat4_identity(void);
Mat4 mat4_mul(const Mat4 *a, const Mat4 *b);          /* a * b */
Mat4 mat4_transpose(const Mat4 *a);
Mat4 mat4_perspective(float fovy_rad, float aspect, float znear, float zfar);
Mat4 mat4_look_at(const float eye[3], const float target[3], const float up[3]);
Mat4 mat4_translation(float x, float y, float z);

void mat4_transform(const Mat4 *a, const float v[4], float out[4]);

/* vector helpers */
void  vec3_set(float out[3], float x, float y, float z);
void  vec3_add_scaled(float out[3], const float v[3], float s);
void  vec3_sub(float out[3], const float a[3], const float b[3]);
void  vec3_cross(float out[3], const float a[3], const float b[3]);
float vec3_dot(const float a[3], const float b[3]);
void  vec3_normalize(float v[3]);

#endif
