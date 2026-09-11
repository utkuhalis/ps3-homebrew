#ifndef WAYPOINT_H
#define WAYPOINT_H

#include "camera.h"
#include "mat4.h"

/* Markers that show target points on screen.
 *
 * The world position goes through the view-projection matrix to screen
 * coordinates. If the target is off screen the marker is pinned to the
 * edge and shows an arrow, so the player knows which way to turn. */

typedef struct {
    float sx, sy;       /* screen position (virtual 1280x720) */
    float distance;     /* distance from the camera */
    int   on_screen;    /* 1: target in view */
    int   behind;       /* 1: target behind the camera */
    float edge_angle;   /* arrow direction when off screen (radians) */
} WaypointView;

/* Converts a world point to screen information. vp: proj * view */
void waypoint_project(const Mat4 *vp, const Camera *cam, const float world[3],
                      WaypointView *out);

/* Draws all runway targets */
void waypoint_draw_all(const Camera *cam, const Mat4 *proj);

#endif
