#ifndef AIRCRAFT_H
#define AIRCRAFT_H

#include "flight.h"
#include "camera.h"
#include "mat4.h"
#include "atmosphere.h"

/* Ucak govdesi ve hareketli kumanda yuzeyleri.
 *
 * Model kodla uretilir; govde, kanatlar, kuyruk ve motorlar sabit, flap,
 * aileron, spoiler, elevator ve rudder ise kendi mentese eksenlerinde doner.
 * Her karede tum parcalar ucagin konum ve yonelimine gore donusturulup tek
 * cizim cagrisinda gonderilir. */

int  aircraft_init(void);
void aircraft_draw(const Flight *f, const Camera *cam, const Mat4 *proj,
                   const Atmosphere *atm);

/* Kokpit ve dis kamera icin ucak uzerindeki referans noktalari */
void aircraft_cockpit_pos(const Flight *f, float out[3]);
void aircraft_chase_pos(const Flight *f, float out[3]);

#endif
