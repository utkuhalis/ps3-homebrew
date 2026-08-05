#include <stdio.h>
#include <string.h>
#include <math.h>

#include <ppu-lv2.h>
#include <rsx/gcm_sys.h>
#include <rsx/rsx.h>

#include "scene.h"
#include "rsx3d.h"
#include "skycolor.h"

/* Derleme sirasinda gomulen Cg programlari */
#include "sky_vpo.h"
#include "sky_fpo.h"
#include "water_vpo.h"
#include "water_fpo.h"

/* Dalga ornekleme kurali: izgara hucresi, en kucuk geometri dalgasinin
 * yariisindan buyuk olmamali. 3000/96 = 31 birim hucre; vertex'te tasinan
 * dalgalar 140 ve 74 birim oldugu icin rahatca ornekleniyor. Daha kucuk
 * dalgalar geometride degil, fragment'ta normal olarak hesaplanir. */
#define SEA_SIZE      4000.0f
#define SEA_DIV       100
#define GRID_SPACING  50.0f     /* duzlem kaydirma adimi */
#define FOG_DISTANCE  1500.0f

#define VERT_COUNT  ((SEA_DIV + 1) * (SEA_DIV + 1))
#define INDEX_COUNT (SEA_DIV * SEA_DIV * 6)

/* Calisan referans ornekle ayni vertex duzeni: konum + normal + doku
 * koordinati. Gokyuzu dortgeninde "normal" alani bakis yonunu tasir. */
typedef struct {
    float x, y, z;
    float nx, ny, nz;
    float u, v;
} Vertex3;

static Vertex3 *sea_verts = NULL;
static u16     *sea_idx = NULL;
static Vertex3 *sky_verts = NULL;
static u16     *sky_idx = NULL;

static rsxVertexProgram   *water_vp = (rsxVertexProgram *)water_vpo;
static rsxFragmentProgram *water_fp = (rsxFragmentProgram *)water_fpo;
static rsxVertexProgram   *sky_vp = (rsxVertexProgram *)sky_vpo;
static rsxFragmentProgram *sky_fp = (rsxFragmentProgram *)sky_fpo;

static void *water_vp_ucode = NULL;
static void *water_fp_ucode = NULL;
static u32  *water_fp_buf = NULL;
static u32   water_fp_off = 0;

static void *sky_vp_ucode = NULL;
static void *sky_fp_ucode = NULL;
static u32  *sky_fp_buf = NULL;
static u32   sky_fp_off = 0;

/* su programi sabitleri */
static rsxProgramConst *w_mvp, *w_time_v, *w_woff;
static rsxProgramConst *w_eye, *w_sun, *w_time_f, *w_fog;
/* gokyuzu programi sabitleri */
static rsxProgramConst *s_sun, *s_time;
static rsxProgramConst *s_fwd, *s_right, *s_up, *s_tan;

static int build_sea(void)
{
    int ix, iz, i = 0;
    float step = SEA_SIZE / SEA_DIV;
    float half = SEA_SIZE * 0.5f;

    sea_verts = (Vertex3 *)rsxMemalign(128, VERT_COUNT * sizeof(Vertex3));
    sea_idx   = (u16 *)rsxMemalign(128, INDEX_COUNT * sizeof(u16));
    if (sea_verts == NULL || sea_idx == NULL)
        return -1;

    for (iz = 0; iz <= SEA_DIV; iz++) {
        for (ix = 0; ix <= SEA_DIV; ix++) {
            Vertex3 *v = &sea_verts[iz * (SEA_DIV + 1) + ix];

            v->x = -half + ix * step;
            v->y = WATER_LEVEL;
            v->z = -half + iz * step;
            v->nx = 0.0f; v->ny = 1.0f; v->nz = 0.0f;
            v->u = (float)ix / SEA_DIV;
            v->v = (float)iz / SEA_DIV;
        }
    }

    for (iz = 0; iz < SEA_DIV; iz++) {
        for (ix = 0; ix < SEA_DIV; ix++) {
            u16 a = (u16)(iz * (SEA_DIV + 1) + ix);
            u16 b = (u16)(a + 1);
            u16 c = (u16)(a + (SEA_DIV + 1));
            u16 d = (u16)(c + 1);

            sea_idx[i++] = a; sea_idx[i++] = c; sea_idx[i++] = b;
            sea_idx[i++] = b; sea_idx[i++] = c; sea_idx[i++] = d;
        }
    }
    return 0;
}

