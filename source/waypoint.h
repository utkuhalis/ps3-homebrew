#ifndef WAYPOINT_H
#define WAYPOINT_H

#include "camera.h"
#include "mat4.h"

/* Ekranda hedef noktalarini gosteren isaretler.
 *
 * Dunya konumu goruntu-projeksiyon matrisiyle ekran koordinatina cevrilir.
 * Hedef ekranin disinda kaliyorsa isaret kenara sabitlenir ve bir ok yonu
 * gosterir; boylece oyuncu nereye donecegini bilir. */

typedef struct {
    float sx, sy;       /* ekran konumu (sanal 1280x720) */
    float distance;     /* kameradan uzaklik */
    int   on_screen;    /* 1: hedef gorus alaninda */
    int   behind;       /* 1: hedef kameranin arkasinda */
    float edge_angle;   /* ekran disindaysa ok yonu (radyan) */
} WaypointView;

/* Dunya noktasini ekran bilgisine cevirir. vp: proj * view */
void waypoint_project(const Mat4 *vp, const Camera *cam, const float world[3],
                      WaypointView *out);

/* Tum pist hedeflerini cizer */
void waypoint_draw_all(const Camera *cam, const Mat4 *proj);

#endif
