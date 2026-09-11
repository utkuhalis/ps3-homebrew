#ifndef CAMERA_H
#define CAMERA_H

#include "mat4.h"

/* Free-flight camera. Hardware independent, unit tested.
 * Inputs arrive normalised to -1..+1; this module knows nothing about
 * button mapping. */

#define WATER_LEVEL     0.0f    /* sea plane y = 0 */
#define MIN_ALTITUDE    2.0f    /* minimum distance to keep above the water */
#define MAX_PITCH       1.4835f /* ~85 degrees, radians */
#define MOVE_SPEED     40.0f    /* units / second */
#define VERT_SPEED     25.0f
#define LOOK_SPEED      1.8f    /* radians / second */

typedef struct {
    float pos[3];
    float yaw;      /* radians; 0 = looking towards -Z */
    float pitch;    /* radians; + is up */
} Camera;

void camera_init(Camera *c);

/* forward/strafe/updown/yaw_in/pitch_in: -1..+1, dt: seconds */
void camera_update(Camera *c, float forward, float strafe, float updown,
                   float yaw_in, float pitch_in, float dt);

void camera_forward(const Camera *c, float out[3]);
void camera_right(const Camera *c, float out[3]);
Mat4 camera_view_matrix(const Camera *c);

#endif
