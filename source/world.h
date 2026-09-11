#ifndef WORLD_H
#define WORLD_H

#include "camera.h"
#include "mat4.h"
#include "atmosphere.h"

/* World (scene) layer.
 *
 * The game is not tied to one scene: every world provides its own
 * init/draw, and the main loop only draws "the one selected now". So a new
 * environment can be added without touching the existing one - if it goes
 * badly, removing the world from the list is enough.
 *
 * The flight model, camera, HUD and audio are independent of the world;
 * all of them work the same in every scene. A world only defines the
 * ENVIRONMENT: ground, sky, runways, structures. */

typedef struct {
    const char *name;

    /* Once, at program start. 0: success */
    int  (*init)(void);

    /* Draws the scene (3D pass). The overlay is a separate layer. */
    void (*draw)(const Camera *cam, const Mat4 *proj, float time_sec,
                 const Atmosphere *atm);

    /* Runway centre, heading and threshold offset the aircraft starts from */
    void (*start_point)(float out_xz[2], float *heading, float *offset);

    /* Rectangle where ground contact applies (half dimensions) */
    void (*ground_extent)(float *half_len, float *half_wid);
} World;

/* Number of registered worlds and access */
int          world_count(void);
const World *world_get(int index);

/* The currently selected world */
int          world_current_index(void);
void         world_select(int index);
const World *world_current(void);

/* Initialises all worlds; the first one is selected. 0: success */
int          world_init_all(void);

#endif
