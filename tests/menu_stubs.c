/* gamemenu_draw'in cagirdigi cizim fonksiyonlari - testlerde etkisiz */
#include "../source/overlay.h"

void overlay_begin(void) { }
void overlay_fill_rect(int x, int y, int w, int h, color_t c)
{ (void)x; (void)y; (void)w; (void)h; (void)c; }
void overlay_blend_rect(int x, int y, int w, int h, color_t c, int a)
{ (void)x; (void)y; (void)w; (void)h; (void)c; (void)a; }
void font_draw_text(int x, int y, int s, const char *t, color_t c)
{ (void)x; (void)y; (void)s; (void)t; (void)c; }
void font_draw_center(int cx, int y, int s, const char *t, color_t c)
{ (void)cx; (void)y; (void)s; (void)t; (void)c; }
int  font_text_width(const char *t, int s) { (void)t; (void)s; return 0; }
