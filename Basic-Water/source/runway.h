#ifndef RUNWAY_H
#define RUNWAY_H

#include "camera.h"
#include "mat4.h"
#include "atmosphere.h"

/* Deniz uzerindeki iki pist platformu: kalkis ve inis.
 * Konumlari burada tanimlanir ve mini harita, gorevler ve yon gostergeleri
 * ayni degerleri kullanir. */

#define RUNWAY_COUNT 2

/* Dunya koordinatlari (x, z) ve platform yuksekligi */
extern const float RUNWAY_POS[RUNWAY_COUNT][2];
#define RUNWAY_DECK_Y   9.0f    /* platform yuzeyi deniz seviyesinin uzerinde */
/* 737-800 kalkis kosusu ~1150 m; 1100 m'lik pist sinirda kaliyordu.
 * Gercek bir ticari pist olcusune cikarildi. */
#define RUNWAY_LENGTH 2600.0f
#define RUNWAY_WIDTH   60.0f    /* gercek pist genisligi ~45-60 m */

const char *runway_name(int index);

int  runway_init(void);
void runway_draw(const Camera *cam, const Mat4 *proj, const Atmosphere *atm);

#endif
