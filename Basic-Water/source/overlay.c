#include <string.h>
#include <math.h>

#include <ppu-lv2.h>
#include <rsx/gcm_sys.h>
#include <rsx/rsx.h>

#include "overlay.h"
#include "rsx3d.h"

#include "overlay_vpo.h"
#include "overlay_fpo.h"

/* HUD ve menu, ekran tamponuna CPU ile yazilarak degil GPU'ya cizdirilerek
 * olusturulur. Sebep: emulator (ve genel olarak RSX) o an bagli olan render
 * hedefine CPU'dan yazilmasini kabul etmiyor ("Cannot invalidate a currently
 * bound render target"). Bu yuzden tum dikdortgenler bir tamponda toplanip
 * kare sonunda tek cizim cagrisiyla gonderilir. */

#define MAX_RECTS   1600
#define MAX_VERTS   (MAX_RECTS * 4)
#define MAX_INDICES (MAX_RECTS * 6)

typedef struct {
    float x, y, z;      /* clip uzayi konumu */
    float r, g, b;      /* renk */
    float a;            /* saydamlik      -> TEXCOORD0.x */
    float shape;        /* sekil modu     -> TEXCOORD0.y */
    float u, v;         /* kose UV        -> TEXCOORD0.zw */
    float inner;        /* halka ic yari. -> TEXCOORD1.x */
    float pad;          /* hizalama */
} OvlVertex;

static OvlVertex *verts = NULL;
static u16       *idx = NULL;
static int        rect_count = 0;

static rsxVertexProgram   *ovl_vp = (rsxVertexProgram *)overlay_vpo;
static rsxFragmentProgram *ovl_fp = (rsxFragmentProgram *)overlay_fpo;
static void *vp_ucode = NULL;
static void *fp_ucode = NULL;
static u32  *fp_buf = NULL;
static u32   fp_off = 0;

int overlay_init(void)
{
    u32 sz = 0;
    int i;

    verts = (OvlVertex *)rsxMemalign(128, MAX_VERTS * sizeof(OvlVertex));
    idx   = (u16 *)rsxMemalign(128, MAX_INDICES * sizeof(u16));
    if (verts == NULL || idx == NULL)
        return -1;

    /* indeks duzeni sabittir: her dikdortgen iki ucgen */
    for (i = 0; i < MAX_RECTS; i++) {
        u16 v = (u16)(i * 4);

        idx[i * 6 + 0] = v;
        idx[i * 6 + 1] = (u16)(v + 1);
        idx[i * 6 + 2] = (u16)(v + 2);
        idx[i * 6 + 3] = (u16)(v + 2);
        idx[i * 6 + 4] = (u16)(v + 1);
        idx[i * 6 + 5] = (u16)(v + 3);
    }

    rsxVertexProgramGetUCode(ovl_vp, &vp_ucode, &sz);

    rsxFragmentProgramGetUCode(ovl_fp, &fp_ucode, &sz);
    fp_buf = (u32 *)rsxMemalign(64, sz);
    if (fp_buf == NULL)
        return -2;
    memcpy(fp_buf, fp_ucode, sz);
    if (rsxAddressToOffset(fp_buf, &fp_off) != 0)
        return -3;

    return 0;
}

void overlay_begin(void)
{
    rect_count = 0;
}

static void push_quad(float cx, float cy, float hw, float hh, float angle,
                      color_t c, int alpha, float shape, float inner)
{
    OvlVertex *v;
    float r, g, b, a;
    float ca, sa;
    float ox[4], oy[4];
    int i;

    if (rect_count >= MAX_RECTS || hw <= 0.0f || hh <= 0.0f)
        return;

    /* kose ofsetleri (merkez etrafinda), gerekirse dondurulur */
    ox[0] = -hw; oy[0] = -hh;
    ox[1] =  hw; oy[1] = -hh;
    ox[2] = -hw; oy[2] =  hh;
    ox[3] =  hw; oy[3] =  hh;

    ca = cosf(angle);
    sa = sinf(angle);

    r = ((c >> 16) & 0xFF) / 255.0f;
    g = ((c >> 8) & 0xFF) / 255.0f;
    b = (c & 0xFF) / 255.0f;
    a = alpha / 255.0f;

    v = &verts[rect_count * 4];

    for (i = 0; i < 4; i++) {
        float px = cx + ox[i] * ca - oy[i] * sa;
        float py = cy + ox[i] * sa + oy[i] * ca;

        /* sanal 1280x720 -> clip uzayi (-1..1), y yukari pozitif */
        v[i].x = px / OVL_W * 2.0f - 1.0f;
        v[i].y = 1.0f - py / OVL_H * 2.0f;
        v[i].z = 0.0f;
        v[i].r = r; v[i].g = g; v[i].b = b;
        v[i].a = a; v[i].shape = shape;
        v[i].u = (i & 1) ? 1.0f : 0.0f;
        v[i].v = (i & 2) ? 1.0f : 0.0f;
        v[i].inner = inner;
    }

    rect_count++;
}

