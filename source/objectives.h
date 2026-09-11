#ifndef OBJECTIVES_H
#define OBJECTIVES_H

#include "camera.h"

/* Objective list.
 *
 * The state logic is hardware independent (unit tested): every objective
 * has a check rule and completes from the current state. */

#define OBJ_COUNT 4

typedef struct {
    int done[OBJ_COUNT];
    int completed_count;
} Objectives;

void objectives_init(Objectives *o);

/* Updates the objectives from the current state */
void objectives_update(Objectives *o, const Camera *cam, float speed_kmh);

void objectives_draw(const Objectives *o);

const char *objective_text(int index);

#endif
