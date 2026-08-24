#include <math.h>

#include "flightcam.h"
#include "aircraft.h"

const char *flightcam_name(CamMode m)
{
    switch (m) {
    case CAM_CHASE:      return "Chase";
    case CAM_COCKPIT:    return "Cockpit";
    case CAM_LEFT_WING:  return "Left Wing";
    case CAM_RIGHT_WING: return "Right Wing";
    case CAM_TAIL:       return "Tail";
    case CAM_FREE:       return "Free";
    default:             return "?";
    }
}

/* Kameranin ileri yonu: (cos(pitch)*sin(yaw), sin(pitch), -cos(pitch)*cos(yaw))
 * Ters cevirirsek yaw = atan2(dx, -dz), pitch = asin(dy). */
void flightcam_look_at(const float eye[3], const float target[3],
                       float *yaw, float *pitch)
{
    float dx = target[0] - eye[0];
    float dy = target[1] - eye[1];
    float dz = target[2] - eye[2];
    float len = sqrtf(dx * dx + dy * dy + dz * dz);

    if (len < 1e-4f) {
        *yaw = 0.0f;
        *pitch = 0.0f;
        return;
    }

    dx /= len;
    dy /= len;
    dz /= len;

    *yaw = atan2f(dx, -dz);
    *pitch = asinf(dy < -1.0f ? -1.0f : (dy > 1.0f ? 1.0f : dy));
}

/* Ucak govdesine gore yerel bir noktayi dunya uzayina tasir.
 * Kamera yerlesimlerinde yatis (roll) bilerek uygulanmaz: ucak yattiginda
 * kameranin da yan yatmasi TPS goruntusunu okunmaz hale getiriyor. Yalnizca
 * yaw ve pitch uygulanir. */
static void mount_point(const Flight *f, const float local[3], float out[3])
{
    float cy = cosf(f->yaw),   sy = sinf(f->yaw);
    float cp = cosf(f->pitch), sp = sinf(f->pitch);
    float x = local[0];
    float y = local[1] * cp - local[2] * sp;
    float z = local[1] * sp + local[2] * cp;

    /* Yaw dondurmesi burun yonuyle tutarli olmali: yerel -Z ekseni dunyada
     * (sin yaw, 0, -cos yaw) yonune gitmeli - forward_vec ile ayni. Isaret
     * ters yazilinca kamera bazi yonelimlerde ucagin ONUNE dusuyordu. */
    out[0] = f->pos[0] + (x * cy - z * sy);
    out[1] = f->pos[1] + y;
    out[2] = f->pos[2] + (x * sy + z * cy);
}

/* Kameranin bakacagi nokta: ucagin govde merkezi, biraz ileride.
 * Burnun onune bakmak ucus yonunu gorunur kilar. */
static void aim_point(const Flight *f, float out[3])
{
    float local[3];

    local[0] = 0.0f;
    local[1] = 0.6f;
    local[2] = -3.0f;           /* govde merkezine yakin: yorungede donerken
                                 * ucak kadrajin ortasinda kalir */
    mount_point(f, local, out);
}

/* --- yorunge (orbit) durumu ---
 * Kamera, ucagin cevresinde kuresel koordinatlarla konumlanir. */
/* Kamera ile engel arasinda birakilan pay */
#define CAM_SKIN               3.0f
#define CAM_GROUND_CLEARANCE   1.5f

#define ORBIT_PITCH_MIN  (-0.35f)
#define ORBIT_PITCH_MAX   ( 1.15f)
#define ORBIT_DIST_MIN    18.0f
#define ORBIT_DIST_MAX   160.0f

static float orbit_yaw = 0.0f;      /* 0 = tam arkadan */
static float orbit_pitch = 0.22f;   /* + yukaridan bakar */
static float orbit_dist = 48.0f;

void flightcam_orbit(float dyaw, float dpitch, float dzoom, float dt)
{
    orbit_yaw += dyaw * 1.9f * dt;
    orbit_pitch += dpitch * 1.2f * dt;
    orbit_dist += dzoom * 22.0f * dt;

    while (orbit_yaw >  3.14159265f) orbit_yaw -= 6.28318531f;
    while (orbit_yaw < -3.14159265f) orbit_yaw += 6.28318531f;

    if (orbit_pitch < ORBIT_PITCH_MIN) orbit_pitch = ORBIT_PITCH_MIN;
    if (orbit_pitch > ORBIT_PITCH_MAX) orbit_pitch = ORBIT_PITCH_MAX;
    if (orbit_dist < ORBIT_DIST_MIN) orbit_dist = ORBIT_DIST_MIN;
    if (orbit_dist > ORBIT_DIST_MAX) orbit_dist = ORBIT_DIST_MAX;
}

void flightcam_orbit_reset(void)
{
    orbit_yaw = 0.0f;
    orbit_pitch = 0.22f;
    orbit_dist = 48.0f;
}

int flightcam_orbit_active(void)
{
    float a = orbit_yaw < 0.0f ? -orbit_yaw : orbit_yaw;

    return (a > 0.05f);
}

