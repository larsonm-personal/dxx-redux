#ifndef _SECRETAREA_H
#define _SECRETAREA_H

#include "secret_area_scan.h"

void secret_area_rescan_current_level(void);
const secret_area_state *secret_area_get_state(void);
int secret_area_note_segment_entered(int segnum);

#endif
