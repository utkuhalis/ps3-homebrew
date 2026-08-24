#ifndef ATMOSPHERE_H
#define ATMOSPHERE_H

#include "gamemenu.h"

/* Hava durumu ve gun saatinden sahne parametrelerini uretir.
 *
 * Donanim bagimsizdir (birim testli). Uretilen degerler gokyuzu ve su
 * shader'larina uniform olarak gonderilir; boylece sahnenin tum atmosfer
 * ayari tek yerden yonetilir. */

typedef struct {
    float sun_dir[3];       /* birim vektor; gece ay yonu olarak kullanilir */
    float sun_color[3];     /* gunes/ay isiginin rengi */
    float horizon[3];       /* ufuk rengi */
    float zenith[3];        /* tepe rengi */
    float cloud_low;        /* bulut esigi alt siniri (dusuk = daha cok bulut) */
    float cloud_high;       /* bulut esigi ust siniri */
    float cloud_bright;     /* bulut parlakligi (firtinada koyu) */
    float fog_distance;     /* sis mesafesi (birim) */
    float wave_scale;       /* dalga genligi carpani */
    float wave_length;      /* dalga boyu carpani: genlik buyurken boy da
                             * uzamazsa tepeler izgaraya sigmayip koseli
                             * (low-poly) gorunur */
    float water_dark;       /* suyun taban renginin karartma carpani */
    float rain;             /* 0..1 yagmur siddeti */
    float lightning;        /* 0..1 simsek olasiligi */
} Atmosphere;

void atmosphere_compute(Atmosphere *a, Weather w, TimeOfDay t);

#endif
