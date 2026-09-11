#ifndef WAVES_H
#define WAVES_H

/* Sea surface waves: the sum of a few directional sine waves.
 *
 * IMPORTANT: the same formula is also in shaders/water.vcg (Cg has no
 * include). When one changes, the other must be updated too.
 *
 * Hardware independent: used in the unit tests and the host preview. */

#define WAVE_MAX_HEIGHT 2.6f   /* upper limit of a wave crest (units) */

/* Wave height at point (x, z) (relative to sea level) */
float wave_height(float x, float z, float time);

/* Surface normal at the same point (unit vector). The accuracy of glitter
 * and reflection depends on it. */
void  wave_normal(float x, float z, float time, float out[3]);

#endif
