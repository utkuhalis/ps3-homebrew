#ifndef RUNWAY_H
#define RUNWAY_H

#include "camera.h"
#include "mat4.h"
#include "atmosphere.h"

/* The two runway platforms on the sea: departure and arrival.
 * Their positions are defined here, and the mini map, objectives and
 * waypoint markers all use the same values. */

#define RUNWAY_COUNT 2

/* World coordinates (x, z) and platform height */
extern const float RUNWAY_POS[RUNWAY_COUNT][2];
#define RUNWAY_DECK_Y   9.0f    /* the platform surface is above sea level */
/* A 737-800 takeoff run is ~1150 m; a 1100 m runway was borderline.
 * Raised to the size of a real commercial runway. */
#define RUNWAY_LENGTH 2600.0f
#define RUNWAY_WIDTH   60.0f    /* real runways are ~45-60 m wide */

const char *runway_name(int index);

int  runway_init(void);
void runway_draw(const Camera *cam, const Mat4 *proj, const Atmosphere *atm);

#endif
