#ifndef WEATHERFX_H
#define WEATHERFX_H

#include "atmosphere.h"

/* Ekran ustu hava efektleri: yagmur cizgileri ve simsek parlamasi.
 * 2D bindirme katmani uzerine cizilir; atmosferin rain/lightning
 * degerlerine gore siddetlenir. */

void weatherfx_draw(const Atmosphere *atm, float time);

#endif