static int build_sky_quad(void)
{
    static const float qx[4] = { -1.0f,  1.0f, -1.0f, 1.0f };
    static const float qy[4] = { -1.0f, -1.0f,  1.0f, 1.0f };
    int i;

    sky_verts = (Vertex3 *)rsxMemalign(128, 4 * sizeof(Vertex3));
    sky_idx   = (u16 *)rsxMemalign(128, 6 * sizeof(u16));
    if (sky_verts == NULL || sky_idx == NULL)
        return -1;

    for (i = 0; i < 4; i++) {
        sky_verts[i].x = qx[i];
        sky_verts[i].y = qy[i];
        sky_verts[i].z = 0.0f;
        sky_verts[i].nx = 0.0f;
        sky_verts[i].ny = 0.0f;
        sky_verts[i].nz = -1.0f;
        sky_verts[i].u = 0.0f;
        sky_verts[i].v = 0.0f;
    }

    sky_idx[0] = 0; sky_idx[1] = 1; sky_idx[2] = 2;
    sky_idx[3] = 2; sky_idx[4] = 1; sky_idx[5] = 3;
    return 0;
}

static int load_programs(void)
{
    u32 sz = 0;

    /* --- su --- */
    rsxVertexProgramGetUCode(water_vp, &water_vp_ucode, &sz);
    w_mvp    = rsxVertexProgramGetConst(water_vp, "mvpMatrix");
    w_time_v = rsxVertexProgramGetConst(water_vp, "timeSec");
    w_woff   = rsxVertexProgramGetConst(water_vp, "worldOffset");

    rsxFragmentProgramGetUCode(water_fp, &water_fp_ucode, &sz);
    water_fp_buf = (u32 *)rsxMemalign(64, sz);
    if (water_fp_buf == NULL)
        return -1;
    memcpy(water_fp_buf, water_fp_ucode, sz);
    if (rsxAddressToOffset(water_fp_buf, &water_fp_off) != 0)
        return -2;

    w_eye    = rsxFragmentProgramGetConst(water_fp, "eyePosition");
    w_sun    = rsxFragmentProgramGetConst(water_fp, "sunDir");
    w_time_f = rsxFragmentProgramGetConst(water_fp, "timeSec");
    w_fog    = rsxFragmentProgramGetConst(water_fp, "fogDistance");

    /* --- gokyuzu --- */
    rsxVertexProgramGetUCode(sky_vp, &sky_vp_ucode, &sz);

    rsxFragmentProgramGetUCode(sky_fp, &sky_fp_ucode, &sz);
    sky_fp_buf = (u32 *)rsxMemalign(64, sz);
    if (sky_fp_buf == NULL)
        return -3;
    memcpy(sky_fp_buf, sky_fp_ucode, sz);
    if (rsxAddressToOffset(sky_fp_buf, &sky_fp_off) != 0)
        return -4;

    s_sun  = rsxFragmentProgramGetConst(sky_fp, "sunDir");
    s_time = rsxFragmentProgramGetConst(sky_fp, "timeSec");

    s_fwd   = rsxVertexProgramGetConst(sky_vp, "camFwd");
    s_right = rsxVertexProgramGetConst(sky_vp, "camRight");
    s_up    = rsxVertexProgramGetConst(sky_vp, "camUp");
    s_tan   = rsxVertexProgramGetConst(sky_vp, "tanHalf");

    printf("su: mvp=%p tv=%p eye=%p sun=%p fog=%p | gok: sun=%p time=%p\n",
           (void *)w_mvp, (void *)w_time_v, (void *)w_eye, (void *)w_sun,
           (void *)w_fog, (void *)s_sun, (void *)s_time);
    return 0;
}

