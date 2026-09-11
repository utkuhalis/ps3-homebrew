#include <string.h>
#include <math.h>

#include <ppu-lv2.h>
#include <rsx/gcm_sys.h>
#include <rsx/rsx.h>

#include "aircraft.h"
#include "rsx3d.h"
#include "mesh.h"
#include "profiler.h"
#include "ps3log.h"

#include "aircraft_vpo.h"
#include "aircraft_fpo.h"
#include "plane_bin.h"

/* Aircraft local coordinate system:
 *   +X right wing, +Y up, -Z towards the nose (same rule as the camera)
 *
 * The body is not generated in code; the model prepared by
 * tools/glb_to_mesh.py comes embedded in the EBOOT. Every frame the CPU
 * transforms the vertices to the aircraft's position and attitude - no
 * shader changes needed, and the cost is measurable but small.
 *
 * Parts: the 737 arrives as 41 named parts (body, flaps, spoilers,
 * ailerons, rudder, gear, wheels, fans, doors). Gear parts are not drawn
 * while the gear is up. */

#define BODY_PART_NAME "body"

/* Moving control surfaces.
 *
 * The 737 model has every surface as its own part. Each surface rotates
 * about its own hinge: flaps and ailerons about the span (X) axis, the
 * rudder about the vertical (Y) axis. The hinge is the part's LEADING
 * edge; with the nose at -Z in aircraft coordinates, that is the part's
 * smallest Z. */

typedef enum {
    SURF_NONE = 0,
    SURF_FLAP_L, SURF_FLAP_R,
    SURF_AIL_L,  SURF_AIL_R,
    SURF_RUDDER,
    SURF_ELEV_L, SURF_ELEV_R,
    SURF_SPOIL_L, SURF_SPOIL_R,
    SURF_WHEEL,         /* wheel hub: spins with ground speed */
    SURF_FIXED          /* fixed to the body, but drawn as a separate part */
} SurfaceKind;

typedef struct {
    SurfaceKind kind;
    float hinge[3];         /* hinge point, in aircraft local space */
} PartInfo;

static PartInfo part_info[MESH_MAX_PARTS];

/* Part types are recognised by PREFIX: in real aircraft models one surface
 * is split into several panels (flap_left01..04, spoiler_right01..06) and
 * all of them move with the same control input. */
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

/* Current deflection of the surface (radians). Signs: flaps positive down,
 * ailerons opposite each other, rudder in the pedal direction. */
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
        return -f->spoiler * SPOILER_MAX_RAD;   /* deflects upward */
    case SURF_WHEEL:
        return f->wheel_spin;
    default:
        return 0.0f;
    }
}

/* Lighting is done on the GPU, in the vertex program, so only raw data
 * is carried here: position, world-space normal, colour and metallic. */
/* Field order matches the attribute bindings in aircraft_draw: position
 * (POS), normal (NORMAL), colour and metallic (TEXCOORD0). */
