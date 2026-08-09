#include <string.h>
#include <math.h>

#include <ppu-lv2.h>
#include <rsx/gcm_sys.h>
#include <rsx/rsx.h>

#include "runway.h"
#include "rsx3d.h"

#include "solid_vpo.h"
#include "solid_fpo.h"

/* Kalkis ve inis pistleri. Yonleri farkli ki kalkis ile inis ayni
 * hissettirmesin. */
const float RUNWAY_POS[RUNWAY_COUNT][2] = {
    { -1500.0f,  1200.0f },     /* kalkis */
    {  1600.0f, -1400.0f }      /* inis */
};

static const float RUNWAY_ANGLE[RUNWAY_COUNT] = { 0.55f, -0.85f };

static const char *NAMES[RUNWAY_COUNT] = { "DEPARTURE", "ARRIVAL" };

const char *runway_name(int index)
{
    if (index < 0 || index >= RUNWAY_COUNT)
        return "";
    return NAMES[index];
}

typedef struct {
    float x, y, z;
    float r, g, b;
    float bright, pad;
} SolidVertex;

/* Her pist: platform + asfalt + esik cizgileri + orta cizgi parcalari */
#define STRIPES        9
#define QUADS_PER_RUNWAY (2 + 4 + STRIPES)
#define TOTAL_QUADS    (QUADS_PER_RUNWAY * RUNWAY_COUNT)
#define TOTAL_VERTS    (TOTAL_QUADS * 4)
#define TOTAL_INDICES  (TOTAL_QUADS * 6)

static SolidVertex *verts = NULL;
static u16 *idx = NULL;
static int quad_count = 0;

static rsxVertexProgram   *vp = (rsxVertexProgram *)solid_vpo;
static rsxFragmentProgram *fp = (rsxFragmentProgram *)solid_fpo;
static void *vp_ucode = NULL;
static void *fp_ucode = NULL;
static u32  *fp_buf = NULL;
static u32   fp_off = 0;

static rsxProgramConst *c_mvp, *c_eye, *c_hor, *c_fog;

/* Yatay bir dortgen ekler: merkez (cx,cz), boyut (w x l), yatayda donuk */
static void add_quad(float cx, float cz, float y, float w, float l,
                     float angle, float r, float g, float b, float bright)
{
    SolidVertex *v;
    float ca = cosf(angle), sa = sinf(angle);
    float hx = w * 0.5f, hz = l * 0.5f;
    float ox[4] = { -hx,  hx, -hx,  hx };
    float oz[4] = { -hz, -hz,  hz,  hz };
    int i;

    if (quad_count >= TOTAL_QUADS)
        return;

    v = &verts[quad_count * 4];

    for (i = 0; i < 4; i++) {
        v[i].x = cx + ox[i] * ca - oz[i] * sa;
        v[i].y = y;
        v[i].z = cz + ox[i] * sa + oz[i] * ca;
        v[i].r = r; v[i].g = g; v[i].b = b;
        v[i].bright = bright;
        v[i].pad = 0.0f;
    }

    quad_count++;
}

static void build_runway(int index)
{
    float cx = RUNWAY_POS[index][0];
    float cz = RUNWAY_POS[index][1];
    float a = RUNWAY_ANGLE[index];
    float y = RUNWAY_DECK_Y;
    float ca = cosf(a), sa = sinf(a);
    int i;

    /* platform: asfalttan biraz genis, koyu gri */
    add_quad(cx, cz, y - 0.6f, RUNWAY_WIDTH + 46.0f, RUNWAY_LENGTH + 60.0f, a,
             0.30f, 0.32f, 0.35f, 1.0f);

    /* asfalt */
    add_quad(cx, cz, y, RUNWAY_WIDTH, RUNWAY_LENGTH, a,
             0.13f, 0.14f, 0.16f, 1.0f);

    /* iki uctaki esik bantlari */
    for (i = 0; i < 2; i++) {
        float off = (i == 0 ? -1.0f : 1.0f) * (RUNWAY_LENGTH * 0.5f - 22.0f);

        add_quad(cx - sa * off, cz + ca * off, y + 0.05f,
                 RUNWAY_WIDTH - 14.0f, 12.0f, a, 0.92f, 0.93f, 0.95f, 1.0f);
    }

    /* kenar cizgileri */
    for (i = 0; i < 2; i++) {
        float off = (i == 0 ? -1.0f : 1.0f) * (RUNWAY_WIDTH * 0.5f - 5.0f);

        add_quad(cx + ca * off, cz + sa * off, y + 0.05f,
                 3.0f, RUNWAY_LENGTH - 30.0f, a, 0.85f, 0.86f, 0.88f, 1.0f);
    }

    /* orta cizgi: kesikli */
    for (i = 0; i < STRIPES; i++) {
        float t = ((float)i / (STRIPES - 1) - 0.5f) * (RUNWAY_LENGTH - 90.0f);

        add_quad(cx - sa * t, cz + ca * t, y + 0.05f,
                 4.0f, 30.0f, a, 0.95f, 0.95f, 0.95f, 1.0f);
    }
}