int scene_init(void)
{
    int rc = build_sea();

    if (rc != 0)
        return rc;
    rc = build_sky_quad();
    if (rc != 0)
        return rc;

    return load_programs();
}

/* Uc oznitelik de baglanir (calisan referans kod boyle yapiyor). */
static void bind_attribs(const Vertex3 *verts)
{
    gcmContextData *ctx = rsx3d_context();
    u32 off;

    rsxAddressToOffset((void *)&verts[0].x, &off);
    rsxBindVertexArrayAttrib(ctx, GCM_VERTEX_ATTRIB_POS, 0, off,
                             sizeof(Vertex3), 3, GCM_VERTEX_DATA_TYPE_F32,
                             GCM_LOCATION_RSX);

    rsxAddressToOffset((void *)&verts[0].nx, &off);
    rsxBindVertexArrayAttrib(ctx, GCM_VERTEX_ATTRIB_NORMAL, 0, off,
                             sizeof(Vertex3), 3, GCM_VERTEX_DATA_TYPE_F32,
                             GCM_LOCATION_RSX);

    rsxAddressToOffset((void *)&verts[0].u, &off);
    rsxBindVertexArrayAttrib(ctx, GCM_VERTEX_ATTRIB_TEX0, 0, off,
                             sizeof(Vertex3), 2, GCM_VERTEX_DATA_TYPE_F32,
                             GCM_LOCATION_RSX);
}

static void disable_user_clip(void)
{
    rsxSetUserClipPlaneControl(rsx3d_context(),
                               GCM_USER_CLIP_PLANE_DISABLE,
                               GCM_USER_CLIP_PLANE_DISABLE,
                               GCM_USER_CLIP_PLANE_DISABLE,
                               GCM_USER_CLIP_PLANE_DISABLE,
                               GCM_USER_CLIP_PLANE_DISABLE,
                               GCM_USER_CLIP_PLANE_DISABLE);
}

/* Gokyuzu: ekrani kaplayan dortgen. Kose bakis yonleri kameradan turetilir
 * ve "normal" ozniteliginde tasinir. */
static void draw_sky(const Camera *cam, float time)
{
    gcmContextData *ctx = rsx3d_context();
    float fwd[3], right[3], up[3];
    float tan_half = tanf(60.0f * 3.14159265f / 180.0f * 0.5f);
    float aspect = rsx3d_aspect();
    float sun[4], tan_xy[4], f4[4], r4[4], u4[4];
    u32 off;
    int i, k;

    camera_forward(cam, fwd);
    camera_right(cam, right);
    vec3_cross(up, right, fwd);
    vec3_normalize(up);

    tan_xy[0] = tan_half * aspect;
    tan_xy[1] = tan_half;
    tan_xy[2] = 0.0f;
    tan_xy[3] = 0.0f;

    for (k = 0; k < 3; k++) {
        f4[k] = fwd[k];
        r4[k] = right[k];
        u4[k] = up[k];
        sun[k] = SUN_DIR[k];
    }
    f4[3] = r4[3] = u4[3] = 0.0f;
    sun[3] = 0.0f;
    (void)i;

    /* Gokyuzu her seyin arkasindadir: derinlik testi ve yazimi kapatilir.
     * Aksi halde temizleme degeriyle karsilastirilip elenebiliyor ve ekranda
     * yalnizca temizleme rengi kaliyor. */
    rsxSetDepthTestEnable(ctx, GCM_FALSE);
    rsxSetDepthWriteEnable(ctx, 0);

    bind_attribs(sky_verts);
    rsxLoadVertexProgram(ctx, sky_vp, sky_vp_ucode);

    if (s_fwd)   rsxSetVertexProgramParameter(ctx, sky_vp, s_fwd,   f4);
    if (s_right) rsxSetVertexProgramParameter(ctx, sky_vp, s_right, r4);
    if (s_up)    rsxSetVertexProgramParameter(ctx, sky_vp, s_up,    u4);
    if (s_tan)   rsxSetVertexProgramParameter(ctx, sky_vp, s_tan,   tan_xy);

    if (s_sun)  rsxSetFragmentProgramParameter(ctx, sky_fp, s_sun,  sun,   sky_fp_off, GCM_LOCATION_RSX);
    if (s_time) rsxSetFragmentProgramParameter(ctx, sky_fp, s_time, &time, sky_fp_off, GCM_LOCATION_RSX);

    rsxLoadFragmentProgramLocation(ctx, sky_fp, sky_fp_off, GCM_LOCATION_RSX);
    disable_user_clip();

    rsxAddressToOffset(&sky_idx[0], &off);
    rsxDrawIndexArray(ctx, GCM_TYPE_TRIANGLES, off, 6,
                      GCM_INDEX_TYPE_16B, GCM_LOCATION_RSX);

    /* su icin derinlik testini geri ac */
    rsxSetDepthTestEnable(ctx, GCM_TRUE);
    rsxSetDepthWriteEnable(ctx, 1);
}

