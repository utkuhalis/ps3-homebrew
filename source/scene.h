#ifndef SCENE_H
#define SCENE_H

#include "camera.h"
#include "mat4.h"
#include "atmosphere.h"

/* Sea plane: geometry generation, shader loading and drawing. */

#define SKY_CLEAR_COLOR  0x7CA8F0   /* sky blue (XRGB) - screen clear */

int  scene_init(void);
void scene_draw(const Camera *cam, const Mat4 *proj, float time,
                const Atmosphere *atm);

#endif
