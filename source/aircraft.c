#include <string.h>
#include <math.h>

#include <ppu-lv2.h>
#include <rsx/gcm_sys.h>
#include <rsx/rsx.h>

#include "aircraft.h"
#include "rsx3d.h"
#include "mesh.h"
#include "profiler.h"

#include "solid_vpo.h"
#include "solid_fpo.h"
#include "plane_bin.h"

/* Ucak yerel koordinat sistemi:
 *   +X sag kanat, +Y yukari, -Z burun yonu (kamera ile ayni kural)
 *
 * Govde artik kodla uretilmiyor; tools/glb_to_mesh.py ile hazirlanan gercek
 * model EBOOT'a gomulu geliyor. Vertexler her karede CPU'da ucagin konum ve
 * yonelimine gore donusturuluyor - RSX shader'i degistirmeye gerek kalmiyor
 * ve maliyet 22 bin vertex icin olculebilir duzeyde (~2 ms).
 *
 * Parca ayrimi: "Air Plane" govde ve kanatlar, geri kalan bes parca govde
 * altindaki inis takimi grubudur (tekerlekler ve destekleri) - takim
 * kapaliyken cizilmezler. */

#define BODY_PART_NAME "body"

/* Hareketli kumanda yuzeyleri.
 *
 * Parcalar Blender'da (tools/blender/split_surfaces.py) govdeden ayrildi.
 * Her yuzey kendi mentese ekseninde doner: flap ve aileron kanat acikligi
 * (X) ekseninde, rudder dikey (Y) eksende. Mentese noktasi parcanin ON
 * kenaridir; ucak koordinatinda burun -Z oldugu icin bu, parcanin en kucuk
 * Z degeridir. */

typedef enum {
    SURF_NONE = 0,
    SURF_FLAP_L, SURF_FLAP_R,
    SURF_AIL_L,  SURF_AIL_R,
    SURF_RUDDER,
    SURF_ELEV_L, SURF_ELEV_R,
    SURF_SPOIL_L, SURF_SPOIL_R,
    SURF_WHEEL,         /* tekerlek gobegi: hizla orantili doner */
    SURF_FIXED          /* govdeyle birlikte duran, ama ayri cizilen parca */
} SurfaceKind;

typedef struct {
    SurfaceKind kind;
    float hinge[3];         /* mentese noktasi, ucak yerel uzayinda */
} PartInfo;

static PartInfo part_info[MESH_MAX_PARTS];

/* Parca turu ONEK ile taninir: gercek ucak modellerinde ayni yuzey birden
 * cok panele bolunmus oluyor (flap_left01..04, spoiler_right01..06) ve hepsi
 * ayni kumanda girdisiyle hareket eder. */
static SurfaceKind kind_of(const char *name)
{
    if (strncmp(name, "flap_left", 9) == 0)      return SURF_FLAP_L;
    if (strncmp(name, "flap_right", 10) == 0)    return SURF_FLAP_R;
    if (strncmp(name, "aileron_left", 12) == 0)  return SURF_AIL_L;
    if (strncmp(name, "aileron_right", 13) == 0) return SURF_AIL_R;
    if (strncmp(name, "rudder", 6) == 0)         return SURF_RUDDER;
    if (strncmp(name, "elevator_left", 13) == 0)  return SURF_ELEV_L;
    if (strncmp(name, "elevator_right", 14) == 0) return SURF_ELEV_R;
    if (strncmp(name, "spoiler_left", 12) == 0)   return SURF_SPOIL_L;
    if (strncmp(name, "spoiler_right", 13) == 0)  return SURF_SPOIL_R;
    if (strncmp(name, "wheel_", 6) == 0)         return SURF_WHEEL;
    return SURF_FIXED;
}

/* Yuzeyin o anki sapma acisi (radyan). Isaretler: flap asagi pozitif,
 * aileron'lar birbirinin tersi, rudder pedal yonunde. */
