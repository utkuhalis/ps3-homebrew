#include <string.h>
#include <io/pad.h>

#include "input.h"

#define ANALOG_DEAD 60      /* 128 merkezden sapma esigi */

static padInfo padinfo;
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

/* padData alanlarini tek bir bit maskesinde toplar. Analog cubuk d-pad ile
 * ayni bitleri sürer; boylece hem tutma hem kenar algilama tek yerden gelir. */
static unsigned int pack(const padData *p)
{
    unsigned int b = 0;

    if (p->BTN_UP)     b |= PAD_UP;
    if (p->BTN_DOWN)   b |= PAD_DOWN;
    if (p->BTN_CROSS)  b |= PAD_CROSS;
    if (p->BTN_CIRCLE) b |= PAD_CIRCLE;
    if (p->BTN_START)  b |= PAD_START;
    if (p->BTN_SELECT) b |= PAD_SELECT;
    if (p->BTN_L1)     b |= PAD_L1;
    if (p->BTN_R1)     b |= PAD_R1;

    /* Analog cubuk: bazi ortamlarda (orn. RPCS3 klavye modu) analog verisi hic
     * gelmez ve tum eksenler 0 kalir. Bunu "cubuk tam yukarida" sanmamak icin,
     * iki eksen de tam 0 iken analog girdisi yok sayilir. Gercek bir cubukta
     * her iki eksenin ayni anda tam 0 olmasi sol-ust koseye tam dayanmak
     * demektir; o durumda da d-pad zaten calisir. */
    if (!(p->ANA_L_H == 0 && p->ANA_L_V == 0)) {
        if (p->ANA_L_V < 128 - ANALOG_DEAD) b |= PAD_UP;
        if (p->ANA_L_V > 128 + ANALOG_DEAD) b |= PAD_DOWN;
    }

    return b;
}

/* Ham eksen degerini (0..255, merkez 128) -1..+1 araligina cevirir.
 * Olu bolge disi kalan kisim yeniden olceklenir, boylece esigin hemen
 * disinda ani sicrama olmaz. */
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

void input_update(void)
{
    padData data;
    int i;

    ioPadGetInfo(&padinfo);

    axis_lx = axis_ly = axis_rx = axis_ry = 0.0f;

    for (i = 0; i < PP_MAX_PADS; i++) {
        prev_btn[i] = cur_btn[i];

        if (padinfo.status[i] != 0 && ioPadGetData(i, &data) == 0) {
            connected[i] = 1;
            cur_btn[i] = pack(&data);

            /* Eksenler yalnizca 1. koldan okunur. Analog verisi hic gelmiyorsa
             * (tum eksenler 0) eksen degerleri 0 birakilir; asagida d-pad
             * yedegi devreye girer. */
            if (i == 0 && !(data.ANA_L_H == 0 && data.ANA_L_V == 0 &&
                            data.ANA_R_H == 0 && data.ANA_R_V == 0)) {
                axis_lx = axis_norm(data.ANA_L_H);
                axis_ly = axis_norm(data.ANA_L_V);
                axis_rx = axis_norm(data.ANA_R_H);
                axis_ry = axis_norm(data.ANA_R_V);
            }
        } else {
            connected[i] = 0;
            cur_btn[i] = 0;     /* kol cikarilirsa tuslar birakilmis sayilir */
        }
    }

    /* Analog yoksa d-pad ile ileri/geri ve donus yine de mumkun olsun
     * (RPCS3 klavye modunda oynanabilirlik icin). */
    if (axis_ly == 0.0f) {
        if (cur_btn[0] & PAD_UP)   axis_ly = -1.0f;
        if (cur_btn[0] & PAD_DOWN) axis_ly =  1.0f;
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

float input_axis_left_x(void)  { return axis_lx; }
float input_axis_left_y(void)  { return axis_ly; }
float input_axis_right_x(void) { return axis_rx; }
float input_axis_right_y(void) { return axis_ry; }
