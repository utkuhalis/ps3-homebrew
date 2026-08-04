#include <string.h>
#include <malloc.h>
#include <unistd.h>

#include <ppu-lv2.h>
#include <sysutil/video.h>
#include <rsx/gcm_sys.h>
#include <rsx/rsx.h>

#include "video.h"
#include "game.h"   /* FIELD_W / FIELD_H: sanal koordinat sistemi */

#define CB_SIZE    0x100000
#define HOST_SIZE  (32 * 1024 * 1024)

static gcmContextData *context = NULL;
static void *host_addr = NULL;
static u32 *buffers[2] = { NULL, NULL };
static u32  offsets[2];
static int  cur_buf = 0;
static int  scr_w = 0, scr_h = 0;

static void wait_flip(void)
{
    while (gcmGetFlipStatus() != 0)
        usleep(200);
    gcmResetFlipStatus();
}

int video_init(void)
{
    videoState state;
    videoConfiguration vconfig;
    videoResolution res;
    int i;

    host_addr = memalign(1024 * 1024, HOST_SIZE);
    if (host_addr == NULL)
        return -1;

    context = NULL;
    rsxInit(&context, CB_SIZE, HOST_SIZE, host_addr);
    if (context == NULL)
        return -2;

    if (videoGetState(0, 0, &state) != 0 || state.state != 0)
        return -3;                                  /* ekran bagli degil */

    if (videoGetResolution(state.displayMode.resolution, &res) != 0)
        return -4;

    memset(&vconfig, 0, sizeof(vconfig));
    vconfig.resolution = state.displayMode.resolution;
    vconfig.format     = VIDEO_BUFFER_FORMAT_XRGB;
    vconfig.pitch      = res.width * 4;
    vconfig.aspect     = state.displayMode.aspect;

    if (videoConfigure(0, &vconfig, NULL, 0) != 0)
        return -5;
    if (videoGetState(0, 0, &state) != 0)
        return -6;

    scr_w = res.width;
    scr_h = res.height;

    gcmSetFlipMode(GCM_FLIP_VSYNC);

    for (i = 0; i < 2; i++) {
        buffers[i] = (u32 *)rsxMemalign(64, res.height * res.width * 4);
        if (buffers[i] == NULL)
            return -7;
        if (rsxAddressToOffset(buffers[i], &offsets[i]) != 0)
            return -8;
        if (gcmSetDisplayBuffer(i, offsets[i], res.width * 4,
                                res.width, res.height) != 0)
            return -9;
    }

    gcmResetFlipStatus();
    cur_buf = 0;
    return 0;
}

void video_exit(void)
{
    gcmSetWaitFlip(context);
    rsxFinish(context, 1);
    if (host_addr != NULL) {
        free(host_addr);
        host_addr = NULL;
    }
}

void video_clear(color_t c)
{
    u32 *fb = buffers[cur_buf];
    int i, n = scr_w * scr_h;

    for (i = 0; i < n; i++)
        fb[i] = c;
}

void video_fill_rect(int x, int y, int w, int h, color_t c)
{
    u32 *fb = buffers[cur_buf];
    int x0, y0, x1, y1, px, py;

    /* sanal 1280x720 -> gercek cozunurluk */
    x0 = x * scr_w / FIELD_W;
    y0 = y * scr_h / FIELD_H;
    x1 = (x + w) * scr_w / FIELD_W;
    y1 = (y + h) * scr_h / FIELD_H;

    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > scr_w) x1 = scr_w;
    if (y1 > scr_h) y1 = scr_h;

    for (py = y0; py < y1; py++) {
        u32 *row = fb + py * scr_w;
        for (px = x0; px < x1; px++)
            row[px] = c;
    }
}

void video_draw_image(const color_t *src, int sw, int sh,
                      int x, int y, int w, int h, int use_alpha)
{
    u32 *fb = buffers[cur_buf];
    int x0, y0, x1, y1, px, py;

    if (src == NULL || sw <= 0 || sh <= 0 || w <= 0 || h <= 0)
        return;

    /* sanal 1280x720 -> gercek cozunurluk */
    x0 = x * scr_w / FIELD_W;
    y0 = y * scr_h / FIELD_H;
    x1 = (x + w) * scr_w / FIELD_W;
    y1 = (y + h) * scr_h / FIELD_H;

    if (x1 <= x0 || y1 <= y0)
        return;

    for (py = (y0 < 0 ? 0 : y0); py < y1 && py < (int)scr_h; py++) {
        u32 *row = fb + py * scr_w;
        /* hedef satirin kaynaktaki karsiligi (en yakin komsu) */
        int sy = (py - y0) * sh / (y1 - y0);
        const color_t *srow = src + sy * sw;

        for (px = (x0 < 0 ? 0 : x0); px < x1 && px < (int)scr_w; px++) {
            int sx = (px - x0) * sw / (x1 - x0);
            color_t s = srow[sx];

            if (!use_alpha) {
                row[px] = s & 0x00FFFFFF;
                continue;
            }

            {
                unsigned int a = (s >> 24) & 0xFF;

                if (a == 0)
                    continue;                       /* tamamen saydam */
                if (a == 255) {
                    row[px] = s & 0x00FFFFFF;       /* tamamen opak */
                } else {
                    color_t d = row[px];
                    unsigned int ia = 255 - a;
                    unsigned int r = (((s >> 16) & 0xFF) * a + ((d >> 16) & 0xFF) * ia) / 255;
                    unsigned int g = (((s >>  8) & 0xFF) * a + ((d >>  8) & 0xFF) * ia) / 255;
                    unsigned int b = (( s        & 0xFF) * a + ( d        & 0xFF) * ia) / 255;

                    row[px] = (r << 16) | (g << 8) | b;
                }
            }
        }
    }
}

void video_flip(void)
{
    gcmSetFlip(context, cur_buf);
    rsxFlushBuffer(context);
    gcmSetWaitFlip(context);
    wait_flip();
    cur_buf ^= 1;
}
