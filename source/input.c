#include <string.h>
#include <io/pad.h>

#include "input.h"

#define ANALOG_DEAD 60      /* threshold of deviation from the 128 centre */

static padInfo padinfo;

/* Right after the pad starts, the first readings can carry an invalid
 * button state (measured on a real DualShock 3: on the first valid frame
 * SQUARE appeared held). Data arrives sparsely, so that value lingered and
 * the camera seemed stuck turning. The first valid readings are skipped. */
#define PAD_WARMUP_READS 4
static int warmup[PP_MAX_PADS];
static float axis_lx, axis_ly, axis_rx, axis_ry;
static unsigned int cur_btn[PP_MAX_PADS];
static unsigned int prev_btn[PP_MAX_PADS];
static int connected[PP_MAX_PADS];

int input_init(void)
{
    memset(cur_btn, 0, sizeof(cur_btn));
    memset(prev_btn, 0, sizeof(prev_btn));
    memset(connected, 0, sizeof(connected));
    return ioPadInit(7);
}

void input_exit(void)
{
    ioPadEnd();
}

/* Packs the padData fields into one bit mask. The analogue stick drives
 * the same bits as the d-pad, so both hold and edge detection come from
 * one place. */
static unsigned int pack(const padData *p)
{
    unsigned int b = 0;

    if (p->BTN_UP)       b |= PAD_UP;
    if (p->BTN_DOWN)     b |= PAD_DOWN;
    if (p->BTN_LEFT)     b |= PAD_LEFT;
    if (p->BTN_RIGHT)    b |= PAD_RIGHT;
    if (p->BTN_SQUARE)   b |= PAD_SQUARE;
    if (p->BTN_TRIANGLE) b |= PAD_TRIANGLE;
    if (p->BTN_CROSS)  b |= PAD_CROSS;
    if (p->BTN_CIRCLE) b |= PAD_CIRCLE;
    if (p->BTN_START)  b |= PAD_START;
    if (p->BTN_SELECT) b |= PAD_SELECT;
    if (p->BTN_L1)     b |= PAD_L1;
    if (p->BTN_R1)     b |= PAD_R1;
    if (p->BTN_L2)     b |= PAD_L2;
    if (p->BTN_R2)     b |= PAD_R2;
    if (p->BTN_L3)     b |= PAD_L3;
    if (p->BTN_R3)     b |= PAD_R3;

    /* Analogue stick: in some environments (e.g. RPCS3 keyboard mode) no
     * analogue data arrives and every axis stays 0. So that this is not taken
     * for "stick fully up", analogue input is ignored while both axes are
     * exactly 0. On a real stick both axes at exactly 0 means pushed hard into
     * the top-left corner; the d-pad still works in that case. */
    if (!(p->ANA_L_H == 0 && p->ANA_L_V == 0)) {
        if (p->ANA_L_V < 128 - ANALOG_DEAD) b |= PAD_UP;
        if (p->ANA_L_V > 128 + ANALOG_DEAD) b |= PAD_DOWN;
    }

    return b;
}

/* Converts a raw axis value (0..255, centre 128) to -1..+1.
 * The part outside the dead zone is rescaled so there is no jump just past
 * the threshold. */
static float axis_norm(unsigned char raw)
{
    float v = ((float)raw - 128.0f) / 127.0f;   /* -1 .. +1 */
    float dead = (float)ANALOG_DEAD / 127.0f;

    if (v > dead)
        return (v - dead) / (1.0f - dead);
    if (v < -dead)
        return (v + dead) / (1.0f - dead);
    return 0.0f;
}

/* Both axes of a stick are resolved together, because "no analogue data"
 * can only be told by looking at both: if both axes are exactly 0, the
 * stick is not pushed into the top-left corner - the environment simply
 * sends no analogue data.
 *
 * This guard used to apply only to d-pad synthesis; the axes were read
 * unconditionally, so in RPCS3's keyboard mode axis_lx stuck at -1.0 the
 * moment the game started. The result: 'a' seemed dead because it changed
 * nothing, pressing 'd' made the emulator start sending real analogue data
 * centred at 128, and only after that did 'a' work. */
static void axis_pair(unsigned char h, unsigned char v, float *ox, float *oy)
{
    if (h == 0 && v == 0) {
        *ox = 0.0f;
        *oy = 0.0f;
        return;
    }
    *ox = axis_norm(h);
    *oy = axis_norm(v);
}

