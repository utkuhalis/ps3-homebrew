#ifndef FONT_H
#define FONT_H

#include "overlay.h"

/* 8x8 bitmap font. Metinler UTF-8'dir; Turkce karakterler desteklenir.
 * Koordinatlar sanal 1280x720 sisteminde, scale glif pikselinin buyuklugu. */

#define FONT_CELL 8

void font_draw_text(int x, int y, int scale, const char *utf8, color_t c);
void font_draw_center(int cx, int y, int scale, const char *utf8, color_t c);
int  font_text_width(const char *utf8, int scale);

#endif
