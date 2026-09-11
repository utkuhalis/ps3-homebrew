#ifndef PROFILER_H
#define PROFILER_H

/* Frame time measurement.
 *
 * To measure what is expensive instead of guessing. Every section is
 * measured in microseconds and averaged over recent frames, so single-frame
 * noise disappears.
 *
 * The clock is the system time (sysGetSystemTime), not a hardware counter:
 * it does not show GPU work directly. But RSX falling behind shows up as
 * waiting in the FLIP section - so if FLIP is large the bottleneck is the
 * GPU, if the others are large it is the CPU. */

typedef enum {
    PROF_FLIGHT = 0,    /* flight model + autopilot + audio update */
    PROF_MODEL,         /* transforming aircraft vertices on the CPU */
    PROF_SCENE,         /* sky + water draw commands */
    PROF_RUNWAY,        /* runways */
    PROF_AIRCRAFT,      /* aircraft drawing */
    PROF_OVERLAY,       /* collecting and sending the HUD */
    PROF_FLIP,          /* end of frame: waiting for RSX and flipping */
    PROF_COUNT
} ProfSection;

/* Start of frame */
void  prof_frame_begin(void);

/* Start and end of a section */
void  prof_begin(ProfSection s);
void  prof_end(ProfSection s);

/* End of frame: updates the averages */
void  prof_frame_end(void);

/* Average values (microseconds) */
float prof_avg_us(ProfSection s);
float prof_frame_us(void);
float prof_fps(void);

/* Section name (for the HUD) */
const char *prof_name(ProfSection s);

/* Draw load counters - reported by hand every frame */
void  prof_set_counts(unsigned int triangles, unsigned int overlay_rects);
unsigned int prof_triangles(void);
unsigned int prof_rects(void);

#endif