static void push_rect_shaped(int x, int y, int w, int h, color_t c, int alpha,
                             float shape)
{
    if (w <= 0 || h <= 0)
        return;

    push_quad(x + w * 0.5f, y + h * 0.5f, w * 0.5f, h * 0.5f, 0.0f,
              c, alpha, shape, 0.0f);
}

void overlay_fill_rect(int x, int y, int w, int h, color_t c)
{
    push_rect_shaped(x, y, w, h, c, 255, 0.0f);
}

void overlay_blend_rect(int x, int y, int w, int h, color_t c, int alpha)
{
    push_rect_shaped(x, y, w, h, c, alpha, 0.0f);
}

void overlay_soft_blob(int x, int y, int w, int h, color_t c, int alpha)
{
    push_rect_shaped(x, y, w, h, c, alpha, 1.0f);
}

void overlay_rot_rect(float cx, float cy, float w, float h, float angle,
                      color_t c, int alpha)
{
    push_quad(cx, cy, w * 0.5f, h * 0.5f, angle, c, alpha, 0.0f, 0.0f);
}

void overlay_ring(float cx, float cy, float radius, float thickness,
                  color_t c, int alpha)
{
    float inner;

    if (radius <= 0.0f || thickness <= 0.0f)
        return;

    inner = (radius - thickness) / radius;
    if (inner < 0.0f)  inner = 0.0f;
    if (inner > 0.95f) inner = 0.95f;

    push_quad(cx, cy, radius, radius, 0.0f, c, alpha, 2.0f, inner);
}

void overlay_disc(float cx, float cy, float radius, color_t c, int alpha)
{
    push_quad(cx, cy, radius, radius, 0.0f, c, alpha, 2.0f, 0.0f);
}

void overlay_flush(void)
{
    gcmContextData *ctx = rsx3d_context();
    u32 off;

    if (rect_count <= 0)
        return;

    /* saydamlik icin harmanlama, derinlik testi kapali (her sey en ustte) */
    rsxSetBlendEnable(ctx, GCM_TRUE);
    rsxSetBlendFunc(ctx, GCM_SRC_ALPHA, GCM_ONE_MINUS_SRC_ALPHA,
                    GCM_SRC_ALPHA, GCM_ONE_MINUS_SRC_ALPHA);
    rsxSetBlendEquation(ctx, GCM_FUNC_ADD, GCM_FUNC_ADD);
    rsxSetDepthTestEnable(ctx, GCM_FALSE);
    rsxSetDepthWriteEnable(ctx, 0);

    rsxAddressToOffset(&verts[0].x, &off);
    rsxBindVertexArrayAttrib(ctx, GCM_VERTEX_ATTRIB_POS, 0, off,
                             sizeof(OvlVertex), 3, GCM_VERTEX_DATA_TYPE_F32,
                             GCM_LOCATION_RSX);

    rsxAddressToOffset(&verts[0].r, &off);
    rsxBindVertexArrayAttrib(ctx, GCM_VERTEX_ATTRIB_NORMAL, 0, off,
                             sizeof(OvlVertex), 3, GCM_VERTEX_DATA_TYPE_F32,
                             GCM_LOCATION_RSX);

    rsxAddressToOffset(&verts[0].a, &off);
    rsxBindVertexArrayAttrib(ctx, GCM_VERTEX_ATTRIB_TEX0, 0, off,
                             sizeof(OvlVertex), 4, GCM_VERTEX_DATA_TYPE_F32,
                             GCM_LOCATION_RSX);

    rsxAddressToOffset(&verts[0].inner, &off);
    rsxBindVertexArrayAttrib(ctx, GCM_VERTEX_ATTRIB_TEX1, 0, off,
                             sizeof(OvlVertex), 1, GCM_VERTEX_DATA_TYPE_F32,
                             GCM_LOCATION_RSX);

    rsxLoadVertexProgram(ctx, ovl_vp, vp_ucode);
    rsxLoadFragmentProgramLocation(ctx, ovl_fp, fp_off, GCM_LOCATION_RSX);

    rsxSetUserClipPlaneControl(ctx,
                               GCM_USER_CLIP_PLANE_DISABLE,
                               GCM_USER_CLIP_PLANE_DISABLE,
                               GCM_USER_CLIP_PLANE_DISABLE,
                               GCM_USER_CLIP_PLANE_DISABLE,
                               GCM_USER_CLIP_PLANE_DISABLE,
                               GCM_USER_CLIP_PLANE_DISABLE);

    rsxAddressToOffset(&idx[0], &off);
    rsxDrawIndexArray(ctx, GCM_TYPE_TRIANGLES, off, rect_count * 6,
                      GCM_INDEX_TYPE_16B, GCM_LOCATION_RSX);

    rsxSetBlendEnable(ctx, GCM_FALSE);
    rsxSetDepthTestEnable(ctx, GCM_TRUE);
    rsxSetDepthWriteEnable(ctx, 1);
}
