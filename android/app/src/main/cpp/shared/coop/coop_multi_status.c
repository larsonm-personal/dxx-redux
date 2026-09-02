#include <string.h>

#include "byteswap.h"
#include "console.h"
#include "coop_multi_status.h"
#include "game.h"
#include "multi.h"
#include "object.h"
#include "robot.h"

#ifdef __ANDROID__
#include <physfs.h>

#include "coop_save.h"
#endif

// -- Coop kill stats (android port: coop QoL overlay) --

coop_player_kill_stats Coop_kill_stats[MAX_PLAYERS];
int Coop_total_robot_score = 0;

void coop_record_robot_kill(int pnum, int score_value)
{
	if (pnum < 0 || pnum >= MAX_PLAYERS)
		return;
	Coop_kill_stats[pnum].robots_killed++;
	Coop_kill_stats[pnum].score_earned += score_value;
}

void coop_reset_kill_stats(void)
{
	memset(Coop_kill_stats, 0, sizeof(Coop_kill_stats));
	Coop_total_robot_score = 0;
}

int coop_compute_total_robot_score(void)
{
	int total = 0;
	int i;
	for (i = 0; i <= Highest_object_index; i++) {
		if (Objects[i].type == OBJ_ROBOT)
			total += Robot_info[Objects[i].id].score_value;
	}
	return total;
}

// Resolve a killer objnum to a player index, or -1 if not a player kill
int coop_killer_to_pnum(int killer_objnum)
{
	if (killer_objnum < 0 || killer_objnum > Highest_object_index)
		return -1;
	if (Objects[killer_objnum].type == OBJ_PLAYER)
		return Objects[killer_objnum].id;
	if (Objects[killer_objnum].type == OBJ_WEAPON) {
		int parent = Objects[killer_objnum].ctype.laser_info.parent_num;
		if (parent >= 0 && parent <= Highest_object_index &&
		    Objects[parent].type == OBJ_PLAYER)
			return Objects[parent].id;
	}
	return -1;
}

#ifdef __ANDROID__
/* android port: periodic coop kill stats broadcast (shields/energy/weapons use
 * existing MULTI_DAMAGE/MULTI_REPAIR/MULTI_SHIP_STATUS packets instead) */
void coop_send_peer_status(void)
{
	multibuf[0] = MULTI_COOP_PEER_STATUS;
	multibuf[1] = Player_num;
	PUT_INTEL_SHORT(multibuf + 2, Coop_kill_stats[Player_num].robots_killed);
	PUT_INTEL_INT(multibuf + 4, Coop_kill_stats[Player_num].score_earned);
	multi_send_data(multibuf, 8, 0);
}

void coop_do_peer_status(const ubyte *buf)
{
	int pnum = buf[1];
	if (pnum < 0 || pnum >= MAX_PLAYERS || pnum == Player_num)
		return;
	Coop_kill_stats[pnum].robots_killed = GET_INTEL_SHORT(buf + 2);
	Coop_kill_stats[pnum].score_earned = GET_INTEL_INT(buf + 4);
}

static int coop_remove_rejoin_spew(int pnum)
{
	int i;
	int removed = 0;

	for (i = 0; i <= Highest_object_index; i++) {
		if (Objects[i].type != OBJ_POWERUP)
			continue;
		if (Objects[i].flags & OF_SHOULD_BE_DEAD)
			continue;
		if (object_owner[i] != pnum)
			continue;
		multi_send_remobj(i);
		Objects[i].flags |= OF_SHOULD_BE_DEAD;
		removed++;
	}

	return removed;
}

