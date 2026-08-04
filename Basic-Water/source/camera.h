#ifndef CAMERA_H
#define CAMERA_H

#include "mat4.h"

/* Serbest ucus kamerasi. Donanim bagimsiz, birim testli.
 * Girdiler -1..+1 araliginda normalize gelir; bu modul tus eslemesini bilmez. */

#define WATER_LEVEL     0.0f    /* deniz duzlemi y = 0 */
#define MIN_ALTITUDE    2.0f    /* su yuzeyinin ustunde kalinacak en az mesafe */
#define MAX_PITCH       1.4835f /* ~85 derece, radyan */
#define MOVE_SPEED     40.0f    /* birim / saniye */
#define VERT_SPEED     25.0f
#define LOOK_SPEED      1.8f    /* radyan / saniye */

typedef struct {
    float pos[3];
    float yaw;      /* radyan; 0 = -Z yonune bakar */
    float pitch;    /* radyan; + yukari */
} Camera;

void camera_init(Camera *c);

/* forward/strafe/updown/yaw_in/pitch_in: -1..+1, dt: saniye */
void camera_update(Camera *c, float forward, float strafe, float updown,
                   float yaw_in, float pitch_in, float dt);

void camera_forward(const Camera *c, float out[3]);
void camera_right(const Camera *c, float out[3]);
Mat4 camera_view_matrix(const Camera *c);

#endif