/* Her mod icin ucak govdesine gore kamera yerlesimi (x sag, y yukari,
 * z geri; burun -Z yonunde). */
static void mount_local(CamMode mode, float out[3])
{
    switch (mode) {
    case CAM_LEFT_WING:
        out[0] = -19.0f; out[1] = 2.0f; out[2] = 2.0f;
        break;
    case CAM_RIGHT_WING:
        out[0] =  19.0f; out[1] = 2.0f; out[2] = 2.0f;
        break;
    case CAM_TAIL:
        out[0] = 0.0f;  out[1] = 12.0f; out[2] = 85.0f;
        break;
    case CAM_CHASE:
    default:
        /* Kuresel koordinat: yorunge acilari ve mesafe */
        {
            float cp = cosf(orbit_pitch);

            out[0] = sinf(orbit_yaw) * cp * orbit_dist;
            out[1] = sinf(orbit_pitch) * orbit_dist + 1.2f;
            out[2] = cosf(orbit_yaw) * cp * orbit_dist;
        }
        break;
    }
}

float flightcam_ground_at(const Flight *f, float wx, float wz)
{
    float dx = wx - f->ground_ref[0];
    float dz = wz - f->ground_ref[2];

    /* Pist dikdortgeninin uzerinde zemin pist yuzeyidir; disinda deniz. */
    if (flight_over_runway(f, dx, dz))
        return DECK_Y;
    return WATER_SAFE_ALT;
}

void flightcam_clamp(Camera *cam, const float plane_pos[3],
                     float plane_radius, float ground_y)
{
    float dx = cam->pos[0] - plane_pos[0];
    float dy = cam->pos[1] - plane_pos[1];
    float dz = cam->pos[2] - plane_pos[2];
    float d2 = dx * dx + dy * dy + dz * dz;
    float rmin = plane_radius + CAM_SKIN;

    /* --- ucagin govdesine girme --- */
    if (d2 < rmin * rmin) {
        float d = sqrtf(d2);

        if (d < 0.001f) {
            /* Tam merkezde: gecerli bir yon yok, arkaya it. */
            cam->pos[0] = plane_pos[0];
            cam->pos[1] = plane_pos[1];
            cam->pos[2] = plane_pos[2] + rmin;
        } else {
            float s = rmin / d;

            cam->pos[0] = plane_pos[0] + dx * s;
            cam->pos[1] = plane_pos[1] + dy * s;
            cam->pos[2] = plane_pos[2] + dz * s;
        }
    }

    /* --- zeminin altina inme --- */
    if (cam->pos[1] < ground_y + CAM_GROUND_CLEARANCE)
        cam->pos[1] = ground_y + CAM_GROUND_CLEARANCE;
}

void flightcam_update(Camera *cam, CamMode mode, const Flight *f, float dt)
{
    float local[3], eye[3], aim[3];
    float k;
    int i;

    if (mode == CAM_FREE)
        return;                 /* serbest modda kamera kendi kontrolunde */

    if (mode == CAM_COCKPIT) {
        aircraft_cockpit_pos(f, eye);
        for (i = 0; i < 3; i++)
            cam->pos[i] = eye[i];

        /* pilot gozunde kamera ucakla birebir ayni yone bakar */
        cam->yaw = f->yaw;
        cam->pitch = f->pitch;
        return;
    }

    mount_local(mode, local);
    mount_point(f, local, eye);
    aim_point(f, aim);

    /* Kanat kameralari govdeye sabittir (titremesin diye lerp yok).
     * Takip ve kuyruk kameralari yumusak yaklasir; manevrada kamera geride
     * savrulur ve hiz hissi olusur. */
    if (mode == CAM_LEFT_WING || mode == CAM_RIGHT_WING) {
        for (i = 0; i < 3; i++)
            cam->pos[i] = eye[i];
    } else {
        k = dt * 6.0f;
        if (k > 1.0f)
            k = 1.0f;
        for (i = 0; i < 3; i++)
            cam->pos[i] += (eye[i] - cam->pos[i]) * k;
    }

    /* Engeller: govde ve zemin.
     *
     * Zemin olarak kameranin ALTINDAKI yuzey yetmiyor: ucak pistteyken
     * kamera pistin yanina savruldugunda oradaki zemin deniz seviyesi
     * oluyor, kamera 9 metre asagi inip pistin ALTINA giriyor ve pist
     * gorusu kapatiyordu. Bu yuzden ucagin altindaki zemin de hesaba
     * katilir; kamera ikisinin YUKSEK olanindan asagi inemez. */
    {
        float g_cam = flightcam_ground_at(f, cam->pos[0], cam->pos[2]);
        float g_plane = flightcam_ground_at(f, f->pos[0], f->pos[2]);
        float ground = (g_cam > g_plane) ? g_cam : g_plane;

        flightcam_clamp(cam, f->pos, aircraft_bound_radius(), ground);
    }

    /* Konum neresi olursa olsun kamera ucaga bakar: ucak her zaman
     * ekranin ortasinda kalir. */
    flightcam_look_at(cam->pos, aim, &cam->yaw, &cam->pitch);
}
