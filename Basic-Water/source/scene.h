#ifndef SCENE_H
#define SCENE_H

#include "camera.h"
#include "mat4.h"
#include "atmosphere.h"

/* Deniz duzlemi: geometri uretimi, shader yukleme ve cizim. */

#define SKY_CLEAR_COLOR  0x7CA8F0   /* gokyuzu mavisi (XRGB) - ekran temizleme */

int  scene_init(void);
void scene_draw(const Camera *cam, const Mat4 *proj, float time,
                const Atmosphere *atm);

#endif
