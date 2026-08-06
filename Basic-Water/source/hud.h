#ifndef HUD_H
#define HUD_H

#include "camera.h"
#include "flight.h"

/* Ucus gostergeleri.
 *
 * Hiz ve yukseklik kameradan gelen GERCEK degerlerdir.
 * Agirlik ve yakit su an gostermeliktir: yakit zamanla azalir, agirlik da
 * yakitla birlikte duser. Ucus modeli eklendiginde bu iki deger gercege
 * baglanacak; HUD arayuzu ayni kalacak. */

typedef struct {
    float fuel_pct;     /* 0..100 */
    float weight_kg;
    float speed_kmh;
    float prev_pos[3];
    int   has_prev;
} Hud;

void hud_init(Hud *h);

/* Degerler artik ucus modelinden gelir: hiz, agirlik ve yakit gercektir. */
void hud_update(Hud *h, const Flight *f, float dt);

void hud_draw(const Hud *h, const Camera *cam);

#endif
