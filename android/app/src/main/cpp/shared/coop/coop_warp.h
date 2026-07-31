/*
 * Coop warp-to-player system.
 *
 * Allows a player to teleport to a teammate in coop when they are
 * separated by at least the configured minimum distance.
 *
 * Shared between d1 and d2.
 */

#ifndef COOP_WARP_H
#define COOP_WARP_H

#include "pstypes.h"
#include "fix.h"

/* --- tunable constants --- */
#define COOP_WARP_DISTANCE_THRESHOLD (F1_0 * 160)
#define COOP_WARP_MAX_RETRIES        30
#define COOP_WARP_OFFSET_SCALE       4

static inline int coop_warp_distance_allows(fix distance)
{
	return distance >= COOP_WARP_DISTANCE_THRESHOLD;
}

/* Reset target selection (call on level start). */
void coop_warp_reset(void);

/* --- warp availability query (called from JNI) --- */

/* Result struct returned by coop_warp_get_status().
 * If available==1, target_pnum and target_callsign are valid. */
typedef struct coop_warp_status {
	int available;           /* 1 if warp is currently allowed */
	int target_pnum;         /* player index of current warp target */
	char target_callsign[9]; /* CALLSIGN_LEN+1 */
} coop_warp_status;

/* Get current warp status.  A live coop teammate is eligible when
 * they are at least COOP_WARP_DISTANCE_THRESHOLD away. */
void coop_warp_get_status(coop_warp_status *out);

/* Advance to the next eligible warp target (for >2 player games). */
void coop_warp_cycle_target(void);

/* --- warp execution --- */

/* Execute a warp to the current target.  Returns 1 on success, 0 on
 * failure (no clear space, target moved, etc). */
int coop_warp_execute(void);

/* Process a received MULTI_WARP_TO_PLAYER packet.
 * Moves the warping player's object to the new position. */
void coop_warp_do_packet(const unsigned char *buf);

/* Send MULTI_WARP_TO_PLAYER packet after a successful local warp. */
void coop_warp_send_packet(int target_pnum, int dest_segnum, int x, int y, int z);

#endif /* COOP_WARP_H */
