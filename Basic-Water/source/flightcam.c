#include <math.h>

#include "flightcam.h"
#include "aircraft.h"

const char *flightcam_name(CamMode m)
{
    switch (m) {
    case CAM_CHASE:   return "Takip";
    case CAM_COCKPIT: return "Kokpit";
    case CAM_FREE:    return "Serbest";
    default:          return "?";
    }
}

void flightcam_update(Camera *cam, CamMode mode, const Flight *f, float dt)
{
    float target[3];
    int i;

    if (mode == CAM_FREE)
        return;                 /* serbest modda kamera kendi kontrolunde */

    if (mode == CAM_COCKPIT) {
        aircraft_cockpit_pos(f, target);
        for (i = 0; i < 3; i++)
            cam->pos[i] = target[i];

        /* pilot gozunde kamera ucakla birebir ayni yone bakar */
        cam->yaw = f->yaw;
        cam->pitch = f->pitch;
        return;
    }

    /* takip modu: hedefe yumusak yaklasim, boylece manevrada kamera
     * ucagin arkasinda savrulur ve hiz hissi olusur */
    aircraft_chase_pos(f, target);
    {
        float k = dt * 6.0f;

        if (k > 1.0f)
            k = 1.0f;

        for (i = 0; i < 3; i++)
            cam->pos[i] += (target[i] - cam->pos[i]) * k;

        /* bakis acisi da yumusak takip eder */
        {
            float dyaw = f->yaw - cam->yaw;

            while (dyaw >  3.14159265f) dyaw -= 6.28318531f;
            while (dyaw < -3.14159265f) dyaw += 6.28318531f;

            cam->yaw += dyaw * k;
            cam->pitch += (f->pitch * 0.6f - cam->pitch) * k;
        }
    }
}
