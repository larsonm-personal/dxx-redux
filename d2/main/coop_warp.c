/*
 * Coop warp-to-player system.
 *
 * android port: coop QoL -- Phase 5
 *
 * Shared between d1 and d2.
 */

#ifdef __ANDROID__

#include "coop_warp.h"
#include "player.h"
#include "object.h"
#include "segment.h"
#include "gameseg.h"
#include "wall.h"
#include "multi.h"
#include "game.h"
#include "ai.h"
#include "hudmsg.h"
#include "vecmat.h"
#include "timer.h"
#include "byteswap.h"
#include "console.h"

#include <string.h>

/* --- state --- */

static fix64 last_robot_engagement_time;
static fix64 last_warp_time;
static int   coop_warp_target_idx;  /* cycles through eligible targets */
static int   coop_warp_recently_respawned; /* set when player dies, cleared after timeout */
static fix64 coop_warp_respawn_time;

/* --- helpers --- */

/* Check if a side's wall allows passage (considering keys held by
 * any player in coop -- but in coop, keys are shared so just check
 * the local player's flags). */
static int coop_side_is_passable(int segnum, int sidenum)
{
	int wall_num;
	wall *wallp;
	segment *segp = &Segments[segnum];

	if (!IS_CHILD(segp->children[sidenum]))
		return 0;

	wall_num = segp->sides[sidenum].wall_num;
	if (wall_num == -1)
		return 1;  /* no wall = passable */

	wallp = &Walls[wall_num];

	/* closed walls are never passable */
	if (wallp->type == WALL_CLOSED)
		return 0;

	/* check key requirements */
	if (wallp->keys != KEY_NONE) {
		/* In coop, keys are shared across all players.  The local
		 * player's flags already contain all keys picked up by
		 * anyone, so checking Player_num is sufficient. */
		unsigned int all_keys = Players[Player_num].flags;
		if (wallp->keys == KEY_BLUE)
			return (all_keys & PLAYER_FLAGS_BLUE_KEY) ? 1 : 0;
		if (wallp->keys == KEY_GOLD)
			return (all_keys & PLAYER_FLAGS_GOLD_KEY) ? 1 : 0;
		if (wallp->keys == KEY_RED)
			return (all_keys & PLAYER_FLAGS_RED_KEY) ? 1 : 0;
	}

	/* locked doors without key requirements */
	if (wallp->type == WALL_DOOR &&
	    (wallp->flags & WALL_DOOR_LOCKED) &&
	    wallp->state == WALL_DOOR_CLOSED)
		return 0;

	return 1;
}

/* BFS from start_seg, filling visited[] with 1 for reachable segs.
 * Returns 1 if target_seg is reachable, 0 otherwise. */
static int coop_bfs_reachable(int start_seg, int target_seg)
{
	static short bfs_queue[MAX_SEGMENTS];
	static sbyte visited[MAX_SEGMENTS];
	int head = 0, tail = 0;

	if (start_seg == target_seg)
		return 1;
	if (start_seg < 0 || target_seg < 0)
		return 0;

	memset(visited, 0, (Highest_segment_index + 1) * sizeof(visited[0]));

	bfs_queue[head++] = start_seg;
	visited[start_seg] = 1;

	while (tail < head) {
		int curseg = bfs_queue[tail++];
		segment *segp = &Segments[curseg];

		for (int i = 0; i < MAX_SIDES_PER_SEGMENT; i++) {
			int child = segp->children[i];
			if (IS_CHILD(child) && child <= Highest_segment_index &&
			    !visited[child] && coop_side_is_passable(curseg, i)) {
				if (child == target_seg)
					return 1;
				visited[child] = 1;
				if (head < MAX_SEGMENTS)
					bfs_queue[head++] = child;
			}
		}
	}
	return 0;
}

/* Return the engagement timeout to use based on whether player
 * recently respawned. */
