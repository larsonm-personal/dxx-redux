#ifndef COOP_MULTI_STATUS_H
#define COOP_MULTI_STATUS_H

#include "player.h"
#include "pstypes.h"

// -- Coop kill stats (android port: coop QoL overlay) --
// Per-player robot kill tracking for coop score overlay
typedef struct coop_player_kill_stats {
	int robots_killed;
	int score_earned;
} coop_player_kill_stats;

extern coop_player_kill_stats Coop_kill_stats[MAX_PLAYERS];
extern int Coop_total_robot_score; // sum of score_value for all robots at level start

// Call when a robot is killed: pnum = player who got the kill
void coop_record_robot_kill(int pnum, int score_value);

// Reset stats for a new level
void coop_reset_kill_stats(void);

// Compute total robot score value by iterating all OBJ_ROBOT in the level
int coop_compute_total_robot_score(void);

// Resolve a killer objnum to a player index, or -1 if not a player kill
int coop_killer_to_pnum(int killer_objnum);

// Periodic coop status broadcast (android port: coop QoL overlay)
void coop_send_peer_status(void);
void coop_do_peer_status(const ubyte *buf);

// Send/receive cached inventory to a rejoining coop player
void coop_send_restore_inventory(int pnum);
void coop_do_restore_inventory(const ubyte *buf, int authenticated_sender);

#endif /* COOP_MULTI_STATUS_H */
