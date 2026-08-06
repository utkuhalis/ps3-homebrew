#ifndef FLIGHTCAM_H
#define FLIGHTCAM_H

#include "camera.h"
#include "flight.h"

/* Kamera modlari: ucagi disaridan takip, pilot gozunden, ve serbest.
 * Mevcut Camera yapisi korunur; ucus modlarinda konum ve acilar ucaktan
 * turetilir, boylece sahne cizimi hic degismez. */

typedef enum {
    CAM_CHASE = 0,      /* ucagin arkasindan, yumusak takip */
    CAM_COCKPIT,        /* pilot gozunden */
    CAM_FREE,           /* serbest gezinme (sahneyi incelemek icin) */
    CAM_MODE_COUNT
} CamMode;

const char *flightcam_name(CamMode m);

/* Ucus moduna gore kamerayi gunceller. dt yumusak takip icin gerekli. */
void flightcam_update(Camera *cam, CamMode mode, const Flight *f, float dt);

#endif