typedef struct {
    float x, y, z;
    float nx, ny, nz;
    float r, g, b, metal;
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

/* Beyond this distance the small parts are skipped (squared units) */
#define LOD_SMALL_PARTS_DIST2  (320.0f * 320.0f)
static int        loaded = 0;

static rsxVertexProgram   *vp = (rsxVertexProgram *)aircraft_vpo;
static rsxFragmentProgram *fp = (rsxFragmentProgram *)aircraft_fpo;
static void *vp_ucode = NULL;
static void *fp_ucode = NULL;
static u32  *fp_buf = NULL;
static u32   fp_off = 0;
static rsxProgramConst *c_mvp, *c_eye, *c_hor, *c_fog;
/* Lighting is solved in the vertex program, so the sun and sky values go
 * to the vertex program. */
static rsxProgramConst *c_v_eye, *c_v_sun, *c_v_scol, *c_v_hor, *c_v_zen;
static rsxProgramConst *c_sun, *c_suncol, *c_zen;

#define V(name, a, b, c) float name[3]; name[0] = (a); name[1] = (b); name[2] = (c)

/* The body -> world transform lives in the pure flight module (so the
 * host tests can link it); this is only a shortcut. */
static void aircraft_body_to_world(const Flight *f, const float in[3],
                                   float out[3])
{
    flight_body_to_world(f, in, out);
}

void aircraft_cockpit_pos(const Flight *f, float out[3])
{
    V(local, 0.0f, 1.6f, -16.0f);   /* the 737 cockpit is close to the nose */
    aircraft_body_to_world(f, local, out);
}

void aircraft_chase_pos(const Flight *f, float out[3])
{
    V(local, 0.0f, 8.0f, 48.0f);
    aircraft_body_to_world(f, local, out);
}

/* The attitude matrix lives in the pure flight module; this only applies it. */
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

    /* Hinge point for each part: the middle of its leading edge */
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
                float c = mp->verts[v * MESH_VERTEX_FLOATS + k];

                if (c < mn[k]) mn[k] = c;
                if (c > mx[k]) mx[k] = c;
            }
        }

        /* Bounding radius that keeps the camera out of the aircraft: the body's
         * farthest point. Computed from the model, because a fixed value is wrong
         * as soon as the aircraft changes (13 m jet, 39 m 737). */
        if (i == body_part) {
            float rx = (mx[0] - mn[0]) * 0.5f;
            float ry = (mx[1] - mn[1]) * 0.5f;
            float rz = (mx[2] - mn[2]) * 0.5f;
            float r2 = rx * rx + ry * ry + rz * rz;

            bound_radius = sqrtf(r2);
        }

        part_info[i].hinge[0] = (mn[0] + mx[0]) * 0.5f;
        part_info[i].hinge[1] = (mn[1] + mx[1]) * 0.5f;

        /* Control surfaces rotate about their LEADING EDGE; a wheel rotates about
         * its own CENTRE. */
        part_info[i].hinge[2] = (part_info[i].kind == SURF_WHEEL)
                                ? (mn[2] + mx[2]) * 0.5f
                                : mn[2];

        /* The wheel spins about the X axis: its hinge is its centre. */
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

    /* Indices are fixed and copied once. Every part is bound relative to the
     * start of its own vertex array, so index values stay part-local and never
     * pass the u16 limit. */
    for (i = 0; i < model.part_count; i++)
        memcpy(&idx[part_idx_base[i]], model.part[i].idx,
               model.part[i].index_count * sizeof(u16));

    rsxVertexProgramGetUCode(vp, &vp_ucode, &sz);
    c_mvp    = rsxVertexProgramGetConst(vp, "mvpMatrix");
    c_v_eye  = rsxVertexProgramGetConst(vp, "eyePosition");
    c_v_sun  = rsxVertexProgramGetConst(vp, "sunDir");
    c_v_scol = rsxVertexProgramGetConst(vp, "sunColor");
    c_v_hor  = rsxVertexProgramGetConst(vp, "horizonCol");
    c_v_zen  = rsxVertexProgramGetConst(vp, "zenithCol");

    rsxFragmentProgramGetUCode(fp, &fp_ucode, &sz);
    fp_buf = (u32 *)rsxMemalign(64, sz);
    if (fp_buf == NULL)
        return -3;
    memcpy(fp_buf, fp_ucode, sz);
    if (rsxAddressToOffset(fp_buf, &fp_off) != 0)
        return -4;

    c_eye    = rsxFragmentProgramGetConst(fp, "eyePosition");
    c_hor    = rsxFragmentProgramGetConst(fp, "horizonCol");
    c_fog    = rsxFragmentProgramGetConst(fp, "fogDistance");
    c_sun    = rsxFragmentProgramGetConst(fp, "sunDir");
    c_suncol = rsxFragmentProgramGetConst(fp, "sunColor");
    c_zen    = rsxFragmentProgramGetConst(fp, "zenithCol");

    /* A missing uniform leaves its term at zero and darkens the body; every
     * lookup is guarded with an if at draw time. */
    loaded = 1;
    return 0;
}

/* Moves the model to the aircraft's position and attitude.
 *
 * Lighting is not computed here any more - the vertex program does it.
 * That also cut the per-frame CPU work: no per-vertex half vectors or
 * powers. */
static void build_model(const Flight *f)
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
            const float *src = &mp->verts[v * MESH_VERTEX_FLOATS];
            float local[MESH_VERTEX_FLOATS];
            float wp[3], wn[3];
            int k;

            for (k = 0; k < MESH_VERTEX_FLOATS; k++)
                local[k] = src[k];

            /* First the hinge rotation (the part about its own axis), then the body */
            if (kind != SURF_NONE && kind != SURF_FIXED
                && (ang > 0.0001f || ang < -0.0001f)) {
                float dy = local[1] - part_info[p].hinge[1];
                float dz = local[2] - part_info[p].hinge[2];
                float dx = local[0] - part_info[p].hinge[0];

                if (kind == SURF_RUDDER) {
                    local[0] = part_info[p].hinge[0] + dx * ca - dz * sa;
                    local[2] = part_info[p].hinge[2] + dx * sa + dz * ca;
                    {
                        float nx = local[3], nz = local[5];

                        local[3] = nx * ca - nz * sa;
                        local[5] = nx * sa + nz * ca;
                    }
                } else {
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

            dst[v].x = wp[0];
            dst[v].y = wp[1];
            dst[v].z = wp[2];
            dst[v].nx = wn[0];
            dst[v].ny = wn[1];
            dst[v].nz = wn[2];
            dst[v].r = src[6];
            dst[v].g = src[7];
            dst[v].b = src[8];
            dst[v].metal = src[9];
        }
    }
}

