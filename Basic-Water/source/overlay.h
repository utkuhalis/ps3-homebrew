#ifndef OVERLAY_H
#define OVERLAY_H

/* 3D sahnenin uzerine 2D cizim (HUD, menu).
 * Koordinatlar sanal 1280x720 sistemindedir; modul gercek cozunurluge
 * olceklendirir. Renkler 0xRRGGBB. */

typedef unsigned int color_t;

#define OVL_W 1280
#define OVL_H 720

#define RGB(r, g, b) (((color_t)(r) << 16) | ((color_t)(g) << 8) | (color_t)(b))

/* Bir kez, program baslarken */
int  overlay_init(void);

/* Kare basi: toplanan dikdortgenleri sifirlar */
void overlay_begin(void);

/* Toplanan tum dikdortgenleri tek cizim cagrisinda GPU'ya gonderir.
 * 3D sahne cizildikten sonra, kare sonunda cagrilir. */
void overlay_flush(void);

/* Kare icinde cizim tamponu doldu mu (tani icin) */
int  overlay_overflowed(void);

void overlay_fill_rect(int x, int y, int w, int h, color_t c);

/* alpha: 0..255, arka planla harmanlar (yari saydam paneller icin) */
void overlay_blend_rect(int x, int y, int w, int h, color_t c, int alpha);

/* Kenarlari merkezden disa dogru sonen yumusak leke (yagmur damlasi gibi).
 * Ayni dikdortgen verisi kullanilir; yumusatma fragment tarafinda yapilir. */
void overlay_soft_blob(int x, int y, int w, int h, color_t c, int alpha);

/* Merkez etrafinda dondurulmus dikdortgen (ibreler, kadran centikleri).
 * cx, cy merkez; w, h tam genislik/yukseklik; angle radyan. */
void overlay_rot_rect(float cx, float cy, float w, float h, float angle,
                      color_t c, int alpha);

/* Halka (gosterge cercevesi) ve dolu daire */
void overlay_ring(float cx, float cy, float radius, float thickness,
                  color_t c, int alpha);
void overlay_disc(float cx, float cy, float radius, color_t c, int alpha);

#endif
