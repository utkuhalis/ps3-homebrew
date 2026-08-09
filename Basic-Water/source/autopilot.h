#ifndef AUTOPILOT_H
#define AUTOPILOT_H

#include "flight.h"

/* Otopilot: irtifa, yon ve hiz tutar.
 *
 * Donanim bagimsiz ve saftir (birim testli). Ucaga dogrudan dokunmaz;
 * kumanda girdisi uretir - tipki oyuncunun yaptigi gibi. Boylece ucus
 * modeli tek yerden calisir ve otopilot da ayni fizige tabidir. */

#define AP_MAX_BANK_RAD    0.42f    /* ~24 derece, yolcu ucagi konforu */
#define AP_MAX_PITCH_RAD   0.22f
#define AP_VS_LIMIT_MS      8.0f    /* hedef tirmanma/alcalma hizi siniri */

typedef struct {
    int   engaged;
    float target_alt;       /* metre */
    float target_heading;   /* radyan */
    float target_speed;     /* m/s */
} Autopilot;

void autopilot_init(Autopilot *ap);

/* Devreye alir: o anki irtifa, yon ve hizi hedef olarak kilitler. */
void autopilot_engage(Autopilot *ap, const Flight *f);
void autopilot_disengage(Autopilot *ap);
void autopilot_toggle(Autopilot *ap, const Flight *f);

/* Hedefleri elle kaydirir (menu ya da tuslarla) */
void autopilot_adjust_alt(Autopilot *ap, float delta);
void autopilot_adjust_heading(Autopilot *ap, float delta);

/* Kumanda girdilerini uretir. engaged degilse ucaga dokunmaz. */
void autopilot_update(const Autopilot *ap, Flight *f, float dt);

/* -pi..pi araligina indirger (yon farki hesabinda kullanilir) */
float autopilot_wrap_angle(float a);

#endif
