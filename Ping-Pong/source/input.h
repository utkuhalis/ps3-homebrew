#ifndef INPUT_H
#define INPUT_H

/* En fazla 2 kol okunur. Menude tus tekrarini onlemek icin kenar algilama
 * (input_pressed) saglanir. */

#define PAD_UP      (1u << 0)
#define PAD_DOWN    (1u << 1)
#define PAD_CROSS   (1u << 2)
#define PAD_CIRCLE  (1u << 3)
#define PAD_START   (1u << 4)
#define PAD_SELECT  (1u << 5)
#define PAD_TRIANGLE (1u << 6)

#define PP_MAX_PADS 2

int  input_init(void);
void input_exit(void);
void input_update(void);

int  input_connected(int pad);
int  input_held(int pad, unsigned int mask);
int  input_pressed(int pad, unsigned int mask);   /* sadece basildigi karede */

/* Dikey yon: -1 yukari, 0 sabit, +1 asagi (d-pad veya sol analog) */
int  input_dir(int pad);

/* Dikey hareket miktari: -1.0 .. +1.0
 * Analog cubuk baglıysa ne kadar itildigine gore orantili deger doner
 * (hafif itis = yavas raket). Analog verisi yoksa d-pad tam guc surer. */
float input_move(int pad);

/* Herhangi bir koldan gelen giris - menu icin */
int  input_any_pressed(unsigned int mask);
int  input_any_dir_pressed(void);

#endif
