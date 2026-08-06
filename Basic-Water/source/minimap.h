#ifndef MINIMAP_H
#define MINIMAP_H

#include "camera.h"

/* Sol alt kose mini haritasi: rota, ucagin konumu ve yonu, kalan mesafe.
 * Pistler eklendiginde kalkis/inis noktalari gercek koordinatlara baglanacak;
 * su an sabit iki nokta arasindaki rota gosteriliyor. */

void minimap_draw(const Camera *cam);

#endif