static float surface_angle(SurfaceKind k, const Flight *f)
{
    switch (k) {
    case SURF_FLAP_L:
    case SURF_FLAP_R:
        return f->flap * FLAP_MAX_RAD;
    case SURF_AIL_L:
        return -f->in_roll * AILERON_MAX_RAD;
    case SURF_AIL_R:
        return  f->in_roll * AILERON_MAX_RAD;
    case SURF_RUDDER:
        return f->in_yaw * RUDDER_MAX_RAD;
    case SURF_ELEV_L:
    case SURF_ELEV_R:
        return -f->in_pitch * ELEVATOR_MAX_RAD;
    case SURF_SPOIL_L:
    case SURF_SPOIL_R:
        return -f->spoiler * SPOILER_MAX_RAD;   /* yukari kalkar */
    case SURF_WHEEL:
        return f->wheel_spin;
    default:
        return 0.0f;
    }
}

typedef struct {
    float x, y, z;
    float r, g, b;
    float bright, pad;
} AcVertex;

static Mesh       model;
static AcVertex  *verts = NULL;
static u16       *idx = NULL;
static u32        part_vtx_base[MESH_MAX_PARTS];
static u32        part_idx_base[MESH_MAX_PARTS];
static int        body_part = -1;
static float      cam_dist2 = 0.0f;
static unsigned int drawn_tris = 0;
static float bound_radius = 12.0f;

/* Bu mesafenin otesinde yalnizca govde cizilir (birim kare) */
#define LOD_SMALL_PARTS_DIST2  (320.0f * 320.0f)
static int        loaded = 0;

static rsxVertexProgram   *vp = (rsxVertexProgram *)solid_vpo;
static rsxFragmentProgram *fp = (rsxFragmentProgram *)solid_fpo;
static void *vp_ucode = NULL;
static void *fp_ucode = NULL;
static u32  *fp_buf = NULL;
static u32   fp_off = 0;
static rsxProgramConst *c_mvp, *c_eye, *c_hor, *c_fog;

#define V(name, a, b, c) float name[3]; name[0] = (a); name[1] = (b); name[2] = (c)

/* Govde -> dunya donusumu saf flight modulunde (host testine linklenebilsin
 * diye); burada yalnizca kisa bir kisayol var. */
static void aircraft_body_to_world(const Flight *f, const float in[3],
                                   float out[3])
{
    flight_body_to_world(f, in, out);
}

void aircraft_cockpit_pos(const Flight *f, float out[3])
{
    V(local, 0.0f, 1.6f, -16.0f);   /* 737 kokpiti burna yakin */
    aircraft_body_to_world(f, local, out);
}

void aircraft_chase_pos(const Flight *f, float out[3])
{
    V(local, 0.0f, 8.0f, 48.0f);
    aircraft_body_to_world(f, local, out);
}

/* Yonelim matrisi saf flight modulunde; burada yalnizca uygulama var. */
static void apply_mat(const float m[3][3], const float in[3], float out[3])
{
    out[0] = m[0][0] * in[0] + m[0][1] * in[1] + m[0][2] * in[2];
    out[1] = m[1][0] * in[0] + m[1][1] * in[1] + m[1][2] * in[2];
    out[2] = m[2][0] * in[0] + m[2][1] * in[1] + m[2][2] * in[2];
}

