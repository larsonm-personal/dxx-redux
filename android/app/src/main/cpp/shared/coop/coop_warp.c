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
#include "multi.h"
#include "game.h"
#include "ai.h"
#include "hudmsg.h"
#include "vecmat.h"
#include "byteswap.h"
#include "console.h"

#include <string.h>

/* --- state --- */

static int coop_warp_target_idx; /* cycles through eligible targets */

/* --- helpers --- */

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
	if (!coop_warp_distance_allows(
	        vm_vec_dist_quick(&my_obj->pos, &their_obj->pos)))
		return 0;

	return 1;
}

/* Find the next valid target starting from coop_warp_target_idx. */
static int coop_find_next_target(void)
{
	for (int i = 0; i < N_players; i++) {
		int idx = (coop_warp_target_idx + i) % N_players;
		if (coop_is_valid_target(idx))
			return idx;
	}
	return -1;
}

/* --- public API --- */

void coop_warp_reset(void)
{
	coop_warp_target_idx = 0;
}

void coop_warp_get_status(coop_warp_status *out)
{
	memset(out, 0, sizeof(*out));

	/* must be in coop game */
	if (!(Game_mode & GM_MULTI_COOP))
		return;

	/* must be alive */
	if (Players[Player_num].connected != CONNECT_PLAYING)
		return;
	if (Objects[Players[Player_num].objnum].type != OBJ_PLAYER)
		return;

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
	if (N_players <= 0)
		return;

	/* advance past current target to find the next one */
	coop_warp_target_idx = (coop_warp_target_idx + 1) % N_players;

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

	/* send to other players */
	coop_warp_send_packet(target_pnum, dest_seg, candidate.x, candidate.y, candidate.z);

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

void coop_warp_send_packet(int target_pnum, int dest_segnum, int x, int y, int z)
{
	int count = 0;

	multibuf[count] = MULTI_WARP_TO_PLAYER;
	count += 1;
	multibuf[count] = (ubyte) Player_num;
	count += 1;
	multibuf[count] = (ubyte) target_pnum;
	count += 1;

	PUT_INTEL_INT(multibuf + count, x);
	count += 4;
	PUT_INTEL_INT(multibuf + count, y);
	count += 4;
	PUT_INTEL_INT(multibuf + count, z);
	count += 4;
	PUT_INTEL_SHORT(multibuf + count, (short) dest_segnum);
	count += 2;

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
		return; /* don't process our own warp */

	new_pos.x = GET_INTEL_INT(buf + 3);
	new_pos.y = GET_INTEL_INT(buf + 7);
	new_pos.z = GET_INTEL_INT(buf + 11);
	new_seg = GET_INTEL_SHORT(buf + 15);

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

#endif /* __ANDROID__ */
