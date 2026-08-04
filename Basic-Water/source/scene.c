#include <stdio.h>
#include <string.h>
#include <math.h>

#include <ppu-lv2.h>
#include <rsx/gcm_sys.h>
#include <rsx/rsx.h>

#include "scene.h"
#include "rsx3d.h"

/* Derlenmis Cg programlari (Makefile tarafindan binary olarak gomulur) */
#include "water_vpo.h"
#include "water_fpo.h"

#define SEA_SIZE      8000.0f   /* duzlemin kenar uzunlugu (birim) */
#define SEA_DIV       32        /* kenar basina bolme sayisi */
#define GRID_SPACING  50.0f     /* izgara araligi (birim) */
#define FOG_DISTANCE  2500.0f

#define VERT_COUNT  ((SEA_DIV + 1) * (SEA_DIV + 1))
#define INDEX_COUNT (SEA_DIV * SEA_DIV * 6)

typedef struct {
    float x, y, z;
} Vertex3;

static Vertex3 *vertices = NULL;
static u16     *indices = NULL;

static rsxVertexProgram   *vpo = (rsxVertexProgram *)water_vpo;
static rsxFragmentProgram *fpo = (rsxFragmentProgram *)water_fpo;

static void *vp_ucode = NULL;
static void *fp_ucode = NULL;
static u32  *fp_buffer = NULL;
static u32   fp_offset = 0;

/* vertex programi sabitleri */
static rsxProgramConst *c_row[4];
static rsxProgramConst *c_woff;

/* fragment programi sabitleri */
static rsxProgramConst *c_eye;
static rsxProgramConst *c_water;
static rsxProgramConst *c_grid;
static rsxProgramConst *c_sky;
static rsxProgramConst *c_spacing;
static rsxProgramConst *c_fog;

static int build_mesh(void)
{
    int ix, iz, i = 0;
    float step = SEA_SIZE / SEA_DIV;
    float half = SEA_SIZE * 0.5f;

    vertices = (Vertex3 *)rsxMemalign(128, VERT_COUNT * sizeof(Vertex3));
    indices  = (u16 *)rsxMemalign(128, INDEX_COUNT * sizeof(u16));
    if (vertices == NULL || indices == NULL)
        return -1;

    for (iz = 0; iz <= SEA_DIV; iz++) {
        for (ix = 0; ix <= SEA_DIV; ix++) {
            Vertex3 *v = &vertices[iz * (SEA_DIV + 1) + ix];
            v->x = -half + ix * step;
            v->y = WATER_LEVEL;
            v->z = -half + iz * step;
        }
    }

    for (iz = 0; iz < SEA_DIV; iz++) {
        for (ix = 0; ix < SEA_DIV; ix++) {
            u16 a = (u16)(iz * (SEA_DIV + 1) + ix);
            u16 b = (u16)(a + 1);
            u16 c = (u16)(a + (SEA_DIV + 1));
            u16 d = (u16)(c + 1);

            indices[i++] = a; indices[i++] = c; indices[i++] = b;
            indices[i++] = b; indices[i++] = c; indices[i++] = d;
        }
    }

    return 0;
}

static int load_shaders(void)
{
    u32 vpsize = 0, fpsize = 0;

    rsxVertexProgramGetUCode(vpo, &vp_ucode, &vpsize);
    c_row[0] = rsxVertexProgramGetConst(vpo, "mvpRow0");
    c_row[1] = rsxVertexProgramGetConst(vpo, "mvpRow1");
    c_row[2] = rsxVertexProgramGetConst(vpo, "mvpRow2");
    c_row[3] = rsxVertexProgramGetConst(vpo, "mvpRow3");
    c_woff   = rsxVertexProgramGetConst(vpo, "worldOffset");
    printf("vp const: rows=%p %p %p %p woff=%p\n",
           (void *)c_row[0], (void *)c_row[1], (void *)c_row[2],
           (void *)c_row[3], (void *)c_woff);

    rsxFragmentProgramGetUCode(fpo, &fp_ucode, &fpsize);
    fp_buffer = (u32 *)rsxMemalign(64, fpsize);
    if (fp_buffer == NULL)
        return -2;
    memcpy(fp_buffer, fp_ucode, fpsize);
    if (rsxAddressToOffset(fp_buffer, &fp_offset) != 0)
        return -3;

    c_eye     = rsxFragmentProgramGetConst(fpo, "eyePosition");
    c_water   = rsxFragmentProgramGetConst(fpo, "waterColor");
    c_grid    = rsxFragmentProgramGetConst(fpo, "gridColor");
    c_sky     = rsxFragmentProgramGetConst(fpo, "skyColor");
    c_spacing = rsxFragmentProgramGetConst(fpo, "gridSpacing");
    c_fog     = rsxFragmentProgramGetConst(fpo, "fogDistance");
    printf("fp const: eye=%p water=%p grid=%p sky=%p sp=%p fog=%p\n",
           (void *)c_eye, (void *)c_water, (void *)c_grid,
           (void *)c_sky, (void *)c_spacing, (void *)c_fog);

    return 0;
}

