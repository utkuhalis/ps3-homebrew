#include <string.h>
#include <io/pad.h>

#include "input.h"

#define ANALOG_DEAD 60      /* 128 merkezden sapma esigi */

static padInfo padinfo;
static float axis_v[PP_MAX_PADS];

static unsigned int cur_btn[PP_MAX_PADS];
static unsigned int prev_btn[PP_MAX_PADS];
static int connected[PP_MAX_PADS];

int input_init(void)
{
    int i;

    memset(cur_btn, 0, sizeof(cur_btn));
    memset(prev_btn, 0, sizeof(prev_btn));
    memset(connected, 0, sizeof(connected));
    memset(axis_v, 0, sizeof(axis_v));
    (void)i;

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
    if (p->BTN_START)    b |= PAD_START;
    if (p->BTN_SELECT)   b |= PAD_SELECT;
    if (p->BTN_TRIANGLE) b |= PAD_TRIANGLE;

    /* Analog cubuk bilerek buton maskesine katilmaz: ayni girdinin hem tus
     * hem eksen olarak sayilmasi menude tek basista birden fazla satir
     * atlanmasina yol aciyordu. Analog yalnizca axis_v uzerinden okunur. */

    return b;
}

/* Ham eksen degerini (0..255, merkez 128) -1..+1 araligina cevirir.
 * Olu bolge disindaki kisim yeniden olceklenir; boylece esigin hemen otesinde
 * ani sicrama olmaz ve cubugu az itince raket yavas gider. */
static float axis_norm(unsigned char raw)
{
    float v = ((float)raw - 128.0f) / 127.0f;
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

    for (i = 0; i < PP_MAX_PADS; i++) {
        prev_btn[i] = cur_btn[i];

        if (padinfo.status[i] == 0) {
            /* kol cikarildi: tuslar birakilmis, cubuk merkezde sayilir */
            connected[i] = 0;
            cur_btn[i] = 0;
            axis_v[i] = 0.0f;
            continue;
        }

        connected[i] = 1;

        /* PS3 pad API'si her karede veri vermez. data.len == 0 ise bu karede
         * YENI VERI YOKTUR ve yapinin icerigi gecersizdir (eksenler 0 gelir).
         * Bu durumda onceki karenin durumu korunur; aksi halde her karede
         * tuslar sifirlanip yeniden basilmis gibi gorunur (menude coklu
         * atlama) ve eksenler 0 okunup cubuk sonuna kadar itilmis sayilir
         * (raketin yukari yapismasi). */
        if (ioPadGetData(i, &data) == 0 && data.len > 0) {
            cur_btn[i] = pack(&data);
            axis_v[i] = axis_norm(data.ANA_L_V);
        }
        /* data.len == 0: cur_btn ve axis_v oldugu gibi birakilir */
    }
}

float input_move(int pad)
{
    float a, d = 0.0f;

    if (pad < 0 || pad >= PP_MAX_PADS)
        return 0.0f;

    a = axis_v[pad];                 /* analog: itildigi kadar hiz */

    if (cur_btn[pad] & PAD_UP)
        d = -1.0f;                   /* d-pad: tam guc */
    else if (cur_btn[pad] & PAD_DOWN)
        d = 1.0f;

    /* Ikisi birden varsa buyuk olan kazanir; analog yoksa d-pad surer. */
    return ((a < 0.0f ? -a : a) > (d < 0.0f ? -d : d)) ? a : d;
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

    /* Menude YALNIZCA yon tuslari gecerlidir. Analog cubuk bilerek disarida
     * birakildi: emulatorde eksen degerleri kararsiz gelip menunun kendi
     * kendine gezinmesine yol aciyordu. Analog sadece oyun icinde,
     * raket hizi icin kullanilir (bkz. input_move). */
    for (i = 0; i < PP_MAX_PADS; i++) {
        if (input_pressed(i, PAD_UP))
            return -1;
        if (input_pressed(i, PAD_DOWN))
            return 1;
    }
    return 0;
}
