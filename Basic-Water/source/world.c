#include <stddef.h>

#include "world.h"
#include "scene.h"
#include "runway.h"
#include "flight.h"

/* --- Dunya 1: acik deniz ---
 * Gokyuzu, su ve iki pist. Projenin baslangicindan beri var olan ortam. */

static int sea_init(void)
{
    int rc = scene_init();

    if (rc < 0)
        return rc;
    return runway_init();
}

static void sea_draw(const Camera *cam, const Mat4 *proj, float time_sec,
                     const Atmosphere *atm)
{
    scene_draw(cam, proj, time_sec, atm);
    runway_draw(cam, proj, atm);
}

static void sea_start(float out_xz[2], float *heading, float *offset)
{
    out_xz[0] = RUNWAY_POS[0][0];
    out_xz[1] = RUNWAY_POS[0][1];
    *heading = 0.55f;
    *offset = RUNWAY_LENGTH * 0.45f;
}

static void sea_extent(float *half_len, float *half_wid)
{
    *half_len = RUNWAY_LENGTH * 0.5f;
    *half_wid = RUNWAY_WIDTH * 0.5f;
}

static const World WORLDS[] = {
    { "Open Sea", sea_init, sea_draw, sea_start, sea_extent },
};

#define WORLD_COUNT ((int)(sizeof(WORLDS) / sizeof(WORLDS[0])))

static int current = 0;

int world_count(void)
{
    return WORLD_COUNT;
}

const World *world_get(int index)
{
    if (index < 0 || index >= WORLD_COUNT)
        return NULL;
    return &WORLDS[index];
}

int world_current_index(void)
{
    return current;
}

void world_select(int index)
{
    if (index >= 0 && index < WORLD_COUNT)
        current = index;
}

const World *world_current(void)
{
    return &WORLDS[current];
}

int world_init_all(void)
{
    int i;

    for (i = 0; i < WORLD_COUNT; i++) {
        if (WORLDS[i].init != NULL) {
            int rc = WORLDS[i].init();

            if (rc < 0)
                return rc;
        }
    }
    current = 0;
    return 0;
}
