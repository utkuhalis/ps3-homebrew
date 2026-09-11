#include <string.h>
#include <math.h>

#include <ppu-lv2.h>
#include <rsx/gcm_sys.h>
#include <rsx/rsx.h>

#include "overlay.h"
#include "rsx3d.h"

#include "overlay_vpo.h"
#include "overlay_fpo.h"

/* The HUD and menu are not written into the framebuffer by the CPU but
 * drawn by the GPU. The reason: the emulator (and RSX in general) refuses
 * CPU writes to the currently bound render target ("Cannot invalidate a
 * currently bound render target"). So every primitive is collected in a
 * buffer and sent in one draw call at the end of the frame. */

/* Budgets. Text is now triangle meshes (about 57 vertices and 250 indices
 * per glyph), so vertices and indices are counted separately and indices are
 * 32-bit: a full HUD with the menu open passes the 65536 limit of 16-bit
 * indices. */
#define MAX_VERTS   96000
#define MAX_INDICES 240000

typedef struct {
    float x, y, z;      /* clip-space position */
    float r, g, b;      /* colour */
    float a;            /* opacity        -> TEXCOORD0.x */
    float shape;        /* shape          -> TEXCOORD0.y */
    float u, v;         /* corner UV      -> TEXCOORD0.zw */
    float inner;        /* ring inner rad -> TEXCOORD1.x */
    float pad;          /* alignment */
} OvlVertex;

static OvlVertex *verts = NULL;
static u32       *idx = NULL;
static int        vert_count = 0;
static int        idx_count = 0;
static int        prim_count = 0;    /* quads and meshes, for the profiler */
static int        overflow = 0;

static rsxVertexProgram   *ovl_vp = (rsxVertexProgram *)overlay_vpo;
static rsxFragmentProgram *ovl_fp = (rsxFragmentProgram *)overlay_fpo;
static void *vp_ucode = NULL;
static void *fp_ucode = NULL;
static u32  *fp_buf = NULL;
static u32   fp_off = 0;

int overlay_init(void)
{
    u32 sz = 0;

    verts = (OvlVertex *)rsxMemalign(128, MAX_VERTS * sizeof(OvlVertex));
    idx   = (u32 *)rsxMemalign(128, MAX_INDICES * sizeof(u32));
    if (verts == NULL || idx == NULL)
        return -1;

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
    vert_count = 0;
    idx_count = 0;
    prim_count = 0;
    overflow = 0;
}

unsigned int overlay_rect_count(void)
{
    return (unsigned int)prim_count;
}

int overlay_overflowed(void)
{
    return overflow;
}

static void push_quad(float cx, float cy, float hw, float hh, float angle,
                      color_t c, int alpha, float shape, float inner)
{
    OvlVertex *v;
    float r, g, b, a;
    float ca, sa;
    float ox[4], oy[4];
    int i;

    if (hw <= 0.0f || hh <= 0.0f)
        return;

    if (vert_count + 4 > MAX_VERTS || idx_count + 6 > MAX_INDICES) {
        overflow = 1;           /* flag it rather than draw something incomplete without a trace */
        return;
    }

    /* corner offsets (around the centre), rotated if needed */
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

    v = &verts[vert_count];

    for (i = 0; i < 4; i++) {
        float px = cx + ox[i] * ca - oy[i] * sa;
        float py = cy + ox[i] * sa + oy[i] * ca;

        /* virtual 1280x720 -> clip space (-1..1), y positive up */
        v[i].x = px / OVL_W * 2.0f - 1.0f;
        v[i].y = 1.0f - py / OVL_H * 2.0f;
        v[i].z = 0.0f;
        v[i].r = r; v[i].g = g; v[i].b = b;
        v[i].a = a; v[i].shape = shape;
        v[i].u = (i & 1) ? 1.0f : 0.0f;
        v[i].v = (i & 2) ? 1.0f : 0.0f;
        v[i].inner = inner;
    }

    idx[idx_count + 0] = (u32)vert_count;
    idx[idx_count + 1] = (u32)vert_count + 1;
    idx[idx_count + 2] = (u32)vert_count + 2;
    idx[idx_count + 3] = (u32)vert_count + 2;
    idx[idx_count + 4] = (u32)vert_count + 1;
    idx[idx_count + 5] = (u32)vert_count + 3;
    vert_count += 4;
    idx_count += 6;
    prim_count++;
}

void overlay_tris(const OvlPoint *p, int np, const unsigned short *tri, int ni,
                  color_t c, int alpha)
{
    float r = ((c >> 16) & 0xFF) / 255.0f;
    float g = ((c >> 8) & 0xFF) / 255.0f;
    float b = (c & 0xFF) / 255.0f;
    float a = alpha / 255.0f;
    OvlVertex *v;
    int i;

    if (np <= 0 || ni <= 0)
        return;
    if (vert_count + np > MAX_VERTS || idx_count + ni > MAX_INDICES) {
        overflow = 1;
        return;
    }

    v = &verts[vert_count];
    for (i = 0; i < np; i++) {
        v[i].x = p[i].x / OVL_W * 2.0f - 1.0f;
        v[i].y = 1.0f - p[i].y / OVL_H * 2.0f;
        v[i].z = 0.0f;
        v[i].r = r; v[i].g = g; v[i].b = b;
        v[i].a = p[i].a * a;
        v[i].shape = 0.0f;                  /* plain fill: alpha is the vertex's */
        v[i].u = 0.0f; v[i].v = 0.0f;
        v[i].inner = 0.0f;
    }
    for (i = 0; i < ni; i++)
        idx[idx_count + i] = (u32)(vert_count + tri[i]);

    vert_count += np;
    idx_count += ni;
    prim_count++;
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

    if (idx_count <= 0)
        return;

    /* Glyph triangles come out of the triangulator in either winding. */
    rsx3d_set_culling(0);

    /* blending for transparency, depth test off (everything on top) */
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
    rsxDrawIndexArray(ctx, GCM_TYPE_TRIANGLES, off, idx_count,
                      GCM_INDEX_TYPE_32B, GCM_LOCATION_RSX);

    rsxSetBlendEnable(ctx, GCM_FALSE);
    rsxSetDepthTestEnable(ctx, GCM_TRUE);
    rsxSetDepthWriteEnable(ctx, 1);
}
