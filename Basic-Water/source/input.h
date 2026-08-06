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
#define PAD_L1      (1u << 6)
#define PAD_R1      (1u << 7)
#define PAD_LEFT    (1u << 8)
#define PAD_RIGHT   (1u << 9)
#define PAD_SQUARE  (1u << 10)
#define PAD_TRIANGLE (1u << 11)

#define PP_MAX_PADS 2

int  input_init(void);
void input_exit(void);
void input_update(void);

int  input_connected(int pad);
int  input_held(int pad, unsigned int mask);
int  input_pressed(int pad, unsigned int mask);   /* sadece basildigi karede */

/* Dikey yon: -1 yukari, 0 sabit, +1 asagi (d-pad veya sol analog) */
int  input_dir(int pad);

/* Analog eksenler, -1.0 .. +1.0. Olu bolge uygulanmistir.
 * Y ekseni ASAGI pozitiftir (kol donanimiyla ayni yon).
 * Analog verisi gelmiyorsa (RPCS3 klavye modu) d-pad bunlari surer. */
float input_axis_left_x(void);
float input_axis_left_y(void);
/* Menu acikken bakis tus yedegi kapatilir (onay tusuyla cakismasin) */
void  input_set_look_keys(int enabled);

float input_axis_right_x(void);
float input_axis_right_y(void);

/* Herhangi bir koldan gelen giris - menu icin */
int  input_any_pressed(unsigned int mask);
int  input_any_dir_pressed(void);

#endif
