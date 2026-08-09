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

/* Ucus moduna gore kamerayi gunceller. dt yumusak takip icin gerekli. */
void flightcam_update(Camera *cam, CamMode mode, const Flight *f, float dt);

/* Bir noktadan hedefe bakan yaw/pitch acilarini hesaplar.
 * Donanim bagimsiz - birim testli. */
void flightcam_look_at(const float eye[3], const float target[3],
                       float *yaw, float *pitch);

#endif
