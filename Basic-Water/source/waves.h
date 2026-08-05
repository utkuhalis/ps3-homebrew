#ifndef WAVES_H
#define WAVES_H

/* Deniz yuzeyi dalgalari: birkac yonlu sinus dalgasinin toplami.
 *
 * ONEMLI: Ayni formul shaders/water.vcg icinde de bulunur (Cg'de include
 * yok). Biri degistiginde digeri de guncellenmelidir.
 *
 * Donanim bagimsizdir: birim testlerde ve host onizlemesinde kullanilir. */

#define WAVE_MAX_HEIGHT 2.6f   /* dalga tepesinin ust siniri (birim) */

/* (x, z) noktasindaki dalga yuksekligi (deniz seviyesine gore) */
float wave_height(float x, float z, float time);

/* Ayni noktadaki yuzey normali (birim vektor). Parlama ve yansima
 * dogrulugu buna bagli. */
void  wave_normal(float x, float z, float time, float out[3]);

#endif
