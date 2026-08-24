#ifndef OBJECTIVES_H
#define OBJECTIVES_H

#include "camera.h"

/* Gorev listesi.
 *
 * Durum mantigi donanimdan bagimsizdir (birim testli): her gorevin bir
 * kontrol kurali vardir ve kamera durumuna gore tamamlanir. Pistler
 * eklendiginde kurallar gercek konum hedeflerine baglanacak. */

#define OBJ_COUNT 4

typedef struct {
    int done[OBJ_COUNT];
    int completed_count;
} Objectives;

void objectives_init(Objectives *o);

/* Kamera durumuna gore gorevleri gunceller */
void objectives_update(Objectives *o, const Camera *cam, float speed_kmh);

void objectives_draw(const Objectives *o);

const char *objective_text(int index);

#endif
