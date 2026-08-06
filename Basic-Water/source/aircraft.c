#include <string.h>
#include <math.h>

#include <ppu-lv2.h>
#include <rsx/gcm_sys.h>
#include <rsx/rsx.h>

#include "aircraft.h"
#include "rsx3d.h"

#include "solid_vpo.h"
#include "solid_fpo.h"

/* Ucak yerel koordinat sistemi:
 *   +X sag kanat, +Y yukari, -Z burun yonu (kamera ile ayni kural) */

#define MAX_QUADS 96
#define MAX_VERTS (MAX_QUADS * 4)
#define MAX_IDX   (MAX_QUADS * 6)

typedef struct {
    float x, y, z;
    float r, g, b;
    float bright, pad;
} AcVertex;

static AcVertex *verts = NULL;
static u16 *idx = NULL;
static int quad_count = 0;

static rsxVertexProgram   *vp = (rsxVertexProgram *)solid_vpo;
static rsxFragmentProgram *fp = (rsxFragmentProgram *)solid_fpo;
static void *vp_ucode = NULL;
static void *fp_ucode = NULL;
static u32  *fp_buf = NULL;
static u32   fp_off = 0;
static rsxProgramConst *c_mvp, *c_eye, *c_hor, *c_fog;

/* --- ucagin yonelim donusumu --- */
static void body_to_world(const Flight *f, const float in[3], float out[3])
{
    float cr = cosf(f->roll),  sr = sinf(f->roll);
    float cp = cosf(f->pitch), sp = sinf(f->pitch);
    float cy = cosf(f->yaw),   sy = sinf(f->yaw);
    float x1, y1, z1, x2, y2, z2;

    /* roll (Z ekseni), sonra pitch (X), sonra yaw (Y) */
    x1 = in[0] * cr - in[1] * sr;
    y1 = in[0] * sr + in[1] * cr;
    z1 = in[2];

    y2 = y1 * cp - z1 * sp;
    z2 = y1 * sp + z1 * cp;
    x2 = x1;

    out[0] = f->pos[0] + (x2 * cy + z2 * sy);
    out[1] = f->pos[1] + y2;
    out[2] = f->pos[2] + (-x2 * sy + z2 * cy);
}

/* Yerel koordinatta dortgen ekler (dunyaya donusturulerek) */
static void quad(const Flight *f,
                 const float a[3], const float b[3],
                 const float c[3], const float d[3],
                 float r, float g, float bl, float bright)
{
    AcVertex *v;
    const float *src[4];
    int i;

    if (quad_count >= MAX_QUADS)
        return;

    src[0] = a; src[1] = b; src[2] = c; src[3] = d;
    v = &verts[quad_count * 4];

    for (i = 0; i < 4; i++) {
        float w[3];

        body_to_world(f, src[i], w);
        v[i].x = w[0]; v[i].y = w[1]; v[i].z = w[2];
        v[i].r = r; v[i].g = g; v[i].b = bl;
        v[i].bright = bright;
        v[i].pad = 0.0f;
    }

    quad_count++;
}

/* Mentese ekseni etrafinda dondurulmus nokta.
 * axis: 0 = X ekseni (flap, elevator), 1 = Y ekseni (rudder) */
static void hinge_point(const float p[3], const float pivot[3],
                        float angle, int axis, float out[3])
{
    float dx = p[0] - pivot[0];
    float dy = p[1] - pivot[1];
    float dz = p[2] - pivot[2];
    float ca = cosf(angle), sa = sinf(angle);

    if (axis == 0) {            /* X ekseni: yukari-asagi kirilma */
        out[0] = pivot[0] + dx;
        out[1] = pivot[1] + dy * ca - dz * sa;
        out[2] = pivot[2] + dy * sa + dz * ca;
    } else {                    /* Y ekseni: saga-sola kirilma */
        out[0] = pivot[0] + dx * ca - dz * sa;
        out[1] = pivot[1] + dy;
        out[2] = pivot[2] + dx * sa + dz * ca;
    }
}

/* Dortgeni verilen mentese ile dondurup ekler */
static void hinged_quad(const Flight *f,
                        const float a[3], const float b[3],
                        const float c[3], const float d[3],
                        const float pivot[3], float angle, int axis,
                        float r, float g, float bl, float bright)
{
    float ra[3], rb[3], rc[3], rd[3];

    hinge_point(a, pivot, angle, axis, ra);
    hinge_point(b, pivot, angle, axis, rb);
    hinge_point(c, pivot, angle, axis, rc);
    hinge_point(d, pivot, angle, axis, rd);

    quad(f, ra, rb, rc, rd, r, g, bl, bright);
}

#define V(name, X, Y, Z) float name[3] = { X, Y, Z }

