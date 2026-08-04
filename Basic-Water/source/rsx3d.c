#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>

#include <ppu-lv2.h>
#include <sysutil/video.h>
#include <rsx/gcm_sys.h>
#include <rsx/rsx.h>

#include "rsx3d.h"

#define CB_SIZE     0x100000
#define HOST_SIZE   (32 * 1024 * 1024)
#define BUFFERS     2

typedef struct {
    u32 *ptr;
    u32  offset;
    u32  width;
    u32  height;
    u32  id;
} Buffer;

static gcmContextData *context = NULL;
static void   *host_addr = NULL;
static Buffer  buffers[BUFFERS];
static int     cur_buf = 0;

static u32 *depth_buffer = NULL;
static u32  depth_offset = 0;
static u32  depth_pitch = 0;

static u32 scr_w = 0, scr_h = 0;

static void wait_flip(void)
{
    while (gcmGetFlipStatus() != 0)
        usleep(200);
    gcmResetFlipStatus();
}

static int make_buffer(Buffer *b, u32 width, u32 height, int id)
{
    u32 pitch = width * sizeof(u32);

    b->ptr = (u32 *)rsxMemalign(64, pitch * height);
    if (b->ptr == NULL)
        return -1;
    if (rsxAddressToOffset(b->ptr, &b->offset) != 0)
        return -1;
    if (gcmSetDisplayBuffer(id, b->offset, pitch, width, height) != 0)
        return -1;

    b->width = width;
    b->height = height;
    b->id = id;
    return 0;
}

/* Renk + derinlik hedefini bagla. Her karede cagrilir (cift tamponlama). */
static void set_render_target(const Buffer *b)
{
    gcmSurface sf;

    memset(&sf, 0, sizeof(sf));

    sf.colorFormat      = GCM_SURFACE_X8R8G8B8;
    sf.colorTarget      = GCM_SURFACE_TARGET_0;
    sf.colorLocation[0] = GCM_LOCATION_RSX;
    sf.colorOffset[0]   = b->offset;
    sf.colorPitch[0]    = depth_pitch;

    sf.colorLocation[1] = GCM_LOCATION_RSX;
    sf.colorLocation[2] = GCM_LOCATION_RSX;
    sf.colorLocation[3] = GCM_LOCATION_RSX;
    sf.colorPitch[1]    = 64;
    sf.colorPitch[2]    = 64;
    sf.colorPitch[3]    = 64;

    sf.depthFormat   = GCM_SURFACE_ZETA_Z16;
    sf.depthLocation = GCM_LOCATION_RSX;
    sf.depthOffset   = depth_offset;
    sf.depthPitch    = depth_pitch;

    sf.type      = GCM_TEXTURE_LINEAR;
    sf.antiAlias = GCM_SURFACE_CENTER_1;

    sf.width  = b->width;
    sf.height = b->height;
    sf.x = 0;
    sf.y = 0;

    rsxSetSurface(context, &sf);
}

