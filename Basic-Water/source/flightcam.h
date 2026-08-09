#ifndef FLIGHTCAM_H
#define FLIGHTCAM_H

#include "camera.h"
#include "flight.h"

/* Kamera modlari. Mevcut Camera yapisi korunur; ucus modlarinda konum ve
 * acilar ucaktan turetilir, boylece sahne cizimi hic degismez.
 *
 * Kokpit disindaki dis kameralar ucaga BAKAR (look-at). Onceki surumde
 * kamera yalnizca ucakla ayni yone donuyordu; donuslerde aci geride kaldigi
 * icin ucak ekranin disina kayiyordu. */

typedef enum {
    CAM_CHASE = 0,      /* arka ust, TPS */
    CAM_COCKPIT,        /* pilot gozunden */
    CAM_LEFT_WING,      /* sol kanat ucundan */
    CAM_RIGHT_WING,     /* sag kanat ucundan */
    CAM_TAIL,           /* kuyrugun arkasindan, uzak */
    CAM_FREE,           /* serbest gezinme */
    CAM_MODE_COUNT
} CamMode;

const char *flightcam_name(CamMode m);

/* Yorunge kamerasi: ucagin etrafinda donmek icin.
 *
 * Takip ve kuyruk modlarinda sag analog kamerayi ucagin cevresinde dondurur,
 * boylece ucagin buyuklugu ve bicimi gorulebilir. Acilar kalicidir - birakinca
 * geri sifirlanmaz; SELECT ile menuye girip cikmak da bozmaz. */
void flightcam_orbit(float dyaw, float dpitch, float dzoom, float dt);

/* Yorunge acilarini varsayilana dondurur (kamera modu degisiminde) */
void flightcam_orbit_reset(void);

/* HUD'da gostermek icin: yorunge acilari varsayilandan sapmis mi */
int  flightcam_orbit_active(void);

/* Ucus moduna gore kamerayi gunceller. dt yumusak takip icin gerekli. */
void flightcam_update(Camera *cam, CamMode mode, const Flight *f, float dt);

/* Bir noktadan hedefe bakan yaw/pitch acilarini hesaplar.
 * Donanim bagimsiz - birim testli. */
void flightcam_look_at(const float eye[3], const float target[3],
                       float *yaw, float *pitch);

#endif