static void build_aircraft(const Flight *f)
{
    /* olculer (birim) */
    const float FL = 9.0f;      /* govde yarim uzunluk */
    const float FW = 1.1f;      /* govde yarim genislik */
    const float WS = 8.5f;      /* kanat acikligi (tek taraf) */

    float flap_ang = f->flap * FLAP_MAX_RAD;
    float spoil_ang = -f->spoiler * SPOILER_MAX_RAD;
    float ail_ang = f->in_roll * AILERON_MAX_RAD;
    float elev_ang = -f->in_pitch * ELEVATOR_MAX_RAD;
    float rud_ang = f->in_yaw * RUDDER_MAX_RAD;
    int side;

    quad_count = 0;

    /* --- govde: ust, alt ve iki yan yuzey --- */
    {
        V(a, -FW,  0.9f, -FL); V(b,  FW,  0.9f, -FL);
        V(c, -FW,  1.0f,  FL); V(d,  FW,  1.0f,  FL);
        quad(f, a, b, c, d, 0.88f, 0.90f, 0.93f, 1.00f);      /* ust */
    }
    {
        V(a, -FW, -0.9f, -FL); V(b,  FW, -0.9f, -FL);
        V(c, -FW, -1.0f,  FL); V(d,  FW, -1.0f,  FL);
        quad(f, a, b, c, d, 0.55f, 0.57f, 0.60f, 1.00f);      /* alt */
    }
    for (side = 0; side < 2; side++) {
        float s = side ? 1.0f : -1.0f;
        V(a, s * FW,  0.9f, -FL); V(b, s * FW, -0.9f, -FL);
        V(c, s * FW,  1.0f,  FL); V(d, s * FW, -1.0f,  FL);
        quad(f, a, b, c, d, 0.76f, 0.78f, 0.82f, side ? 1.05f : 0.85f);
    }
    /* burun kapagi */
    {
        V(a, -FW, 0.9f, -FL); V(b, FW, 0.9f, -FL);
        V(c, -0.35f, 0.1f, -FL - 2.2f); V(d, 0.35f, 0.1f, -FL - 2.2f);
        quad(f, a, b, c, d, 0.30f, 0.34f, 0.40f, 1.0f);       /* kokpit cami */
    }

    /* --- kanatlar + hareketli yuzeyler --- */
    for (side = 0; side < 2; side++) {
        float s = side ? 1.0f : -1.0f;
        float bright = side ? 1.02f : 0.88f;

        /* ana kanat */
        {
            V(a, s * 1.0f, 0.1f, -1.2f); V(b, s * WS, 0.1f, 1.6f);
            V(c, s * 1.0f, 0.1f,  2.2f); V(d, s * WS, 0.1f, 3.4f);
            quad(f, a, b, c, d, 0.84f, 0.86f, 0.90f, bright);
        }

        /* flap: kanadin arka ic kismi, menteseden asagi kirilir */
        {
            V(a, s * 1.2f, 0.08f, 2.2f); V(b, s * 4.2f, 0.08f, 2.7f);
            V(c, s * 1.2f, 0.08f, 3.5f); V(d, s * 4.2f, 0.08f, 4.0f);
            V(pivot, 0.0f, 0.08f, 2.2f);
            hinged_quad(f, a, b, c, d, pivot, flap_ang, 0,
                        0.72f, 0.74f, 0.78f, bright);
        }

        /* aileron: arka dis kisim, iki tarafta ters yonde */
        {
            V(a, s * 5.0f, 0.08f, 2.9f); V(b, s * WS, 0.08f, 3.4f);
            V(c, s * 5.0f, 0.08f, 3.8f); V(d, s * WS, 0.08f, 4.2f);
            V(pivot, 0.0f, 0.08f, 2.9f);
            hinged_quad(f, a, b, c, d, pivot, ail_ang * s, 0,
                        0.70f, 0.72f, 0.76f, bright);
        }

        /* spoiler: kanadin ustunde, yukari kalkar */
        {
            V(a, s * 2.0f, 0.14f, 1.2f); V(b, s * 5.0f, 0.14f, 1.8f);
            V(c, s * 2.0f, 0.14f, 2.0f); V(d, s * 5.0f, 0.14f, 2.6f);
            V(pivot, 0.0f, 0.14f, 1.2f);
            hinged_quad(f, a, b, c, d, pivot, spoil_ang, 0,
                        0.62f, 0.64f, 0.68f, bright + 0.1f);
        }

        /* motor: kanadin altinda kisa bir kutu */
        {
            V(a, s * 3.0f, -0.2f, 0.2f); V(b, s * 4.4f, -0.2f, 0.4f);
            V(c, s * 3.0f, -0.2f, 2.0f); V(d, s * 4.4f, -0.2f, 2.2f);
            quad(f, a, b, c, d, 0.42f, 0.44f, 0.48f, bright);
        }
        {
            V(a, s * 3.0f, -1.0f, 0.2f); V(b, s * 4.4f, -1.0f, 0.4f);
            V(c, s * 3.0f, -1.0f, 2.0f); V(d, s * 4.4f, -1.0f, 2.2f);
            quad(f, a, b, c, d, 0.34f, 0.36f, 0.40f, bright * 0.9f);
        }
    }

    /* --- yatay stabilizator + elevator --- */
    for (side = 0; side < 2; side++) {
        float s = side ? 1.0f : -1.0f;
        float bright = side ? 1.02f : 0.88f;

        {
            V(a, s * 0.6f, 0.5f, FL - 2.0f); V(b, s * 3.4f, 0.5f, FL - 1.2f);
            V(c, s * 0.6f, 0.5f, FL - 0.4f); V(d, s * 3.4f, 0.5f, FL - 0.2f);
            quad(f, a, b, c, d, 0.84f, 0.86f, 0.90f, bright);
        }
        {
            V(a, s * 0.6f, 0.48f, FL - 0.4f); V(b, s * 3.4f, 0.48f, FL - 0.2f);
            V(c, s * 0.6f, 0.48f, FL + 0.6f); V(d, s * 3.4f, 0.48f, FL + 0.7f);
            V(pivot, 0.0f, 0.48f, FL - 0.4f);
            hinged_quad(f, a, b, c, d, pivot, elev_ang, 0,
                        0.70f, 0.72f, 0.76f, bright);
        }
    }

    /* --- dikey stabilizator + rudder --- */
    {
        V(a, 0.0f, 0.9f, FL - 2.2f); V(b, 0.0f, 4.2f, FL - 0.6f);
        V(c, 0.0f, 0.9f, FL - 0.2f); V(d, 0.0f, 4.2f, FL + 0.2f);
        quad(f, a, b, c, d, 0.30f, 0.42f, 0.68f, 1.0f);
    }
    {
        V(a, 0.0f, 0.9f, FL - 0.2f); V(b, 0.0f, 4.2f, FL + 0.2f);
        V(c, 0.0f, 0.9f, FL + 0.9f); V(d, 0.0f, 4.2f, FL + 1.0f);
        V(pivot, 0.0f, 0.9f, FL - 0.2f);
        hinged_quad(f, a, b, c, d, pivot, rud_ang, 1,
                    0.26f, 0.36f, 0.60f, 1.0f);
    }
}