int runway_init(void)
{
    u32 sz = 0;
    int i;

    verts = (SolidVertex *)rsxMemalign(128, TOTAL_VERTS * sizeof(SolidVertex));
    idx   = (u16 *)rsxMemalign(128, TOTAL_INDICES * sizeof(u16));
    if (verts == NULL || idx == NULL)
        return -1;

    for (i = 0; i < TOTAL_QUADS; i++) {
        u16 v = (u16)(i * 4);

        idx[i * 6 + 0] = v;
        idx[i * 6 + 1] = (u16)(v + 1);
        idx[i * 6 + 2] = (u16)(v + 2);
        idx[i * 6 + 3] = (u16)(v + 2);
        idx[i * 6 + 4] = (u16)(v + 1);
        idx[i * 6 + 5] = (u16)(v + 3);
    }

    quad_count = 0;
    for (i = 0; i < RUNWAY_COUNT; i++)
        build_runway(i);

    rsxVertexProgramGetUCode(vp, &vp_ucode, &sz);
    c_mvp = rsxVertexProgramGetConst(vp, "mvpMatrix");

    rsxFragmentProgramGetUCode(fp, &fp_ucode, &sz);
    fp_buf = (u32 *)rsxMemalign(64, sz);
    if (fp_buf == NULL)
        return -2;
    memcpy(fp_buf, fp_ucode, sz);
    if (rsxAddressToOffset(fp_buf, &fp_off) != 0)
        return -3;

    c_eye = rsxFragmentProgramGetConst(fp, "eyePosition");
    c_hor = rsxFragmentProgramGetConst(fp, "horizonCol");
    c_fog = rsxFragmentProgramGetConst(fp, "fogDistance");

    return 0;
}

void runway_draw(const Camera *cam, const Mat4 *proj, const Atmosphere *atm)
{
    gcmContextData *ctx = rsx3d_context();
    Mat4 view = camera_view_matrix(cam);
    Mat4 mvp = mat4_mul(proj, &view);
    float eye[4], hor[4];
    float fog = atm->fog_distance;
    u32 off;
    int k;

    for (k = 0; k < 3; k++) {
        eye[k] = cam->pos[k];
        hor[k] = atm->horizon[k];
    }
    eye[3] = hor[3] = 0.0f;

    rsxAddressToOffset(&verts[0].x, &off);
    rsxBindVertexArrayAttrib(ctx, GCM_VERTEX_ATTRIB_POS, 0, off,
                             sizeof(SolidVertex), 3, GCM_VERTEX_DATA_TYPE_F32,
                             GCM_LOCATION_RSX);

    rsxAddressToOffset(&verts[0].r, &off);
    rsxBindVertexArrayAttrib(ctx, GCM_VERTEX_ATTRIB_NORMAL, 0, off,
                             sizeof(SolidVertex), 3, GCM_VERTEX_DATA_TYPE_F32,
                             GCM_LOCATION_RSX);

    rsxAddressToOffset(&verts[0].bright, &off);
    rsxBindVertexArrayAttrib(ctx, GCM_VERTEX_ATTRIB_TEX0, 0, off,
                             sizeof(SolidVertex), 2, GCM_VERTEX_DATA_TYPE_F32,
                             GCM_LOCATION_RSX);

    rsxLoadVertexProgram(ctx, vp, vp_ucode);
    if (c_mvp) rsxSetVertexProgramParameter(ctx, vp, c_mvp, mvp.m);

    if (c_eye) rsxSetFragmentProgramParameter(ctx, fp, c_eye, eye, fp_off, GCM_LOCATION_RSX);
    if (c_hor) rsxSetFragmentProgramParameter(ctx, fp, c_hor, hor, fp_off, GCM_LOCATION_RSX);
    if (c_fog) rsxSetFragmentProgramParameter(ctx, fp, c_fog, &fog, fp_off, GCM_LOCATION_RSX);

    rsxLoadFragmentProgramLocation(ctx, fp, fp_off, GCM_LOCATION_RSX);

    rsxSetUserClipPlaneControl(ctx,
                               GCM_USER_CLIP_PLANE_DISABLE,
                               GCM_USER_CLIP_PLANE_DISABLE,
                               GCM_USER_CLIP_PLANE_DISABLE,
                               GCM_USER_CLIP_PLANE_DISABLE,
                               GCM_USER_CLIP_PLANE_DISABLE,
                               GCM_USER_CLIP_PLANE_DISABLE);

    rsxAddressToOffset(&idx[0], &off);
    rsxDrawIndexArray(ctx, GCM_TYPE_TRIANGLES, off, quad_count * 6,
                      GCM_INDEX_TYPE_16B, GCM_LOCATION_RSX);
}