static void set_draw_env(void)
{
    f32 scale[4], offset[4];
    f32 zmin = 0.0f, zmax = 1.0f;
    int i;

    rsxSetColorMask(context, GCM_COLOR_MASK_B | GCM_COLOR_MASK_G |
                             GCM_COLOR_MASK_R | GCM_COLOR_MASK_A);
    rsxSetColorMaskMrt(context, 0);

    scale[0] = scr_w * 0.5f;
    scale[1] = scr_h * -0.5f;
    scale[2] = (zmax - zmin) * 0.5f;
    scale[3] = 0.0f;
    offset[0] = scr_w * 0.5f;
    offset[1] = scr_h * 0.5f;
    offset[2] = (zmax + zmin) * 0.5f;
    offset[3] = 0.0f;

    rsxSetViewport(context, 0, 0, scr_w, scr_h, zmin, zmax, scale, offset);
    rsxSetScissor(context, 0, 0, scr_w, scr_h);

    rsxSetDepthTestEnable(context, GCM_TRUE);
    rsxSetDepthFunc(context, GCM_LESS);
    rsxSetDepthWriteEnable(context, 1);
    rsxSetShadeModel(context, GCM_SHADE_MODEL_SMOOTH);

    /* Deniz duzlemi tek yuzlu degil - iki taraftan da gorunsun */
    rsxSetCullFaceEnable(context, GCM_FALSE);
    rsxSetFrontFace(context, GCM_FRONTFACE_CCW);

    /* Derinlik kirpma kontrolu ve viewport kirpma dikdortgenleri.
     * Bunlar ayarlanmazsa RSX geometriyi tamamen kirpabilir. */
    rsxSetZMinMaxControl(context, GCM_FALSE, GCM_TRUE, GCM_FALSE);

    for (i = 0; i < 8; i++)
        rsxSetViewportClip(context, i, scr_w, scr_h);

    /* Kullanici kirpma duzlemleri kullanilmiyor; acikca kapatilmali,
     * aksi halde tanimsiz duzlemler tum sahneyi kirpar. */
    rsxSetUserClipPlaneControl(context,
                               GCM_USER_CLIP_PLANE_DISABLE,
                               GCM_USER_CLIP_PLANE_DISABLE,
                               GCM_USER_CLIP_PLANE_DISABLE,
                               GCM_USER_CLIP_PLANE_DISABLE,
                               GCM_USER_CLIP_PLANE_DISABLE,
                               GCM_USER_CLIP_PLANE_DISABLE);
}

int rsx3d_init(void)
{
    videoState state;
    videoConfiguration vconfig;
    videoResolution res;
    int i;

    host_addr = memalign(1024 * 1024, HOST_SIZE);
    if (host_addr == NULL)
        return -1;

    rsxInit(&context, CB_SIZE, HOST_SIZE, host_addr);
    if (context == NULL)
        return -2;

    if (videoGetState(0, 0, &state) != 0 || state.state != 0)
        return -3;
    if (videoGetResolution(state.displayMode.resolution, &res) != 0)
        return -4;

    memset(&vconfig, 0, sizeof(vconfig));
    vconfig.resolution = state.displayMode.resolution;
    vconfig.format     = VIDEO_BUFFER_FORMAT_XRGB;
    vconfig.pitch      = res.width * sizeof(u32);
    vconfig.aspect     = state.displayMode.aspect;

    if (videoConfigure(0, &vconfig, NULL, 0) != 0)
        return -5;
    if (videoGetState(0, 0, &state) != 0)
        return -6;

    scr_w = res.width;
    scr_h = res.height;

    gcmSetFlipMode(GCM_FLIP_VSYNC);

    depth_pitch  = res.width * sizeof(u32);
    depth_buffer = (u32 *)rsxMemalign(64, res.height * depth_pitch * 2);
    if (depth_buffer == NULL)
        return -7;
    if (rsxAddressToOffset(depth_buffer, &depth_offset) != 0)
        return -8;

    for (i = 0; i < BUFFERS; i++) {
        if (make_buffer(&buffers[i], res.width, res.height, i) != 0)
            return -9;
    }

    gcmResetFlipStatus();
    cur_buf = 0;
    return 0;
}

void rsx3d_exit(void)
{
    gcmSetWaitFlip(context);
    rsxFinish(context, 1);
    if (host_addr != NULL) {
        free(host_addr);
        host_addr = NULL;
    }
}

void rsx3d_begin_frame(u32 clear_color)
{
    set_render_target(&buffers[cur_buf]);
    set_draw_env();

    rsxSetClearColor(context, clear_color);
    rsxSetClearDepthStencil(context, 0xffffff00);
    rsxClearSurface(context, GCM_CLEAR_R | GCM_CLEAR_G | GCM_CLEAR_B |
                             GCM_CLEAR_A | GCM_CLEAR_S | GCM_CLEAR_Z);
}

void rsx3d_end_frame(void)
{
    gcmSetFlip(context, cur_buf);
    rsxFlushBuffer(context);
    gcmSetWaitFlip(context);
    wait_flip();
    cur_buf ^= 1;
}

float rsx3d_aspect(void)
{
    return (float)scr_w / (float)scr_h;
}

gcmContextData *rsx3d_context(void)
{
    return context;
}
