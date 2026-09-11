#ifndef ATMOSPHERE_H
#define ATMOSPHERE_H

#include "gamemenu.h"

/* Turns weather and time of day into scene parameters.
 *
 * Hardware independent (unit tested). The values are sent to the sky and
 * water shaders as uniforms, so the whole atmosphere of the scene is set
 * from one place. */

typedef struct {
    float sun_dir[3];       /* unit vector; used as the moon direction at night */
    float sun_color[3];     /* colour of the sun/moon light */
    float horizon[3];       /* horizon colour */
    float zenith[3];        /* zenith colour */
    float cloud_low;        /* lower cloud threshold (lower = more cloud) */
    float cloud_high;       /* upper cloud threshold */
    float cloud_bright;     /* cloud brightness (dark in a storm) */
    float fog_distance;     /* fog distance (units) */
    float wave_scale;       /* wave amplitude multiplier */
    float wave_length;      /* wavelength multiplier: if the waves grow taller
                             * without growing longer, the crests do not fit
                             * the grid and look faceted (low-poly) */
    float water_dark;       /* darkening multiplier for the water's base colour */
    float rain;             /* 0..1 rain intensity */
    float lightning;        /* 0..1 lightning probability */
} Atmosphere;

void atmosphere_compute(Atmosphere *a, Weather w, TimeOfDay t);

#endif
