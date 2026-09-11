#ifndef AIRCRAFT_H
#define AIRCRAFT_H

#include "flight.h"
#include "camera.h"
#include "mat4.h"
#include "atmosphere.h"

/* Aircraft body.
 *
 * The model is the Boeing 737 from AirportPack, imported by
 * tools/blender/import_737.py and converted by tools/glb_to_mesh.py into a
 * binary embedded in the EBOOT. Vertices are transformed to the aircraft's
 * position and attitude every frame. The landing gear is made of separate
 * parts, so it is not drawn while retracted. */

int  aircraft_init(void);
void aircraft_draw(const Flight *f, const Camera *cam, const Mat4 *proj,
                   const Atmosphere *atm);

/* Reference points on the aircraft for the cockpit and outside cameras */
void aircraft_cockpit_pos(const Flight *f, float out[3]);
void aircraft_chase_pos(const Flight *f, float out[3]);

/* Total triangle count of the model (diagnostics) */
unsigned int aircraft_triangle_count(void);

/* Radius of the sphere enclosing the body (camera collision) */
float aircraft_bound_radius(void);

/* Triangles actually drawn this frame (after LOD) */
unsigned int aircraft_drawn_triangles(void);

#endif
