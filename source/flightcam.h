#ifndef FLIGHTCAM_H
#define FLIGHTCAM_H

#include "camera.h"
#include "flight.h"

/* Camera modes. The existing Camera struct is kept; in the flight modes
 * position and angles are derived from the aircraft, so scene drawing does
 * not change at all.
 *
 * The outside cameras other than the cockpit LOOK AT the aircraft. In an
 * earlier version the camera merely turned the same way as the aircraft;
 * in turns the angle lagged behind and the aircraft slid off the screen. */

typedef enum {
    CAM_CHASE = 0,      /* above and behind, chase view */
    CAM_COCKPIT,        /* pilot's eye */
    CAM_LEFT_WING,      /* from the left wing tip */
    CAM_RIGHT_WING,     /* from the right wing tip */
    CAM_TAIL,           /* behind the tail, far */
    CAM_FREE,           /* free roam */
    CAM_MODE_COUNT
} CamMode;

const char *flightcam_name(CamMode m);

/* Orbit camera: for circling around the aircraft.
 *
 * In the chase and tail modes the left stick orbits the camera around the
 * aircraft, so its size and shape can be seen. The angles persist - they
 * do not reset on release, and opening and closing the menu with SELECT
 * does not disturb them. */
void flightcam_orbit(float dyaw, float dpitch, float dzoom, float dt);

/* Resets the orbit angles to their defaults (on camera mode change) */
void flightcam_orbit_reset(void);

/* For the HUD: have the orbit angles moved away from the default */
int  flightcam_orbit_active(void);

/* Updates the camera for the current flight mode. dt is needed for smooth following. */
void flightcam_update(Camera *cam, CamMode mode, const Flight *f, float dt);

/* Pushes the camera out of obstacles.
 *
 * Two rules: the camera cannot enter the aircraft body (it stays outside
 * the bounding sphere) and cannot go below the ground (the runway surface
 * over the runway, the sea elsewhere). Pure function - unit tested.
 *
 * plane_radius: sphere enclosing the body. ground_y: the ground at that point. */
void flightcam_clamp(Camera *cam, const float plane_pos[3],
                     float plane_radius, float ground_y);

/* Ground height at the given world point (runway surface or sea) */
float flightcam_ground_at(const Flight *f, float wx, float wz);

/* Computes the yaw/pitch that look from one point at a target.
 * Hardware independent - unit tested. */
void flightcam_look_at(const float eye[3], const float target[3],
                       float *yaw, float *pitch);

#endif
