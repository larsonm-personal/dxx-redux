#ifndef COOP_GAMEPLAY_RUNTIME_H
#define COOP_GAMEPLAY_RUNTIME_H

#include "game.h"
#include "gameseq.h"
#include "coop_gameplay_fence.h"
#include "coop_world_visit.h"
#include "coop_travel.h"
#include "coop_briefing.h"

/* Completed loads publish life bookkeeping directly, without REAPPEAR traffic */
void coop_gameplay_restore_player_life(void);

/* Capture at enqueue time, never at retry or relay time */
static inline coop_gameplay_stamp coop_gameplay_current_stamp(void)
{
	coop_gameplay_stamp stamp = { 0 };
	if ((Game_mode & GM_MULTI_COOP) && Current_level_num) {
		stamp.visit = coop_world_visit_current();
		stamp.level = Current_level_num;
		stamp.frozen = coop_travel_blocks_world_updates() ||
		               multi_save_transfer_restoring() || multi_save_transfer_paused() || coop_briefing_active();
	}
	return stamp;
}

/* Exit status changes connection state, statistics and reactor timing. Ordinary
 * exit waiting keeps the world open; secret/load/briefing barriers close it */
static inline int coop_gameplay_endlevel_packet_allowed(const unsigned char *data, int size, int expected)
{
	coop_gameplay_stamp sent, current = coop_gameplay_current_stamp();
	if (size != expected || size < COOP_GAMEPLAY_STAMP_BYTES ||
	    !coop_gameplay_stamp_read(&sent, data + size - COOP_GAMEPLAY_STAMP_BYTES,
	                              COOP_GAMEPLAY_STAMP_BYTES)) return 0;
	return !(Game_mode & GM_MULTI_COOP) ||
	       coop_gameplay_endlevel_stamp_allowed(&sent, &current, coop_travel_ending_campaign());
}

#endif