static fix64 coop_engagement_timeout(void)
{
	if (coop_warp_recently_respawned &&
	    timer_query() - coop_warp_respawn_time < COOP_WARP_RESPAWN_TIMEOUT)
		return COOP_WARP_RESPAWN_TIMEOUT;
	coop_warp_recently_respawned = 0;
	return COOP_WARP_ENGAGEMENT_TIMEOUT;
}

/* Check if player pnum is a valid warp target. */
static int coop_is_valid_target(int pnum)
{
	object *my_obj, *their_obj;

	if (pnum == Player_num)
		return 0;
	if (pnum < 0 || pnum >= N_players)
		return 0;
	if (Players[pnum].connected != CONNECT_PLAYING)
		return 0;

	my_obj = &Objects[Players[Player_num].objnum];
	their_obj = &Objects[Players[pnum].objnum];

	if (their_obj->type != OBJ_PLAYER)
		return 0;

	/* distance check */
	if (vm_vec_dist_quick(&my_obj->pos, &their_obj->pos) <
	    COOP_WARP_DISTANCE_THRESHOLD)
		return 0;

	/* reachability check (locked doors) */
	if (!coop_bfs_reachable(my_obj->segnum, their_obj->segnum))
		return 0;

	return 1;
}

/* Find the next valid target starting from coop_warp_target_idx. */
static int coop_find_next_target(void)
{
	for (int i = 0; i < N_players; i++) {
		int idx = (coop_warp_target_idx + i) % MAX_PLAYERS;
		if (coop_is_valid_target(idx))
			return idx;
	}
	return -1;
}

/* --- public API --- */

void coop_warp_record_engagement(void)
{
	last_robot_engagement_time = timer_query();
}

void coop_warp_reset(void)
{
	last_robot_engagement_time = timer_query();
	last_warp_time = 0;
	coop_warp_target_idx = 0;
	coop_warp_recently_respawned = 0;
}

void coop_warp_get_status(coop_warp_status *out)
{
	fix64 now = timer_query();

	memset(out, 0, sizeof(*out));

	/* must be in coop game */
	if (!(Game_mode & GM_MULTI_COOP))
		return;

	/* must be alive */
	if (Players[Player_num].connected != CONNECT_PLAYING)
		return;
	if (Objects[Players[Player_num].objnum].type != OBJ_PLAYER)
		return;

	/* cooldown check */
	if (last_warp_time > 0 && now - last_warp_time < COOP_WARP_COOLDOWN) {
		out->cooldown_secs_left = (int)((COOP_WARP_COOLDOWN -
		                                 (now - last_warp_time)) / F1_0) + 1;
		return;
	}

	/* engagement check */
	if (now - last_robot_engagement_time < coop_engagement_timeout()) {
		out->engaged = 1;
		return;
	}

	/* find a valid target */
	int target = coop_find_next_target();
	if (target < 0)
		return;

	out->available = 1;
	out->target_pnum = target;
	memcpy(out->target_callsign, Players[target].callsign,
	       sizeof(out->target_callsign) - 1);
	out->target_callsign[sizeof(out->target_callsign) - 1] = '\0';
}

void coop_warp_cycle_target(void)
{
	/* advance past current target to find the next one */
	coop_warp_target_idx = (coop_warp_target_idx + 1) % MAX_PLAYERS;

	/* wrap around to find next valid target */
	int target = coop_find_next_target();
	if (target >= 0)
		coop_warp_target_idx = target;
}

