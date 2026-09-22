/* Original D1 object ordering for the engine's prepared view */
#ifndef D1_IN_D2_RENDER_H
#define D1_IN_D2_RENDER_H
#include "render.h"
/* Returns handled for D1; inactive leaves the host lists untouched */
int d1_in_d2_build_object_lists(int n_segs, const short *segments,
    short objects[][OBJS_PER_SEG], const vms_vector *eye);
#endif
