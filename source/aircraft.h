#ifndef AIRCRAFT_H
#define AIRCRAFT_H

#include "flight.h"
#include "camera.h"
#include "mat4.h"
#include "atmosphere.h"

/* Ucak govdesi.
 *
 * Model gercek bir glTF varligindan gelir (assets/model/plane.glb);
 * tools/glb_to_mesh.py ile ikili bicime cevrilip EBOOT'a gomulur. Vertexler
 * her karede ucagin konum ve yonelimine gore donusturulur. Inis takimi ayri
 * parcalar oldugu icin takim kapaliyken cizilmez. */

int  aircraft_init(void);
void aircraft_draw(const Flight *f, const Camera *cam, const Mat4 *proj,
                   const Atmosphere *atm);

/* Kokpit ve dis kamera icin ucak uzerindeki referans noktalari */
void aircraft_cockpit_pos(const Flight *f, float out[3]);
void aircraft_chase_pos(const Flight *f, float out[3]);

/* Modeldeki toplam ucgen sayisi (tani icin) */
unsigned int aircraft_triangle_count(void);

/* O karede gercekten cizilen ucgen sayisi (LOD sonrasi) */
unsigned int aircraft_drawn_triangles(void);

#endif
