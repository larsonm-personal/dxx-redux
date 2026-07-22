#ifndef COOP_LEVEL_RESTART_H
#define COOP_LEVEL_RESTART_H

#include "rewind_file.h"

typedef enum coop_level_restart_state {
	COOP_LEVEL_RESTART_HIDDEN = 0,
	COOP_LEVEL_RESTART_CAPTURING = 1,
	COOP_LEVEL_RESTART_READY = 2,
	COOP_LEVEL_RESTART_BUSY = 3,
	COOP_LEVEL_RESTART_BLOCKED = 4,
} coop_level_restart_state;

void coop_level_restart_note_natural_level(int level_num);
void coop_level_restart_maybe_capture_ready(void);
void coop_level_restart_note_restore_begin(void);
void coop_level_restart_note_restore_end(int restored);
int coop_level_restart_get_state(void);
int coop_level_restart_request(void);
int coop_level_restart_load_retained_and_request(void);
int coop_level_restart_apply_host(void);
const rewind_memory_buffer *coop_level_restart_buffer(void);
void coop_level_restart_transfer_finished(int restored);
void coop_level_restart_clear(void);

#endif
