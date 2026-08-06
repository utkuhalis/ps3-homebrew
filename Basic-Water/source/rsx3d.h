#ifndef RSX3D_H
#define RSX3D_H

#include <ppu-types.h>
#include <rsx/rsx.h>

/* RSX 3D katmani: ekran kurulumu, derinlik tamponu, cizim ortami, flip.
 * Ust katmanlar (scene) gcm ayrintilarini bilmek zorunda kalmasin diye
 * context ve olculer buradan alinir. */

int   rsx3d_init(void);
void  rsx3d_exit(void);

/* Kare basi: render hedefini bagla, ekrani temizle, cizim ortamini kur. */
void  rsx3d_begin_frame(u32 clear_color);
void  rsx3d_end_frame(void);

float rsx3d_aspect(void);
gcmContextData *rsx3d_context(void);

/* Ekran olculeri (2D bindirmenin olcekleme icin ihtiyaci var) */
u32  rsx3d_width(void);
u32  rsx3d_height(void);

#endif