static void draw_water(const Camera *cam, const Mat4 *proj, float time)
{
    gcmContextData *ctx = rsx3d_context();
    Mat4 view = camera_view_matrix(cam);
    Mat4 model, mv, mvp;
    float eye[4], sun[4];
    float fog = FOG_DISTANCE;
    u32 off;

    /* Duzlem kamerayla birlikte kayar; kaydirma izgara adiminin katlarina
     * yuvarlanir ki dalga deseni dunyada sabit kalsin. */
    float sx = floorf(cam->pos[0] / GRID_SPACING) * GRID_SPACING;
    float sz = floorf(cam->pos[2] / GRID_SPACING) * GRID_SPACING;

    model = mat4_translation(sx, 0.0f, sz);
    mv    = mat4_mul(&view, &model);
    mvp   = mat4_mul(proj, &mv);

    eye[0] = cam->pos[0]; eye[1] = cam->pos[1]; eye[2] = cam->pos[2]; eye[3] = 0.0f;
    sun[0] = SUN_DIR[0];  sun[1] = SUN_DIR[1];  sun[2] = SUN_DIR[2];  sun[3] = 0.0f;

    bind_attribs(sea_verts);

    rsxLoadVertexProgram(ctx, water_vp, water_vp_ucode);
    if (w_mvp)    rsxSetVertexProgramParameter(ctx, water_vp, w_mvp, mvp.m);
    if (w_time_v) rsxSetVertexProgramParameter(ctx, water_vp, w_time_v, &time);
    if (w_woff) {
        float woff[2];
        woff[0] = sx;
        woff[1] = sz;
        rsxSetVertexProgramParameter(ctx, water_vp, w_woff, woff);
    }

    if (w_eye)    rsxSetFragmentProgramParameter(ctx, water_fp, w_eye,    eye,   water_fp_off, GCM_LOCATION_RSX);
    if (w_sun)    rsxSetFragmentProgramParameter(ctx, water_fp, w_sun,    sun,   water_fp_off, GCM_LOCATION_RSX);
    if (w_time_f) rsxSetFragmentProgramParameter(ctx, water_fp, w_time_f, &time, water_fp_off, GCM_LOCATION_RSX);
    if (w_fog)    rsxSetFragmentProgramParameter(ctx, water_fp, w_fog,    &fog,  water_fp_off, GCM_LOCATION_RSX);

    rsxLoadFragmentProgramLocation(ctx, water_fp, water_fp_off, GCM_LOCATION_RSX);
    disable_user_clip();

    rsxAddressToOffset(&sea_idx[0], &off);
    rsxDrawIndexArray(ctx, GCM_TYPE_TRIANGLES, off, INDEX_COUNT,
                      GCM_INDEX_TYPE_16B, GCM_LOCATION_RSX);
}

void scene_draw(const Camera *cam, const Mat4 *proj, float time)
{
    draw_sky(cam, time);
    draw_water(cam, proj, time);
}
