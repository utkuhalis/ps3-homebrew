#include <string.h>
#include <math.h>

#include <ppu-lv2.h>
#include <rsx/gcm_sys.h>
#include <rsx/rsx.h>

#include "runway.h"
#include "texture.h"
#include "rsx3d.h"

#include "tex_vpo.h"
#include "tex_fpo.h"

/* Departure and arrival runways. Their headings differ so that departure
 * and arrival do not feel the same. */
const float RUNWAY_POS[RUNWAY_COUNT][2] = {
    { -1500.0f,  1200.0f },     /* departure */
    {  1600.0f, -1400.0f }      /* arrival */
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
    float u, v;                 /* texture coordinate */
} SolidVertex;

/* Each runway: platform + asphalt + threshold bars + centre line segments */
#define STRIPES        9
/* grass + shoulder + asphalt + 2 thresholds + 2 edges + centre lines */
#define QUADS_PER_RUNWAY (3 + 4 + STRIPES)
#define TOTAL_QUADS    (QUADS_PER_RUNWAY * RUNWAY_COUNT)
#define TOTAL_VERTS    (TOTAL_QUADS * 4)
#define TOTAL_INDICES  (TOTAL_QUADS * 6)

static SolidVertex *verts = NULL;
static u16 *idx = NULL;
static int quad_count = 0;

static rsxVertexProgram   *vp = (rsxVertexProgram *)tex_vpo;
static rsxFragmentProgram *fp = (rsxFragmentProgram *)tex_fpo;
static void *vp_ucode = NULL;
static void *fp_ucode = NULL;
static u32  *fp_buf = NULL;
static u32   fp_off = 0;

static rsxProgramConst *c_mvp, *c_eye, *c_hor, *c_fog;
static int tex_unit = 0;
static int tex_index = -1;

/* Adds a horizontal quad: centre (cx,cz), size (w x l), rotated in the plane */
/* How many world units one texture tile covers. Smaller = more repeats. */
#define TEX_WORLD_SIZE 9.0f

/* Surface types: the shader generates the pattern from these.
 * 0 flat colour (lines, markings), 1 asphalt, 2 grass, 3 concrete */
#define SURF_PLAIN    0.0f
#define SURF_ASPHALT  1.0f
#define SURF_GRASS    2.0f
#define SURF_CONCRETE 3.0f

static float quad_surface = SURF_PLAIN;

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
        v[i].pad = quad_surface;

        /* UVs are in world scale: the pattern repeats at the same rate whatever
         * the piece's size, so the runway and its surroundings do not show the
         * same pattern at different scales. */
        v[i].u = (cx + ox[i] * ca - oz[i] * sa) / TEX_WORLD_SIZE;
        v[i].v = (cz + ox[i] * sa + oz[i] * ca) / TEX_WORLD_SIZE;
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

    /* Surrounding land: on its own the runway looked like a strip floating on
     * the sea. A wide grass field makes it look like an airfield. */
    quad_surface = SURF_GRASS;
    add_quad(cx, cz, y - 1.2f, RUNWAY_WIDTH + 520.0f, RUNWAY_LENGTH + 700.0f,
             a, 0.34f, 0.42f, 0.24f, 1.0f);

    /* shoulder: concrete strips on both sides of the asphalt */
    quad_surface = SURF_CONCRETE;
    add_quad(cx, cz, y - 0.6f, RUNWAY_WIDTH + 46.0f, RUNWAY_LENGTH + 60.0f, a,
             0.52f, 0.53f, 0.55f, 1.0f);

    /* Asphalt. The old colour (0.13) was nearly black; real asphalt is
     * clearly lighter and has texture. */
    quad_surface = SURF_ASPHALT;
    add_quad(cx, cz, y, RUNWAY_WIDTH, RUNWAY_LENGTH, a,
             0.30f, 0.31f, 0.33f, 1.0f);

    /* markings stay flat colour */
    quad_surface = SURF_PLAIN;

    /* threshold bands at both ends */
    for (i = 0; i < 2; i++) {
        float off = (i == 0 ? -1.0f : 1.0f) * (RUNWAY_LENGTH * 0.5f - 22.0f);

        add_quad(cx - sa * off, cz + ca * off, y + 0.05f,
                 RUNWAY_WIDTH - 14.0f, 12.0f, a, 0.92f, 0.93f, 0.95f, 1.0f);
    }

    /* edge lines */
    for (i = 0; i < 2; i++) {
        float off = (i == 0 ? -1.0f : 1.0f) * (RUNWAY_WIDTH * 0.5f - 5.0f);

        add_quad(cx + ca * off, cz + sa * off, y + 0.05f,
                 3.0f, RUNWAY_LENGTH - 30.0f, a, 0.85f, 0.86f, 0.88f, 1.0f);
    }

    /* centre line: dashed */
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

    /* The shader has a single sampler, which lands on unit 0.
     * (rsxFragmentProgramGetAttribIndex is declared in the header but not
     * defined in librsx, so it cannot be queried.) */
    tex_unit = 0;

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
                             sizeof(SolidVertex), 4, GCM_VERTEX_DATA_TYPE_F32,
                             GCM_LOCATION_RSX);

    rsxLoadVertexProgram(ctx, vp, vp_ucode);
    if (c_mvp) rsxSetVertexProgramParameter(ctx, vp, c_mvp, mvp.m);

    if (c_eye) rsxSetFragmentProgramParameter(ctx, fp, c_eye, eye, fp_off, GCM_LOCATION_RSX);
    if (c_hor) rsxSetFragmentProgramParameter(ctx, fp, c_hor, hor, fp_off, GCM_LOCATION_RSX);
    if (c_fog) rsxSetFragmentProgramParameter(ctx, fp, c_fog, &fog, fp_off, GCM_LOCATION_RSX);

    rsxLoadFragmentProgramLocation(ctx, fp, fp_off, GCM_LOCATION_RSX);

    if (tex_index < 0)
        tex_index = texture_find("runway");
    texture_bind(tex_unit, tex_index);

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