int scene_init(void)
{
    int rc;

    rc = build_mesh();
    if (rc != 0)
        return rc;

    return load_shaders();
}

void scene_draw(const Camera *cam, const Mat4 *proj)
{
    gcmContextData *ctx = rsx3d_context();
    Mat4 view = camera_view_matrix(cam);
    Mat4 model, mvp;
    float woff[3];
    int r;
    u32 offset;

    /* Sonsuz deniz hissi: duzlem kameranin altinda tutulur. Kaydirma izgara
     * araliginin katlarina yuvarlanir, boylece izgara dunyada sabit gorunur. */
    float sx = floorf(cam->pos[0] / GRID_SPACING) * GRID_SPACING;
    float sz = floorf(cam->pos[2] / GRID_SPACING) * GRID_SPACING;

    float eye[3]        = { cam->pos[0], cam->pos[1], cam->pos[2] };
    float water_col[3]  = { 0.05f, 0.20f, 0.42f };
    float grid_col[3]   = { 0.30f, 0.60f, 0.80f };
    float sky_col[3]    = { 0.486f, 0.659f, 0.941f };  /* SKY_CLEAR_COLOR ile ayni */
    float spacing       = GRID_SPACING;
    float fog           = FOG_DISTANCE;

    model = mat4_translation(sx, 0.0f, sz);

    {
        Mat4 pv = mat4_mul(proj, &view);
        mvp = mat4_mul(&pv, &model);
    }
    woff[0] = sx;
    woff[1] = 0.0f;
    woff[2] = sz;

    rsxAddressToOffset(&vertices[0].x, &offset);
    rsxBindVertexArrayAttrib(ctx, GCM_VERTEX_ATTRIB_POS, 0, offset,
                             sizeof(Vertex3), 3, GCM_VERTEX_DATA_TYPE_F32,
                             GCM_LOCATION_RSX);

    rsxLoadVertexProgram(ctx, vpo, vp_ucode);
    for (r = 0; r < 4; r++)
        if (c_row[r])
            rsxSetVertexProgramParameter(ctx, vpo, c_row[r], &mvp.m[r * 4]);
    if (c_woff) rsxSetVertexProgramParameter(ctx, vpo, c_woff, woff);

    rsxLoadFragmentProgramLocation(ctx, fpo, fp_offset, GCM_LOCATION_RSX);
    if (c_eye) rsxSetFragmentProgramParameter(ctx, fpo, c_eye,     eye,        fp_offset, GCM_LOCATION_RSX);
    if (c_water) rsxSetFragmentProgramParameter(ctx, fpo, c_water,   water_col,  fp_offset, GCM_LOCATION_RSX);
    if (c_grid) rsxSetFragmentProgramParameter(ctx, fpo, c_grid,    grid_col,   fp_offset, GCM_LOCATION_RSX);
    if (c_sky) rsxSetFragmentProgramParameter(ctx, fpo, c_sky,     sky_col,    fp_offset, GCM_LOCATION_RSX);
    if (c_spacing) rsxSetFragmentProgramParameter(ctx, fpo, c_spacing, &spacing,   fp_offset, GCM_LOCATION_RSX);
    if (c_fog) rsxSetFragmentProgramParameter(ctx, fpo, c_fog,     &fog,       fp_offset, GCM_LOCATION_RSX);

    rsxAddressToOffset(&indices[0], &offset);
    rsxDrawIndexArray(ctx, GCM_TYPE_TRIANGLES, offset, INDEX_COUNT,
                      GCM_INDEX_TYPE_16B, GCM_LOCATION_RSX);
}
