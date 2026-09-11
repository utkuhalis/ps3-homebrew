#ifndef AUDIO_H
#define AUDIO_H

#include "flight.h"
#include "atmosphere.h"

/* Flight audio: engine, wind and sea.
 *
 * No sound files are used; waveforms are computed every frame. That keeps
 * the file size at zero and lets the sound follow the flight continuously:
 * the engine rises with throttle, the wind grows with speed, and the waves
 * get louder near sea level.
 *
 * Audio is generated on its own thread and does not affect the frame rate. */

int  audio_init(void);
void audio_exit(void);

/* Called every frame from the main loop; synthesis follows these values. */
void audio_update(const Flight *f, const Atmosphere *atm);

#endif
