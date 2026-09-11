#ifndef GAUGES_H
#define GAUGES_H

/* Analogue flight instruments: speed, artificial horizon, altitude.
 *
 * The value -> needle angle conversion is pure maths in separate
 * functions, so it can be tested independently of drawing. */

/* Value ranges (the values covered by the dials) */
#define SPEED_MAX_KMH   400.0f
#define ALT_MAX_M      4000.0f

/* Needle angle (radians). The 6 o'clock position is the start, increasing
 * clockwise. Values outside the range are pinned to the ends. */
float gauge_speed_angle(float kmh);
float gauge_alt_angle(float meters);

/* Draws the gauges in the bottom right of the screen */
void gauges_draw(float speed_kmh, float altitude_m, float pitch_rad,
                 float roll_rad);

/* Draws the artificial horizon at a given position (exposed for preview and tests) */
void gauges_draw_attitude(float cx, float cy, float r, float pitch, float roll);

/* Artificial horizon helpers - pure, unit tested */
float gauge_horizon_dist(float dx, float dy, float roll, float shift);
float gauge_horizon_shift(float pitch, float r);

#endif
