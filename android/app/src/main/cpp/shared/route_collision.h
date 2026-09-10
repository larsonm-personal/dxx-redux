#ifndef DXX_ROUTE_COLLISION_H
#define DXX_ROUTE_COLLISION_H

#include "object.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Route pose occupancy, including closed doors and grates */
int route_object_intersects_blocking_wall(const object *objp);

#ifdef __cplusplus
}
#endif

#endif
