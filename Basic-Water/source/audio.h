#ifndef AUDIO_H
#define AUDIO_H

#include "flight.h"
#include "atmosphere.h"

/* Ucus sesi: motor, ruzgar ve deniz.
 *
 * Ses dosyasi kullanilmaz; dalga bicimleri her karede hesaplanir. Bu hem
 * dosya boyutunu sifirlar hem de sesin ucusa surekli tepki vermesini saglar:
 * gaz acilinca motor tizlesir, hiz artinca ruzgar siddetlenir, deniz
 * seviyesine inince dalga ugultusu yukselir.
 *
 * Ses ayri bir is parcaciginda uretilir; ana dongunun kare hizini etkilemez. */

int  audio_init(void);
void audio_exit(void);

/* Ana dongude her karede cagrilir; ses uretimi bu degerleri takip eder. */
void audio_update(const Flight *f, const Atmosphere *atm);

#endif
