#ifndef MINIMAP_H
#define MINIMAP_H

#include "camera.h"
#include "flight.h"

/* Bottom-left mini map: route, aircraft position and heading, remaining
 * distance. The departure and arrival points are the runway positions. */

void minimap_draw(const Flight *f);

#endif
