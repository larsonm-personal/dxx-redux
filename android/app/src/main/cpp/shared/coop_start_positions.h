#ifndef COOP_START_POSITIONS_H
#define COOP_START_POSITIONS_H

#include "vecmat.h"

int coop_find_fanout_start(int source, int assigned_count, vms_vector *pos,
                           short *segnum);

#endif