void coop_send_restore_inventory(int pnum)
{
	coop_player_record rec;
	int source_level = Current_level_num;
	int same_level;
	int removed;
	int i;

	if (!(Game_mode & GM_MULTI_COOP))
		return;
	if (!multi_i_am_master())
		return;
	if (!(Netgame.game_flags & NETGAME_FLAG_COOP_QOL))
		return;

	if (!coop_take_absent_player_with_level(Players[pnum].callsign,
	                                        Netgame.players[pnum].client_id,
	                                        &rec, &source_level)) {
		con_printf(CON_NORMAL, "coop_restore: no cached inventory for '%s'\n",
		           Players[pnum].callsign);
		return;
	}

	same_level = (source_level == Current_level_num);
	removed = same_level ? coop_remove_rejoin_spew(pnum) : 0;

	con_printf(CON_NORMAL, "coop_restore: sending inventory to P%d '%s' (src_level=%d cur_level=%d spew=%d shields=%d energy=%d laser=%d)\n",
	           pnum, rec.callsign, source_level, Current_level_num, removed,
	           f2i(rec.shields), f2i(rec.energy), rec.laser_level);

	multibuf[0] = MULTI_COOP_RESTORE_INV;
	multibuf[1] = (ubyte) pnum;
	PUT_INTEL_INT(multibuf + 2, rec.energy);
	PUT_INTEL_INT(multibuf + 6, rec.shields);
	PUT_INTEL_INT(multibuf + 10, rec.score);
	multibuf[14] = rec.laser_level;
	PUT_INTEL_SHORT(multibuf + 15, rec.primary_weapon_flags);
	PUT_INTEL_SHORT(multibuf + 17, rec.secondary_weapon_flags);
	for (i = 0; i < COOP_SAVE_MAX_WEAPONS; i++)
		PUT_INTEL_SHORT(multibuf + 19 + i * 2, rec.primary_ammo[i]);
	for (i = 0; i < COOP_SAVE_MAX_WEAPONS; i++)
		PUT_INTEL_SHORT(multibuf + 39 + i * 2, rec.secondary_ammo[i]);
	PUT_INTEL_INT(multibuf + 59, rec.flags);
	PUT_INTEL_SHORT(multibuf + 63, rec.net_kills_total);
	PUT_INTEL_SHORT(multibuf + 65, rec.net_killed_total);
	PUT_INTEL_SHORT(multibuf + 67, rec.num_kills_total);
	PUT_INTEL_SHORT(multibuf + 69, rec.hostages_rescued_total);
	PUT_INTEL_INT(multibuf + 71, rec.time_total);
	multibuf[75] = (ubyte) rec.hours_total;
	PUT_INTEL_SHORT(multibuf + 76, source_level);
	multibuf[78] = (ubyte) rec.primary_weapon;
	multibuf[79] = (ubyte) rec.secondary_weapon;
	PUT_INTEL_INT(multibuf + 80, rec.afterburner_charge);
	PUT_INTEL_SHORT(multibuf + 84, rec.kill_goal_count);

	multi_send_data_direct(multibuf, 86, pnum, 2);
}

void coop_do_restore_inventory(const ubyte *buf, int authenticated_sender)
{
	int pnum = buf[1];
	int i;
	coop_player_record rec;
	int16_t saved_level;

	if (pnum != Player_num)
		return;
	if (!(Game_mode & GM_MULTI_COOP))
		return;
	if (authenticated_sender >= 0 &&
	    authenticated_sender != multi_who_is_master())
		return;

	/* Unpack the network packet into a coop_player_record */
	memset(&rec, 0, sizeof(rec));
	rec.energy = GET_INTEL_INT(buf + 2);
	rec.shields = GET_INTEL_INT(buf + 6);
	rec.score = GET_INTEL_INT(buf + 10);
	rec.laser_level = buf[14];
	rec.primary_weapon_flags = GET_INTEL_SHORT(buf + 15);
	rec.secondary_weapon_flags = GET_INTEL_SHORT(buf + 17);
	for (i = 0; i < COOP_SAVE_MAX_WEAPONS; i++)
		rec.primary_ammo[i] = GET_INTEL_SHORT(buf + 19 + i * 2);
	for (i = 0; i < COOP_SAVE_MAX_WEAPONS; i++)
		rec.secondary_ammo[i] = GET_INTEL_SHORT(buf + 39 + i * 2);
	rec.flags = GET_INTEL_INT(buf + 59);
	rec.net_kills_total = GET_INTEL_SHORT(buf + 63);
	rec.net_killed_total = GET_INTEL_SHORT(buf + 65);
	rec.num_kills_total = GET_INTEL_SHORT(buf + 67);
	rec.hostages_rescued_total = GET_INTEL_SHORT(buf + 69);
	rec.time_total = GET_INTEL_INT(buf + 71);
	rec.hours_total = buf[75];

	saved_level = GET_INTEL_SHORT(buf + 76);
	rec.primary_weapon = (sbyte) buf[78];
	rec.secondary_weapon = (sbyte) buf[79];
	rec.afterburner_charge = GET_INTEL_INT(buf + 80);
	rec.kill_goal_count = GET_INTEL_SHORT(buf + 84);
	coop_apply_record_to_player(pnum, &rec, saved_level == Current_level_num);
}
#endif
