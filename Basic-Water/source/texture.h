#ifndef TEXTURE_H
#define TEXTURE_H

/* RSX doku destegi.
 *
 * Dokular tools/make_textures.py ile "BWT1" bicimine cevrilip EBOOT'a
 * gomulur: 2'nin kuvveti boyut, ARGB big-endian, tam mip zinciri.
 *
 * Modul acilista tum dokulari RSX belleğine kopyalar ve her biri icin bir
 * gcmTexture tanimlar; cizim sirasinda yalnizca texture_bind cagrilir. */

#define TEXTURE_MAX 8

/* Gomulu doku paketini cozumler ve RSX'e yukler. 0: basarili */
int texture_init(void);

/* Ada gore doku indeksi; bulunamazsa -1 */
int texture_find(const char *name);

/* Dokuyu bir doku birimine baglar (fragment shader'daki sampler sirasi).
 * tile: doku kac kez tekrarlansin (UV carpani shader'a ayrica verilir). */
void texture_bind(int unit, int index);

/* Yuklenen doku sayisi (tani icin) */
int texture_count(void);

#endif