void aircraft_cockpit_pos(const Flight *f, float out[3])
{
    V(local, 0.0f, 0.8f, -8.0f);
    body_to_world(f, local, out);
}

void aircraft_chase_pos(const Flight *f, float out[3])
{
    V(local, 0.0f, 3.4f, 17.0f);
    body_to_world(f, local, out);
}

int aircraft_init(void)
{
    u32 sz = 0;
    int i;

    verts = (AcVertex *)rsxMemalign(128, MAX_VERTS * sizeof(AcVertex));
    idx   = (u16 *)rsxMemalign(128, MAX_IDX * sizeof(u16));
    if (verts == NULL || idx == NULL)
        return -1;

    for (i = 0; i < MAX_QUADS; i++) {
        u16 v = (u16)(i * 4);

        idx[i * 6 + 0] = v;
        idx[i * 6 + 1] = (u16)(v + 1);
        idx[i * 6 + 2] = (u16)(v + 2);
        idx[i * 6 + 3] = (u16)(v + 2);
        idx[i * 6 + 4] = (u16)(v + 1);
        idx[i * 6 + 5] = (u16)(v + 3);
    }

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

void aircraft_draw(const Flight *f, const Camera *cam, const Mat4 *proj,
                   const Atmosphere *atm)
{
    gcmContextData *ctx = rsx3d_context();
    Mat4 view = camera_view_matrix(cam);
    Mat4 mvp = mat4_mul(proj, &view);
    float eye[4], hor[4];
    float fog = atm->fog_distance;
    u32 off;
    int k;

    build_aircraft(f);
    if (quad_count <= 0)
        return;

    for (k = 0; k < 3; k++) {
        eye[k] = cam->pos[k];
        hor[k] = atm->horizon[k];
    }
    eye[3] = hor[3] = 0.0f;

    rsxAddressToOffset(&verts[0].x, &off);
    rsxBindVertexArrayAttrib(ctx, GCM_VERTEX_ATTRIB_POS, 0, off,
                             sizeof(AcVertex), 3, GCM_VERTEX_DATA_TYPE_F32,
                             GCM_LOCATION_RSX);

    rsxAddressToOffset(&verts[0].r, &off);
    rsxBindVertexArrayAttrib(ctx, GCM_VERTEX_ATTRIB_NORMAL, 0, off,
                             sizeof(AcVertex), 3, GCM_VERTEX_DATA_TYPE_F32,
                             GCM_LOCATION_RSX);

    rsxAddressToOffset(&verts[0].bright, &off);
    rsxBindVertexArrayAttrib(ctx, GCM_VERTEX_ATTRIB_TEX0, 0, off,
                             sizeof(AcVertex), 2, GCM_VERTEX_DATA_TYPE_F32,
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
