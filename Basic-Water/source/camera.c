#include <math.h>

#include "camera.h"

void camera_init(Camera *c)
{
    /* kalkis pistinin biraz uzerinde ve gerisinde basla */
    vec3_set(c->pos, -1500.0f, 60.0f, 1600.0f);
    c->yaw = 0.0f;
    c->pitch = 0.0f;
}

void camera_forward(const Camera *c, float out[3])
{
    float cp = cosf(c->pitch);

    out[0] =  cp * sinf(c->yaw);
    out[1] =  sinf(c->pitch);
    out[2] = -cp * cosf(c->yaw);
}

void camera_right(const Camera *c, float out[3])
{
    out[0] = cosf(c->yaw);
    out[1] = 0.0f;
    out[2] = sinf(c->yaw);
}

void camera_update(Camera *c, float forward, float strafe, float updown,
                   float yaw_in, float pitch_in, float dt)
{
    float fwd[3], right[3];

    /* bakis */
    c->yaw += yaw_in * LOOK_SPEED * dt;
    c->pitch += pitch_in * LOOK_SPEED * dt;

    if (c->pitch > MAX_PITCH)
        c->pitch = MAX_PITCH;
    else if (c->pitch < -MAX_PITCH)
        c->pitch = -MAX_PITCH;

    /* yaw'i -pi..pi araliginda tut (uzun ucuslarda hassasiyet kaybini onler) */
    while (c->yaw > 3.14159265f)
        c->yaw -= 6.28318531f;
    while (c->yaw < -3.14159265f)
        c->yaw += 6.28318531f;

    /* hareket */
    camera_forward(c, fwd);
    camera_right(c, right);

    vec3_add_scaled(c->pos, fwd, forward * MOVE_SPEED * dt);
    vec3_add_scaled(c->pos, right, strafe * MOVE_SPEED * dt);
    c->pos[1] += updown * VERT_SPEED * dt;

    /* su yuzeyi carpismasi: asla suyun altina inilemez */
    if (c->pos[1] < WATER_LEVEL + MIN_ALTITUDE)
        c->pos[1] = WATER_LEVEL + MIN_ALTITUDE;
}

Mat4 camera_view_matrix(const Camera *c)
{
    float fwd[3], target[3];
    float up[3];

    camera_forward(c, fwd);
    target[0] = c->pos[0] + fwd[0];
    target[1] = c->pos[1] + fwd[1];
    target[2] = c->pos[2] + fwd[2];

    vec3_set(up, 0.0f, 1.0f, 0.0f);

    return mat4_look_at(c->pos, target, up);
}
