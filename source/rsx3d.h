#ifndef RSX3D_H
#define RSX3D_H

#include <ppu-types.h>
#include <rsx/rsx.h>

/* RSX 3D layer: display setup, depth buffer, draw state, flip.
 * The context and dimensions come from here so the layers above (scene)
 * do not need to know gcm details. */

int   rsx3d_init(void);
void  rsx3d_exit(void);

/* Start of frame: bind the render target, clear the screen, set up draw state. */
void  rsx3d_begin_frame(u32 clear_color);
void  rsx3d_end_frame(void);

float rsx3d_aspect(void);
gcmContextData *rsx3d_context(void);

/* Writes a screenshot as a PPM (diagnostics) */
int rsx3d_capture(const char *path);

/* Backface culling for solid geometry (aircraft, runway) */
void rsx3d_set_culling(int enable);

/* Screen dimensions (the 2D overlay needs them for scaling) */
u32  rsx3d_width(void);
u32  rsx3d_height(void);

#endif