void aircraft_draw(const Flight *f, const Camera *cam, const Mat4 *proj,
                   const Atmosphere *atm)
{
    gcmContextData *ctx = rsx3d_context();
    Mat4 view = camera_view_matrix(cam);
    Mat4 mvp = mat4_mul(proj, &view);
    float eye[4], hor[4], sun[4], suncol[4], zen[4];
    float fog = atm->fog_distance;
    u32 off;
    int k, p;

    if (!loaded)
        return;

    drawn_tris = 0;
    prof_begin(PROF_MODEL);
    build_model(f);
    prof_end(PROF_MODEL);

    /* Level of detail: from a distance small parts (wheels, gear, fans) are a
     * few pixels at most. Skipping them saves triangles and changes nothing
     * visible. */
    {
        float dx = cam->pos[0] - f->pos[0];
        float dy = cam->pos[1] - f->pos[1];
        float dz = cam->pos[2] - f->pos[2];

        cam_dist2 = dx * dx + dy * dy + dz * dz;
    }

    for (k = 0; k < 3; k++) {
        eye[k]    = cam->pos[k];
        hor[k]    = atm->horizon[k];
        sun[k]    = atm->sun_dir[k];
        suncol[k] = atm->sun_color[k];
        zen[k]    = atm->zenith[k];
    }
    eye[3] = hor[3] = sun[3] = suncol[3] = zen[3] = 0.0f;

    rsxLoadVertexProgram(ctx, vp, vp_ucode);
    if (c_mvp)    rsxSetVertexProgramParameter(ctx, vp, c_mvp, mvp.m);
    if (c_v_eye)  rsxSetVertexProgramParameter(ctx, vp, c_v_eye, eye);
    if (c_v_sun)  rsxSetVertexProgramParameter(ctx, vp, c_v_sun, sun);
    if (c_v_scol) rsxSetVertexProgramParameter(ctx, vp, c_v_scol, suncol);
    if (c_v_hor)  rsxSetVertexProgramParameter(ctx, vp, c_v_hor, hor);
    if (c_v_zen)  rsxSetVertexProgramParameter(ctx, vp, c_v_zen, zen);

    if (c_eye)    rsxSetFragmentProgramParameter(ctx, fp, c_eye, eye, fp_off, GCM_LOCATION_RSX);
    if (c_hor)    rsxSetFragmentProgramParameter(ctx, fp, c_hor, hor, fp_off, GCM_LOCATION_RSX);
    if (c_fog)    rsxSetFragmentProgramParameter(ctx, fp, c_fog, &fog, fp_off, GCM_LOCATION_RSX);
    if (c_sun)    rsxSetFragmentProgramParameter(ctx, fp, c_sun, sun, fp_off, GCM_LOCATION_RSX);
    if (c_suncol) rsxSetFragmentProgramParameter(ctx, fp, c_suncol, suncol, fp_off, GCM_LOCATION_RSX);
    if (c_zen)    rsxSetFragmentProgramParameter(ctx, fp, c_zen, zen, fp_off, GCM_LOCATION_RSX);

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

        /* Wheels and struts are not drawn while the gear is up.
         * Parts named "gear"/"wheel" belong to the gear group. */
        if (f->gear_pos < 0.02f
            && (strncmp(model.part[p].name, "gear", 4) == 0
                || strncmp(model.part[p].name, "wheel", 5) == 0
                || strncmp(model.part[p].name, "geardoor", 8) == 0))
            continue;

        /* From a distance ONLY the small parts are skipped. It used to skip
         * everything except the body; now that wings and tail are separate parts
         * too, that would reduce the aircraft to a fuselage. */
        if (cam_dist2 > LOD_SMALL_PARTS_DIST2
            && (part_info[p].kind == SURF_WHEEL
                || strncmp(model.part[p].name, "gear", 4) == 0
                || strncmp(model.part[p].name, "fan", 3) == 0))
            continue;

        rsxAddressToOffset(&base[0].x, &off);
        rsxBindVertexArrayAttrib(ctx, GCM_VERTEX_ATTRIB_POS, 0, off,
                                 sizeof(AcVertex), 3, GCM_VERTEX_DATA_TYPE_F32,
                                 GCM_LOCATION_RSX);

        rsxAddressToOffset(&base[0].nx, &off);
        rsxBindVertexArrayAttrib(ctx, GCM_VERTEX_ATTRIB_NORMAL, 0, off,
                                 sizeof(AcVertex), 3, GCM_VERTEX_DATA_TYPE_F32,
                                 GCM_LOCATION_RSX);

        rsxAddressToOffset(&base[0].r, &off);
        rsxBindVertexArrayAttrib(ctx, GCM_VERTEX_ATTRIB_TEX0, 0, off,
                                 sizeof(AcVertex), 4, GCM_VERTEX_DATA_TYPE_F32,
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
