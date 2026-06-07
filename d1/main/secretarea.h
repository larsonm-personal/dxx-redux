#ifndef _SECRETAREA_H
#define _SECRETAREA_H

#include "secret_area_scan.h"

void secret_area_rescan_current_level(void);
const secret_area_state *secret_area_get_state(void);
int secret_area_note_segment_entered(int segnum);
void secret_area_restore_saved_found(int saved_total, const unsigned char *found, int found_capacity, const unsigned char *visited, int visited_count);
void secret_area_restore_found_from_automap(const unsigned char *visited, int visited_count);
int secret_area_get_reveal_unfound(void);
void secret_area_set_reveal_unfound(int reveal);

#endif