void input_update(void)
{
    padData data;
    int i;

    ioPadGetInfo(&padinfo);

    for (i = 0; i < PP_MAX_PADS; i++) {
        prev_btn[i] = cur_btn[i];

        if (padinfo.status[i] == 0) {
            /* pad removed: buttons count as released, sticks as centred */
            connected[i] = 0;
            cur_btn[i] = 0;
            warmup[i] = 0;              /* start over if the pad is plugged in again */
            if (i == 0) {
                axis_lx = axis_ly = 0.0f;
                axis_rx = axis_ry = 0.0f;
            }
            continue;
        }

        connected[i] = 1;

        /* The PS3 pad API does not deliver data every frame. data.len == 0 means
         * there is NO NEW DATA this frame and the struct contents are invalid (the
         * axes read 0). Without this check the axes read as "stick pushed all the
         * way" and the camera drifted on its own. With no data, the previous
         * frame's state is kept. */
        if (ioPadGetData(i, &data) == 0 && data.len > 0) {
            if (warmup[i] < PAD_WARMUP_READS) {
                warmup[i]++;
                continue;               /* start-up data: ignore */
            }

            cur_btn[i] = pack(&data);

            if (i == 0) {
                axis_pair(data.ANA_L_H, data.ANA_L_V, &axis_lx, &axis_ly);
                axis_pair(data.ANA_R_H, data.ANA_R_V, &axis_rx, &axis_ry);
            }
        }
        /* data.len == 0: cur_btn and the axes are left as they are */
    }
}

int input_connected(int pad)
{
    if (pad < 0 || pad >= PP_MAX_PADS)
        return 0;
    return connected[pad];
}

int input_held(int pad, unsigned int mask)
{
    if (pad < 0 || pad >= PP_MAX_PADS)
        return 0;
    return (cur_btn[pad] & mask) != 0;
}

int input_pressed(int pad, unsigned int mask)
{
    if (pad < 0 || pad >= PP_MAX_PADS)
        return 0;
    return ((cur_btn[pad] & ~prev_btn[pad]) & mask) != 0;
}

int input_dir(int pad)
{
    if (input_held(pad, PAD_UP))
        return -1;
    if (input_held(pad, PAD_DOWN))
        return 1;
    return 0;
}

int input_any_pressed(unsigned int mask)
{
    int i;
    for (i = 0; i < PP_MAX_PADS; i++)
        if (input_pressed(i, mask))
            return 1;
    return 0;
}

int input_any_dir_pressed(void)
{
    int i;
    for (i = 0; i < PP_MAX_PADS; i++) {
        if (input_pressed(i, PAD_UP))
            return -1;
        if (input_pressed(i, PAD_DOWN))
            return 1;
    }
    return 0;
}

/* Without an analogue stick (e.g. keyboard in an emulator) the direction
 * and shoulder buttons step in as full-strength fallbacks, so the camera
 * can be driven anywhere. Analogue takes priority when present, because it
 * gives proportional speed. */
static float axis_or(float a, float dpad)
{
    float aa = a < 0.0f ? -a : a;
    return aa > 0.05f ? a : dpad;
}

float input_axis_left_x(void)
{
    float d = 0.0f;
    if (input_held(0, PAD_LEFT))  d = -1.0f;
    if (input_held(0, PAD_RIGHT)) d =  1.0f;
    return axis_or(axis_lx, d);
}

float input_axis_left_y(void)
{
    float d = 0.0f;
    if (input_held(0, PAD_UP))   d = -1.0f;
    if (input_held(0, PAD_DOWN)) d =  1.0f;
    return axis_or(axis_ly, d);
}

/* Looking comes from the right stick first. Without analogue (keyboard in
 * an emulator) the face buttons can act as a fallback; that fallback is
 * only on while the menu is closed, otherwise it clashes with the menu's
 * confirm button. */
static int look_keys_enabled = 1;

void input_set_look_keys(int enabled)
{
    look_keys_enabled = enabled;
}

float input_axis_right_x(void)
{
    float d = 0.0f;

    if (look_keys_enabled) {
        if (input_held(0, PAD_SQUARE)) d = -1.0f;
        if (input_held(0, PAD_CIRCLE)) d =  1.0f;
    }
    return axis_or(axis_rx, d);
}

/* --- flight: right stick, fallback on the d-pad --- */
float input_flight_pitch(void)
{
    float d = 0.0f;

    if (input_held(0, PAD_UP))   d =  1.0f;   /* nose up */
    if (input_held(0, PAD_DOWN)) d = -1.0f;

    /* Pushing the stick up gives a negative raw value; flipped so nose-up is positive. */
    return axis_or(-axis_ry, d);
}

float input_flight_roll(void)
{
    float d = 0.0f;

    if (input_held(0, PAD_LEFT))  d = -1.0f;
    if (input_held(0, PAD_RIGHT)) d =  1.0f;
    return axis_or(axis_rx, d);
}

/* --- camera: left stick, no fallback (the d-pad is used for flying) --- */
float input_cam_x(void)
{
    return axis_lx;
}

float input_cam_y(void)
{
    return axis_ly;
}

float input_axis_right_y(void)
{
    float d = 0.0f;

    if (look_keys_enabled) {
        if (input_held(0, PAD_TRIANGLE)) d = -1.0f;
        if (input_held(0, PAD_CROSS))    d =  1.0f;
    }
    return axis_or(axis_ry, d);
}
