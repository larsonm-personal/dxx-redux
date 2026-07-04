/*
 * Coop warp-to-player system.
 *
 * Allows a player to teleport to a teammate in coop when they are
 * far from the action and haven't engaged robots recently.  Warping
 * past locked doors is prevented via a BFS reachability check.
 *
 * Shared between d1 and d2.
 */

#ifndef COOP_WARP_H
#define COOP_WARP_H

#include "pstypes.h"
#include "fix.h"

/* --- tunable constants --- */
#define COOP_WARP_DISTANCE_THRESHOLD (F1_0 * 160)
#define COOP_WARP_ENGAGEMENT_TIMEOUT (F1_0 * 12) /* 12s no engagement */
#define COOP_WARP_RESPAWN_TIMEOUT    (F1_0 * 5)  /* shorter after respawn */
#define COOP_WARP_COOLDOWN           (F1_0 * 60) /* 60s between warps */
#define COOP_WARP_MAX_RETRIES        30
#define COOP_WARP_OFFSET_SCALE       4

/* --- engagement tracking --- */

/* Record that the local player engaged with a robot (dealt or took
 * damage).  Called from collide.c hooks. */
void coop_warp_record_engagement(void);

/* Reset engagement timer (call on level start). */
void coop_warp_reset(void);

/* --- warp availability query (called from JNI) --- */

/* Result struct returned by coop_warp_get_status().
 * If available==1, target_pnum and target_callsign are valid. */
typedef struct coop_warp_status {
	int available;           /* 1 if warp is currently allowed */
	int target_pnum;         /* player index of current warp target */
	char target_callsign[9]; /* CALLSIGN_LEN+1 */
	int cooldown_secs_left;  /* 0 if ready, >0 if on cooldown */
	int engaged;             /* 1 if recently engaged with robots */
} coop_warp_status;

/* Get current warp status.  Evaluates all conditions (distance,
 * engagement, cooldown, reachability, alive). */
void coop_warp_get_status(coop_warp_status *out);

/* Advance to the next eligible warp target (for >2 player games). */
void coop_warp_cycle_target(void);

/* --- warp execution --- */

/* Execute a warp to the current target.  Returns 1 on success, 0 on
 * failure (no clear space, target moved, etc).  On success, sets
 * cooldown and sends the network packet. */
int coop_warp_execute(void);

/* Process a received MULTI_WARP_TO_PLAYER packet.
 * Moves the warping player's object to the new position. */
void coop_warp_do_packet(const unsigned char *buf);

/* Send MULTI_WARP_TO_PLAYER packet after a successful local warp. */
void coop_warp_send_packet(int dest_segnum, int x, int y, int z);

/* Note that the player has just respawned (shorter engagement timeout). */
void coop_warp_note_respawn(void);

#endif /* COOP_WARP_H */
