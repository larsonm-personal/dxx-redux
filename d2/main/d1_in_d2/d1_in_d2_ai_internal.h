/* Source behavior meanings shared only within the D1 AI implementation */
#ifndef D1_IN_D2_AI_INTERNAL_H
#define D1_IN_D2_AI_INTERNAL_H

#include "vecmat.h"
struct object;

enum { D1_AIB_HIDE = 0x82, D1_AIB_FOLLOW_PATH = 0x84, D1_AIM_HIDE = 5 };
enum { D1_AISM_GOHIDE = 0, D1_AISM_HIDING = 1 };

/* Native frame and path motion share source turning, without D2 seismic policy */
void d1_in_d2_ai_turn_towards_vector(vms_vector *goal, struct object *obj, fix rate);

#endif
