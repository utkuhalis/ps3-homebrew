#ifndef OVERLAY_H
#define OVERLAY_H

/* 2D drawing over the 3D scene (HUD, menu).
 * Coordinates are in a virtual 1280x720 space; the module scales to the
 * real resolution. Colours are 0xRRGGBB. */

typedef unsigned int color_t;

#define OVL_W 1280
#define OVL_H 720

#define RGB(r, g, b) (((color_t)(r) << 16) | ((color_t)(g) << 8) | (color_t)(b))

/* Once, at program start */
int  overlay_init(void);

/* Start of frame: clears the collected primitives */
void overlay_begin(void);

/* Sends everything collected to the GPU in one draw call.
 * Called at the end of the frame, after the 3D scene. */
void overlay_flush(void);

/* Number of primitives collected this frame (for the profiler) */
unsigned int overlay_rect_count(void);

/* Did the draw buffer fill up this frame (diagnostics) */
int  overlay_overflowed(void);

void overlay_fill_rect(int x, int y, int w, int h, color_t c);

/* alpha: 0..255, blends with the background (semi-transparent panels) */
void overlay_blend_rect(int x, int y, int w, int h, color_t c, int alpha);

/* Soft blob fading outwards from the centre (like a raindrop).
 * Uses the same rectangle data; the softening happens in the fragment program. */
void overlay_soft_blob(int x, int y, int w, int h, color_t c, int alpha);

/* Rectangle rotated about its centre (needles, dial ticks).
 * cx, cy centre; w, h full width/height; angle in radians. */
void overlay_rot_rect(float cx, float cy, float w, float h, float angle,
                      color_t c, int alpha);

/* A point of an arbitrary triangle mesh, in virtual 1280x720 coordinates.
 * a is per-vertex opacity (0..1); the GPU interpolates it across the
 * triangle, which is how glyph edges get their antialiasing ramp. */
typedef struct {
    float x, y, a;
} OvlPoint;

/* Arbitrary triangles, drawn in submission order with everything else.
 * idx holds ni indices into p (three per triangle). alpha scales every
 * vertex's own opacity. */
void overlay_tris(const OvlPoint *p, int np, const unsigned short *idx, int ni,
                  color_t c, int alpha);

/* Ring (gauge bezel) and filled disc */
void overlay_ring(float cx, float cy, float radius, float thickness,
                  color_t c, int alpha);
void overlay_disc(float cx, float cy, float radius, color_t c, int alpha);

#endif
