#ifndef VIDEO_H
#define VIDEO_H

/* Cift tamponlu framebuffer. Tum cizim CPU tarafinda yapilir; shader yok.
 * Disariya acilan koordinatlar sanal 1280x720 sistemindedir (game.h ile ayni),
 * modul iceride gercek ekran cozunurluguna olceklendirir. */

typedef unsigned int color_t;

#define RGB(r, g, b) (((color_t)(r) << 16) | ((color_t)(g) << 8) | (color_t)(b))

/* Ham piksel gorseli cizer (derleme sirasinda gomulen data klasoru verisi).
 * src: ARGB8888 dizi, sw x sh boyutunda.
 * x,y,w,h: sanal 1280x720 sisteminde hedef dikdortgen (olcekleme yapilir).
 * use_alpha: 1 ise alfa harmanlamasi uygulanir (yuvarlak top kenarlari icin). */
void video_draw_image(const color_t *src, int sw, int sh,
                      int x, int y, int w, int h, int use_alpha);

int  video_init(void);
void video_exit(void);
void video_clear(color_t c);
void video_fill_rect(int x, int y, int w, int h, color_t c);
void video_flip(void);

#endif
