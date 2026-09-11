#ifndef SKYCOLOR_H
#define SKYCOLOR_H

/* Sky colour: gradient by view direction + sun + clouds.
 * No textures; everything is computed.
 *
 * IMPORTANT: identical copies of these formulas are in shaders/sky.fcg and
 * shaders/water.fcg (Cg has no include). When one changes, the others
 * must be updated too; otherwise the sky reflected in the water and the
 * real sky disagree.
 *
 * This file is hardware independent: used directly by the unit tests and
 * the host preview. */

/* Sun direction (unit vector) and colour - must match both shaders */
extern const float SUN_DIR[3];

/* dir: unit view vector, time: seconds. out: RGB in 0..1 */
void sky_color(const float dir[3], float time, float out[3]);

/* Cloud density only (0..1) - exposed separately for tests and tuning */
float sky_clouds(const float dir[3], float time);

#endif
