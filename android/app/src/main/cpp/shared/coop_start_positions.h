#ifndef COOP_START_POSITIONS_H
#define COOP_START_POSITIONS_H

#include "vecmat.h"
struct obj_position;

int coop_find_fanout_start(int source, int assigned_count, vms_vector *pos,
                           short *segnum);

/* Plan the complete team before moving any ship; failure leaves output unused */
int coop_find_arrival_positions(const struct obj_position *anchor, int count,
                                struct obj_position *positions);

#endif
