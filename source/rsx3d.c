#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>

#include <ppu-lv2.h>
#include <sysutil/video.h>
#include <rsx/gcm_sys.h>
#include <rsx/rsx.h>

#include "rsx3d.h"
#include "ps3log.h"

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

#define GCM_LABEL_INDEX 255

/* Counterpart of waitRSXIdle in the reference rsxutil: waits for RSX to
 * finish all pending work. It has to be called before changing the video
 * mode; our code was missing this step. */
static void wait_rsx_idle(void)
{
    u32 label_val = 1;

    rsxSetWriteBackendLabel(context, GCM_LABEL_INDEX, label_val);
    rsxSetWaitLabel(context, GCM_LABEL_INDEX, label_val);

    label_val++;

    rsxSetWriteBackendLabel(context, GCM_LABEL_INDEX, label_val);
    rsxFlushBuffer(context);

    while (*(vu32 *)gcmGetLabelAddress(GCM_LABEL_INDEX) != label_val)
        usleep(30);
}

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

/* Bind the colour + depth target. Called every frame (double buffering). */
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

    /* The sea plane is not one-sided - visible from both sides */
    /* Backface culling is off by default: water and sky are one-sided
     * surfaces. Solid passes (aircraft, runway) turn it on themselves. */
    rsxSetCullFaceEnable(context, GCM_FALSE);

    /* The front face winding was MEASURED: triangles facing the camera come
     * out counter-clockwise in screen space (31481 triangles, no exceptions).
     * The first version reasoned from the viewport's Y flip and said CW - that
     * was wrong, and as a result the front faces were culled and the INSIDE
     * of the aircraft was drawn. */
    rsxSetFrontFace(context, GCM_FRONTFACE_CCW);

    /* Depth clip control and viewport scissor rectangles.
     * If these are not set, RSX may clip the geometry away entirely. */
    rsxSetZMinMaxControl(context, GCM_FALSE, GCM_TRUE, GCM_FALSE);

    for (i = 0; i < 8; i++)
        rsxSetViewportClip(context, i, scr_w, scr_h);

    /* User clip planes are not used; they must be disabled explicitly,
     * otherwise undefined planes clip the whole scene. */
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

    wait_rsx_idle();

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
    rsxSetClearDepthStencil(context, 0xffffffff);
    rsxClearSurface(context, GCM_CLEAR_R | GCM_CLEAR_G | GCM_CLEAR_B |
                             GCM_CLEAR_A | GCM_CLEAR_S | GCM_CLEAR_Z);
}

void rsx3d_end_frame(void)
{
    gcmSetFlip(context, cur_buf);
    rsxFlushBuffer(context);
    gcmSetWaitFlip(context);

    cur_buf ^= 1;
    set_render_target(&buffers[cur_buf]);

    wait_flip();
}

float rsx3d_aspect(void)
{
    return (float)scr_w / (float)scr_h;
}

/* Backface culling for solid geometry. On the 60k-triangle aircraft model
 * 49% of the triangles face away from the camera; keeping them out of
 * rasterisation saves nearly half the work. */
void rsx3d_set_culling(int enable)
{
    if (enable) {
        rsxSetCullFace(context, GCM_CULL_BACK);
        rsxSetCullFaceEnable(context, GCM_TRUE);
    } else {
        rsxSetCullFaceEnable(context, GCM_FALSE);
    }
}

gcmContextData *rsx3d_context(void)
{
    return context;
}

/* Waits until the GPU has finished the scene; after that writing to the
 * back buffer from the CPU is safe. Written without waiting, RSX is still
 * drawing over it and the HUD flickers. */
/* Screenshot: writes the current front buffer to disk as a PPM.
 *
 * To diagnose visual bugs (black textures, flipped faces, shifted UVs)
 * without looking at the emulator. It waits for RSX to finish the frame,
 * otherwise a half-drawn buffer is read. */
int rsx3d_capture(const char *path)
{
    const Buffer *b = &buffers[cur_buf ^ 1];   /* the most recently displayed buffer */
    FILE *f;
    u32 x, y;

    if (b->ptr == NULL)
        return -1;

    wait_rsx_idle();

    /* Measure whether the buffer actually holds anything: when a screenshot
     * comes out black, this tells a wrong buffer from an empty one. */
    {
        u32 mid = (b->height / 2) * b->width + (b->width / 2);

        ps3log("capture buf=%d %ux%u px[0]=%08X px[mid]=%08X other=%08X",
               cur_buf ^ 1, b->width, b->height,
               b->ptr[0], b->ptr[mid], buffers[cur_buf].ptr[mid]);
    }

    f = fopen(path, "wb");
    if (f == NULL)
        return -2;

    fprintf(f, "P6\n%u %u\n255\n", b->width, b->height);
    for (y = 0; y < b->height; y++) {
        for (x = 0; x < b->width; x++) {
            u32 px = b->ptr[y * b->width + x];
            unsigned char rgb[3];

            rgb[0] = (unsigned char)((px >> 16) & 0xFF);
            rgb[1] = (unsigned char)((px >> 8) & 0xFF);
            rgb[2] = (unsigned char)(px & 0xFF);
            fwrite(rgb, 1, 3, f);
        }
    }
    fclose(f);
    return 0;
}

u32 rsx3d_width(void)  { return scr_w; }
u32 rsx3d_height(void) { return scr_h; }
