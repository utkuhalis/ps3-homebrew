#ifndef FONT_H
#define FONT_H

#include "overlay.h"

/* Vector text in Nunito Bold. Strings are UTF-8.
 *
 * Coordinates are in the virtual 1280x720 space. y is the top of the capital
 * letters and scale sets the size: one scale step is FONT_EM_PER_SCALE pixels
 * of em. The font is proportional, so always measure with font_text_width. */

#define FONT_EM_PER_SCALE 10.0f

void font_draw_text(int x, int y, int scale, const char *utf8, color_t c);
void font_draw_center(int cx, int y, int scale, const char *utf8, color_t c);
int  font_text_width(const char *utf8, int scale);

#endif