int aircraft_init(void)
{
    u32 sz = 0;
    unsigned int total_v = 0, total_i = 0;
    int i;

    if (mesh_load(&model, plane_bin, plane_bin_size) != 0)
        return -1;

    body_part = mesh_find(&model, BODY_PART_NAME);

    /* Her parca icin mentese noktasi: on kenarin ortasi */
    for (i = 0; i < model.part_count; i++) {
        const MeshPart *mp = &model.part[i];
        float mn[3], mx[3];
        unsigned int v;
        int k;

        part_info[i].kind = kind_of(mp->name);

        for (k = 0; k < 3; k++) {
            mn[k] =  1e30f;
            mx[k] = -1e30f;
        }
        for (v = 0; v < mp->vertex_count; v++) {
            for (k = 0; k < 3; k++) {
                float c = mp->verts[v * 9 + k];

                if (c < mn[k]) mn[k] = c;
                if (c > mx[k]) mx[k] = c;
            }
        }

        /* Kameranin ucaga girmemesi icin gereken sinir yaricapi: govdenin
         * en uzak noktasi. Modelden hesaplanir, cunku ucak degistiginde
         * (13 m'lik jet, 39 m'lik 737) sabit bir deger yanlis olur. */
        if (i == body_part) {
            float rx = (mx[0] - mn[0]) * 0.5f;
            float ry = (mx[1] - mn[1]) * 0.5f;
            float rz = (mx[2] - mn[2]) * 0.5f;
            float r2 = rx * rx + ry * ry + rz * rz;

            bound_radius = sqrtf(r2);
        }

        part_info[i].hinge[0] = (mn[0] + mx[0]) * 0.5f;
        part_info[i].hinge[1] = (mn[1] + mx[1]) * 0.5f;

        /* Kumanda yuzeyleri ON KENARINDAN doner; tekerlek ise kendi
         * MERKEZI etrafinda. */
        part_info[i].hinge[2] = (part_info[i].kind == SURF_WHEEL)
                                ? (mn[2] + mx[2]) * 0.5f
                                : mn[2];

        /* Tekerlek X ekseninde doner: mentese noktasi govde merkezidir. */
    }

    for (i = 0; i < model.part_count; i++) {
        part_vtx_base[i] = total_v;
        part_idx_base[i] = total_i;
        total_v += model.part[i].vertex_count;
        total_i += model.part[i].index_count;
    }

    verts = (AcVertex *)rsxMemalign(128, total_v * sizeof(AcVertex));
    idx   = (u16 *)rsxMemalign(128, total_i * sizeof(u16));
    if (verts == NULL || idx == NULL)
        return -2;

    /* Indexler sabit: bir kez kopyalanir. Her parca kendi vertex dizisinin
     * basina gore bind edildigi icin index degerleri parca-yerel kalir ve
     * u16 sinirini asmaz. */
    for (i = 0; i < model.part_count; i++)
        memcpy(&idx[part_idx_base[i]], model.part[i].idx,
               model.part[i].index_count * sizeof(u16));

    rsxVertexProgramGetUCode(vp, &vp_ucode, &sz);
    c_mvp = rsxVertexProgramGetConst(vp, "mvpMatrix");

    rsxFragmentProgramGetUCode(fp, &fp_ucode, &sz);
    fp_buf = (u32 *)rsxMemalign(64, sz);
    if (fp_buf == NULL)
        return -3;
    memcpy(fp_buf, fp_ucode, sz);
    if (rsxAddressToOffset(fp_buf, &fp_off) != 0)
        return -4;

    c_eye = rsxFragmentProgramGetConst(fp, "eyePosition");
    c_hor = rsxFragmentProgramGetConst(fp, "horizonCol");
    c_fog = rsxFragmentProgramGetConst(fp, "fogDistance");

    loaded = 1;
    return 0;
}

/* Modeli ucagin konum ve yonelimine tasir, isigi vertex basina hesaplar.
 *
 * Aydinlatma tamamen burada yapiliyor: shader renk olarak yalnizca hazir
 * degeri aliyor (vertex formatinda normal tasinmiyor). Difuz terime ek olarak
 * Blinn-Phong yansima hesaplaniyor - govdenin metalik parlamasi bundan
 * geliyor; onceden yalnizca duz Lambert vardi ve ucak mat plastik gibiydi. */
