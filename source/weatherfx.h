#ifndef WEATHERFX_H
#define WEATHERFX_H

#include "atmosphere.h"

/* On-screen weather effects: rain streaks and lightning flashes.
 * Drawn on the 2D overlay layer; they grow with the atmosphere's
 * rain/lightning values. */

void weatherfx_draw(const Atmosphere *atm, float time);

#endif
