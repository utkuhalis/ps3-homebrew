#include <string.h>

#include <lv2/systime.h>

#include "profiler.h"

/* Ortalama penceresi: 60 kare = 1 saniye. Daha kisa tutmak sayilari
 * okunamayacak kadar oynatiyor. */
#define WINDOW 60

static s64   start_us[PROF_COUNT];
static float accum_us[PROF_COUNT];      /* pencere icinde toplam */
static float avg_us[PROF_COUNT];        /* son tamamlanan pencere ortalamasi */

static s64   frame_start_us;
static float frame_accum_us;
static float frame_avg_us = 16666.0f;

static int   samples;
static unsigned int tri_count, rect_count;

static const char *NAMES[PROF_COUNT] = {
    "FLIGHT", "MODEL", "SCENE", "RUNWAY", "PLANE", "HUD", "FLIP"
};

void prof_frame_begin(void)
{
    frame_start_us = sysGetSystemTime();
}

void prof_begin(ProfSection s)
{
    if (s >= 0 && s < PROF_COUNT)
        start_us[s] = sysGetSystemTime();
}

void prof_end(ProfSection s)
{
    if (s >= 0 && s < PROF_COUNT)
        accum_us[s] += (float)(sysGetSystemTime() - start_us[s]);
}

void prof_frame_end(void)
{
    int i;

    frame_accum_us += (float)(sysGetSystemTime() - frame_start_us);
    samples++;

    if (samples >= WINDOW) {
        for (i = 0; i < PROF_COUNT; i++) {
            avg_us[i] = accum_us[i] / (float)samples;
            accum_us[i] = 0.0f;
        }
        frame_avg_us = frame_accum_us / (float)samples;
        frame_accum_us = 0.0f;
        samples = 0;
    }
}

float prof_avg_us(ProfSection s)
{
    if (s < 0 || s >= PROF_COUNT)
        return 0.0f;
    return avg_us[s];
}

float prof_frame_us(void)
{
    return frame_avg_us;
}

float prof_fps(void)
{
    if (frame_avg_us < 1.0f)
        return 0.0f;
    return 1000000.0f / frame_avg_us;
}

const char *prof_name(ProfSection s)
{
    if (s < 0 || s >= PROF_COUNT)
        return "?";
    return NAMES[s];
}

void prof_set_counts(unsigned int triangles, unsigned int overlay_rects)
{
    tri_count = triangles;
    rect_count = overlay_rects;
}

unsigned int prof_triangles(void) { return tri_count; }
unsigned int prof_rects(void)     { return rect_count; }