static void build_model(const Flight *f, const Atmosphere *atm,
                        const float eye[3])
{
    float rot[3][3];
    int p;

    flight_orientation_matrix(f, rot);

    for (p = 0; p < model.part_count; p++) {
        const MeshPart *mp = &model.part[p];
        AcVertex *dst = &verts[part_vtx_base[p]];
        SurfaceKind kind = part_info[p].kind;
        float ang = surface_angle(kind, f);
        float ca = cosf(ang), sa = sinf(ang);
        unsigned int v;

        for (v = 0; v < mp->vertex_count; v++) {
            const float *src = &mp->verts[v * 9];
            float local[9];     /* konum + normal + renk: src ile ayni duzen */
            float wp[3], wn[3];
            float d;
            int k;

            for (k = 0; k < 9; k++)
                local[k] = src[k];

            /* Once mentese donusu (parca kendi ekseninde), sonra govde */
            if (kind != SURF_NONE && (ang > 0.0001f || ang < -0.0001f)) {
                float dy = local[1] - part_info[p].hinge[1];
                float dz = local[2] - part_info[p].hinge[2];
                float dx = local[0] - part_info[p].hinge[0];

                if (kind == SURF_RUDDER) {
                    /* dikey eksen (Y) etrafinda */
                    local[0] = part_info[p].hinge[0] + dx * ca - dz * sa;
                    local[2] = part_info[p].hinge[2] + dx * sa + dz * ca;
                    {
                        float nx = local[3], nz = local[5];

                        local[3] = nx * ca - nz * sa;
                        local[5] = nx * sa + nz * ca;
                    }
                } else {
                    /* kanat acikligi ekseni (X) etrafinda; aci pozitifken
                     * arka kenar asagi iner */
                    local[1] = part_info[p].hinge[1] + dy * ca - dz * sa;
                    local[2] = part_info[p].hinge[2] + dy * sa + dz * ca;
                    {
                        float ny = local[4], nz = local[5];

                        local[4] = ny * ca - nz * sa;
                        local[5] = ny * sa + nz * ca;
                    }
                }
                src = local;
            }

            apply_mat(rot, src, wp);
            wp[0] += f->pos[0];
            wp[1] += f->pos[1];
            wp[2] += f->pos[2];

            apply_mat(rot, src + 3, wn);

            d = wn[0] * atm->sun_dir[0] + wn[1] * atm->sun_dir[1]
                + wn[2] * atm->sun_dir[2];
            if (d < 0.0f)
                d = 0.0f;

            dst[v].x = wp[0];
            dst[v].y = wp[1];
            dst[v].z = wp[2];

            {
                /* yansima: yarim vektor (goz + gunes) ile normal arasi aci */
                float vx = eye[0] - wp[0];
                float vy = eye[1] - wp[1];
                float vz = eye[2] - wp[2];
                float vl = sqrtf(vx * vx + vy * vy + vz * vz);
                float spec = 0.0f;
                float lit = 0.42f + 0.58f * d;

                if (vl > 0.001f) {
                    float hx = atm->sun_dir[0] + vx / vl;
                    float hy = atm->sun_dir[1] + vy / vl;
                    float hz = atm->sun_dir[2] + vz / vl;
                    float hl = sqrtf(hx * hx + hy * hy + hz * hz);

                    if (hl > 0.001f) {
                        float nh = (wn[0] * hx + wn[1] * hy + wn[2] * hz) / hl;

                        if (nh > 0.0f) {
                            float s = nh * nh;    /* ^2 */

                            s = s * s;            /* ^4 */
                            s = s * s;            /* ^8 */
                            s = s * s;            /* ^16 */
                            s = s * s;            /* ^32 */
                            spec = s * 0.55f * d;
                        }
                    }
                }

                dst[v].r = src[6] * lit + spec;
                dst[v].g = src[7] * lit + spec;
                dst[v].b = src[8] * lit + spec;
            }

            dst[v].bright = 1.0f;   /* aydinlatma zaten renge islendi */
            dst[v].pad = 0.0f;
        }
    }
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
    int k, p;

    if (!loaded)
        return;

    drawn_tris = 0;
    prof_begin(PROF_MODEL);
    build_model(f, atm, cam->pos);
    prof_end(PROF_MODEL);

    /* Detay seviyesi: uzaktan bakildiginda kucuk parcalar (tekerlekler,
     * kumanda yuzeyleri) birkac pikseli gecmez. Onlari cizmemek 22 bin
     * ucgen kazandirir ve gorunurde hicbir sey degismez. */
    {
        float dx = cam->pos[0] - f->pos[0];
        float dy = cam->pos[1] - f->pos[1];
        float dz = cam->pos[2] - f->pos[2];

        cam_dist2 = dx * dx + dy * dy + dz * dz;
    }

    for (k = 0; k < 3; k++) {
        eye[k] = cam->pos[k];
        hor[k] = atm->horizon[k];
    }
    eye[3] = hor[3] = 0.0f;

    rsxLoadVertexProgram(ctx, vp, vp_ucode);
    if (c_mvp) rsxSetVertexProgramParameter(ctx, vp, c_mvp, mvp.m);

    if (c_eye) rsxSetFragmentProgramParameter(ctx, fp, c_eye, eye, fp_off, GCM_LOCATION_RSX);
    if (c_hor) rsxSetFragmentProgramParameter(ctx, fp, c_hor, hor, fp_off, GCM_LOCATION_RSX);
    if (c_fog) rsxSetFragmentProgramParameter(ctx, fp, c_fog, &fog, fp_off, GCM_LOCATION_RSX);

    rsxLoadFragmentProgramLocation(ctx, fp, fp_off, GCM_LOCATION_RSX);
    rsx3d_set_culling(1);

    rsxSetUserClipPlaneControl(ctx,
                               GCM_USER_CLIP_PLANE_DISABLE,
                               GCM_USER_CLIP_PLANE_DISABLE,
                               GCM_USER_CLIP_PLANE_DISABLE,
                               GCM_USER_CLIP_PLANE_DISABLE,
                               GCM_USER_CLIP_PLANE_DISABLE,
                               GCM_USER_CLIP_PLANE_DISABLE);

    for (p = 0; p < model.part_count; p++) {
        AcVertex *base = &verts[part_vtx_base[p]];

        /* Inis takimi kapaliyken tekerlek ve bacaklar cizilmez.
         * Parca adi "gear"/"wheel" ile baslayanlar takim grubudur. */
        if (f->gear_pos < 0.02f
            && (strncmp(model.part[p].name, "gear", 4) == 0
                || strncmp(model.part[p].name, "wheel", 5) == 0
                || strncmp(model.part[p].name, "geardoor", 8) == 0))
            continue;

        /* Uzaktan bakildiginda YALNIZCA kucuk parcalar atlanir. Once govde
         * disindaki her sey atlaniyordu; artik kanat ve kuyruk da ayri
         * parca oldugu icin bu ucagi govdeye indirirdi. */
        if (cam_dist2 > LOD_SMALL_PARTS_DIST2
            && (part_info[p].kind == SURF_WHEEL
                || strncmp(model.part[p].name, "gear", 4) == 0
                || strncmp(model.part[p].name, "fan", 3) == 0))
            continue;

        rsxAddressToOffset(&base[0].x, &off);
        rsxBindVertexArrayAttrib(ctx, GCM_VERTEX_ATTRIB_POS, 0, off,
                                 sizeof(AcVertex), 3, GCM_VERTEX_DATA_TYPE_F32,
                                 GCM_LOCATION_RSX);

        rsxAddressToOffset(&base[0].r, &off);
        rsxBindVertexArrayAttrib(ctx, GCM_VERTEX_ATTRIB_NORMAL, 0, off,
                                 sizeof(AcVertex), 3, GCM_VERTEX_DATA_TYPE_F32,
                                 GCM_LOCATION_RSX);

        rsxAddressToOffset(&base[0].bright, &off);
        rsxBindVertexArrayAttrib(ctx, GCM_VERTEX_ATTRIB_TEX0, 0, off,
                                 sizeof(AcVertex), 2, GCM_VERTEX_DATA_TYPE_F32,
                                 GCM_LOCATION_RSX);

        rsxAddressToOffset(&idx[part_idx_base[p]], &off);
        rsxDrawIndexArray(ctx, GCM_TYPE_TRIANGLES, off,
                          model.part[p].index_count,
                          GCM_INDEX_TYPE_16B, GCM_LOCATION_RSX);
        drawn_tris += model.part[p].index_count / 3;
    }

    rsx3d_set_culling(0);
}

unsigned int aircraft_triangle_count(void)
{
    return loaded ? mesh_triangles(&model) : 0;
}

float aircraft_bound_radius(void)
{
    return bound_radius;
}

unsigned int aircraft_drawn_triangles(void)
{
    return drawn_tris;
}