int coop_warp_execute(void)
{
	coop_warp_status status;
	coop_warp_get_status(&status);

	if (!status.available) {
		HUD_init_message_literal(HM_MULTI, "Warp not available");
		return 0;
	}

	int target_pnum = status.target_pnum;
	object *target_obj = &Objects[Players[target_pnum].objnum];
	object *my_obj = &Objects[Players[Player_num].objnum];
	fix offset_dist = my_obj->size * COOP_WARP_OFFSET_SCALE;

	vms_vector candidate;
	int dest_seg = -1;

	for (int attempt = 0; attempt < COOP_WARP_MAX_RETRIES; attempt++) {
		vms_vector rand_dir;
		make_random_vector(&rand_dir);
		vm_vec_scale_add(&candidate, &target_obj->pos, &rand_dir, offset_dist);

		dest_seg = find_point_seg(&candidate, target_obj->segnum);
		if (dest_seg >= 0)
			break;
	}

	if (dest_seg < 0) {
		HUD_init_message(HM_MULTI, "Warp failed -- no clear space near %s",
		                 Players[target_pnum].callsign);
		return 0;
	}

	/* move local player */
	my_obj->pos = candidate;
	obj_relink(Players[Player_num].objnum, dest_seg);

	/* set cooldown */
	last_warp_time = timer_query();

	/* send to other players */
	coop_warp_send_packet(dest_seg, candidate.x, candidate.y, candidate.z);

	HUD_init_message(HM_MULTI, "Warped to %s",
	                 Players[target_pnum].callsign);

	con_printf(CON_NORMAL, "coop_warp: warped to player %d (%s) seg %d\n",
	           target_pnum, Players[target_pnum].callsign, dest_seg);

	return 1;
}

/* --- network packet --- */

/* Packet layout (17 bytes):
 *   [0]    type = MULTI_WARP_TO_PLAYER
 *   [1]    warper pnum
 *   [2]    target pnum (for HUD display only)
 *   [3..6] x position (int, little-endian)
 *   [7..10] y position
 *   [11..14] z position
 *   [15..16] segment (short, little-endian)  */

void coop_warp_send_packet(int dest_segnum, int x, int y, int z)
{
	int count = 0;

	multibuf[count] = MULTI_WARP_TO_PLAYER;    count += 1;
	multibuf[count] = (ubyte)Player_num;        count += 1;
	/* target pnum -- look up from status for display purposes */
	{
		coop_warp_status st;
		coop_warp_get_status(&st);
		multibuf[count] = (ubyte)st.target_pnum;
	}
	count += 1;

	PUT_INTEL_INT(multibuf + count, x);         count += 4;
	PUT_INTEL_INT(multibuf + count, y);         count += 4;
	PUT_INTEL_INT(multibuf + count, z);         count += 4;
	PUT_INTEL_SHORT(multibuf + count, (short)dest_segnum); count += 2;

	multi_send_data(multibuf, count, 2);
}

void coop_warp_do_packet(const unsigned char *buf)
{
	int warper_pnum = buf[1];
	int target_pnum = buf[2];
	vms_vector new_pos;
	int new_seg;

	if (warper_pnum < 0 || warper_pnum >= MAX_PLAYERS)
		return;
	if (warper_pnum == Player_num)
		return;  /* don't process our own warp */

	new_pos.x = GET_INTEL_INT(buf + 3);
	new_pos.y = GET_INTEL_INT(buf + 7);
	new_pos.z = GET_INTEL_INT(buf + 11);
	new_seg   = GET_INTEL_SHORT(buf + 15);

	if (new_seg < 0 || new_seg > Highest_segment_index)
		return;

	/* move the warping player's object */
	int objnum = Players[warper_pnum].objnum;
	if (objnum >= 0 && objnum <= Highest_object_index &&
	    Objects[objnum].type == OBJ_PLAYER) {
		Objects[objnum].pos = new_pos;
		obj_relink(objnum, new_seg);
	}

	/* HUD notification */
	if (target_pnum >= 0 && target_pnum < MAX_PLAYERS) {
		HUD_init_message(HM_MULTI, "%s warped to %s",
		                 Players[warper_pnum].callsign,
		                 Players[target_pnum].callsign);
	}
}

/* --- respawn tracking (called from multi.c when player dies/respawns) --- */

void coop_warp_note_respawn(void)
{
	coop_warp_recently_respawned = 1;
	coop_warp_respawn_time = timer_query();
	/* also reset engagement time so warp becomes available sooner */
	last_robot_engagement_time = 0;
}

#endif /* __ANDROID__ */
