#ifndef INPUT_H
#define INPUT_H

/* Up to 2 pads are read. Edge detection (input_pressed) is provided to
 * avoid key repeat in menus. */

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
#define PAD_L2      (1u << 12)
#define PAD_R2      (1u << 13)
#define PAD_L3      (1u << 14)
#define PAD_R3      (1u << 15)

#define PP_MAX_PADS 2

int  input_init(void);
void input_exit(void);
void input_update(void);

int  input_connected(int pad);
int  input_held(int pad, unsigned int mask);
int  input_pressed(int pad, unsigned int mask);   /* only on the frame it was pressed */

/* Vertical direction: -1 up, 0 none, +1 down (d-pad or left stick) */
int  input_dir(int pad);

/* Analogue axes, -1.0 .. +1.0, dead zone applied.
 * The Y axis is positive DOWN (same as the pad hardware).
 * Without analogue data (RPCS3 keyboard mode) the d-pad drives them. */
/* --- control reads by meaning ---
 *
 * Flight is driven by the RIGHT stick, the camera by the LEFT. Using these
 * instead of the raw axis functions keeps which stick does what in one
 * place; the two got swapped once.
 *
 * The d-pad works as a fallback for the flight controls: in the
 * emulator's keyboard mapping the right stick lands on keys like
 * Home/End/PageDown, which makes the game unplayable. */
float input_flight_pitch(void);   /* + nose up */
float input_flight_roll(void);    /* + bank right */

/* Orbit camera: left stick */
float input_cam_x(void);
float input_cam_y(void);

float input_axis_left_x(void);
float input_axis_left_y(void);
/* The look-button fallback is off while the menu is open (so it does not clash with confirm) */
void  input_set_look_keys(int enabled);

float input_axis_right_x(void);
float input_axis_right_y(void);

/* Input from any pad - for the menu */
int  input_any_pressed(unsigned int mask);
int  input_any_dir_pressed(void);

#endif
