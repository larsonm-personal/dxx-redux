/*
 * D1 weapon behavior adapted from d1/main/laser.c
 * Shared projectile creation, visibility, physics and replay recording stay in the engine
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "inferno.h"
#include "object.h"
#include "weapon.h"
#include "laser.h"
#include "multi.h"
#include "robot.h"
#include "ai.h"
#include "segpoint.h"
#include "fvi.h"
#include "dxxerror.h"
#include "homing_compat.h"
#include "input_demo_hooks.h"
#include "game.h"
#include "hudmsg.h"
#include "kconfig.h"
#include "playsave.h"
#include "text.h"
#include "newmenu.h"
#include "physfsx.h"
#include "strutil.h"
#include "d1_in_d2.h"
#include "d1_in_d2_weapons.h"

enum { D1_QUAD_SELECTION = 16 };
extern int delayed_primary_autoselect_weapon_index;
int POrderList(int num);

static const ubyte d1_primary_order[] = { 4, 3, 2, 1, 0, 255, 16 };
static const ubyte d1_secondary_order[] = { 4, 3, 1, 0, 255, 2 };

void d1_in_d2_reset_weapon_order(void)
{
	memcpy(PlayerCfg.D1WeaponOrder.primary, d1_primary_order, sizeof(d1_primary_order));
	memcpy(PlayerCfg.D1WeaponOrder.secondary, d1_secondary_order, sizeof(d1_secondary_order));
}

const ubyte *d1_in_d2_weapon_order(int secondary)
{
	return secondary ? PlayerCfg.D1WeaponOrder.secondary : PlayerCfg.D1WeaponOrder.primary;
}

int d1_in_d2_set_weapon_order(int secondary, const ubyte *order, int count)
{
	const ubyte *domain = secondary ? d1_secondary_order : d1_primary_order;
	const int expected = secondary ? D1_IN_D2_SECONDARY_ORDER_COUNT : D1_IN_D2_PRIMARY_ORDER_COUNT;
	int i, j;
	unsigned seen = 0;
	if (!order || count != expected)
		return 0;
	for (i = 0; i < count; ++i) {
		for (j = 0; j < expected; ++j)
			if (order[i] == domain[j])
				break;
		if (j == expected || (seen & (1u << j)))
			return 0;
		seen |= 1u << j;
	}
	memcpy(secondary ? PlayerCfg.D1WeaponOrder.secondary : PlayerCfg.D1WeaponOrder.primary, order, count);
	return 1;
}

void d1_in_d2_read_weapon_order(PHYSFS_file *file)
{
	char line[64];
	while (!PHYSFS_eof(file) && PHYSFSX_fgets(line, sizeof(line), file)) {
		char *cursor, *end;
		ubyte order[D1_IN_D2_PRIMARY_ORDER_COUNT];
		int i, secondary, count;
		if (!d_strnicmp(line, "[end]", 5))
			break;
		if (!d_strnicmp(line, "primary=", 8)) {
			secondary = 0;
			cursor = line + 8;
		} else if (!d_strnicmp(line, "secondary=", 10)) {
			secondary = 1;
			cursor = line + 10;
		} else
			continue;
		count = secondary ? D1_IN_D2_SECONDARY_ORDER_COUNT : D1_IN_D2_PRIMARY_ORDER_COUNT;
		for (i = 0; i < count; ++i) {
			const unsigned long value = strtoul(cursor, &end, 0);
			if (cursor == end || value > 255 || (i + 1 < count && *end != ','))
				break;
			order[i] = (ubyte)value;
			cursor = end + (i + 1 < count);
		}
		while (isspace((unsigned char)*cursor)) ++cursor;
		if (i == count && !*cursor)
			d1_in_d2_set_weapon_order(secondary, order, count);
	}
}

void d1_in_d2_write_weapon_order(PHYSFS_file *file)
{
	int secondary, i;
	PHYSFSX_printf(file, "[d1 weapon order]\n");
	for (secondary = 0; secondary < 2; ++secondary) {
		const ubyte *order = d1_in_d2_weapon_order(secondary);
		const int count = secondary ? D1_IN_D2_SECONDARY_ORDER_COUNT : D1_IN_D2_PRIMARY_ORDER_COUNT;
		PHYSFSX_printf(file, "%s=", secondary ? "secondary" : "primary");
		for (i = 0; i < count; ++i)
			PHYSFSX_printf(file, "%s%u", i ? "," : "", (unsigned)order[i]);
		PHYSFSX_printf(file, "\n");
	}
	PHYSFSX_printf(file, "[end]\n");
}

int d1_in_d2_is_quad_selection(int weapon_index)
{
	return d1_in_d2_use_d1_gameplay() && weapon_index == D1_QUAD_SELECTION;
}

int d1_in_d2_primary_selection_index(int weapon_index)
{
	if (d1_in_d2_use_d1_gameplay() && weapon_index == LASER_INDEX &&
		(Players[Player_num].flags & PLAYER_FLAGS_QUAD_LASERS))
		return D1_QUAD_SELECTION;
	return weapon_index;
}

static int native_weapon_rank(int weapon_index, int secondary)
{
	int i;
	const ubyte *order = d1_in_d2_weapon_order(secondary);
	const int count = secondary ? D1_IN_D2_SECONDARY_ORDER_COUNT : D1_IN_D2_PRIMARY_ORDER_COUNT;
	for (i = 0; i < count; ++i)
		if (order[i] == weapon_index)
			return i;
	return -1;
}

int d1_in_d2_primary_order(int weapon_index)
{
	return d1_in_d2_use_d1_gameplay() ? native_weapon_rank(weapon_index, 0) : -1;
}

int d1_in_d2_secondary_order(int weapon_index)
{
	return d1_in_d2_use_d1_gameplay() ? native_weapon_rank(weapon_index, 1) : -1;
}

static int native_order_weapon_available(int weapon_index, int secondary)
{
	if (!secondary) {
		const int quads = (Players[Player_num].flags & PLAYER_FLAGS_QUAD_LASERS) != 0;
		if (weapon_index == D1_QUAD_SELECTION)
			return quads && player_has_weapon(Player_num, LASER_INDEX, 0) == HAS_ALL;
		if (weapon_index == LASER_INDEX && quads)
			return 0;
	}
	return weapon_index >= 0 && weapon_index < 5 &&
		player_has_weapon(Player_num, weapon_index, secondary) == HAS_ALL;
}

int d1_in_d2_weapon_availability(ubyte player_num, int weapon_index, int secondary)
{
	int info, result = 0;
	player *ship = &Players[player_num];
	if (!d1_in_d2_use_d1_gameplay())
		return -1;
	if (ship->energy < 0) ship->energy = 0;
	if (!secondary && weapon_index >= 5) {
		/* Native direct-selection aliases distinguish four laser levels,
		 * with and without quads, before looking up the laser inventory slot */
		const int variant = weapon_index - 5;
		if (variant < 8 && (ship->laser_level != variant % 4 ||
			((ship->flags & PLAYER_FLAGS_QUAD_LASERS) != 0) != (variant >= 4)))
			return 0;
		if (variant >= 8 && weapon_index != D1_QUAD_SELECTION)
			return 0;
		weapon_index = LASER_INDEX;
	}
	if (weapon_index < 0 || weapon_index >= 5)
		return 0;
	info = secondary ? Secondary_weapon_to_weapon_info[weapon_index] : Primary_weapon_to_weapon_info[weapon_index];
	if ((secondary ? ship->secondary_weapon_flags : ship->primary_weapon_flags) & (1 << weapon_index))
		result |= HAS_WEAPON_FLAG;
	if (Weapon_info[info].ammo_usage <= (secondary ? ship->secondary_ammo[weapon_index] : ship->primary_ammo[weapon_index]))
		result |= HAS_AMMO_FLAG;
	if ((!secondary && weapon_index == FUSION_INDEX ? F1_0 * 2 : Weapon_info[info].energy_usage) <= ship->energy)
		result |= HAS_ENERGY_FLAG;
	return result;
}

int d1_in_d2_cycle_weapon(int secondary)
{
	int slot, step;
	const ubyte *order;
	int count, cutoff, restricted;
	if (!d1_in_d2_use_d1_gameplay())
		return 0;
	order = d1_in_d2_weapon_order(secondary);
	count = secondary ? D1_IN_D2_SECONDARY_ORDER_COUNT : D1_IN_D2_PRIMARY_ORDER_COUNT;
	slot = native_weapon_rank(secondary ? Players[Player_num].secondary_weapon :
		d1_in_d2_primary_selection_index(Players[Player_num].primary_weapon), secondary);
	cutoff = native_weapon_rank(255, secondary);
	restricted = slot < cutoff && cutoff > 1 && PlayerCfg.CycleAutoselectOnly;
	for (step = 0; step < count; ++step) {
		slot = (slot + 1) % count;
		if (slot == cutoff) {
			if (restricted) slot = 0;
			else continue;
		}
		if (native_order_weapon_available(order[slot], secondary)) {
			/* Native cycling maps before select_weapon, including an already
			 * selected laser; queued/autoselection maps at the selection boundary */
			select_weapon(order[slot] == D1_QUAD_SELECTION ? LASER_INDEX : order[slot], secondary, 1, 1);
			break;
		}
	}
	return 1;
}

int d1_in_d2_auto_select_weapon(int secondary, int classic)
{
	const ubyte *order;
	int current, slot, cutoff, looped = 0;
	if (!d1_in_d2_use_d1_gameplay())
		return 0;
	current = secondary ? Players[Player_num].secondary_weapon : Players[Player_num].primary_weapon;
	if (player_has_weapon(Player_num, current, secondary) == HAS_ALL)
		return 1;
	order = d1_in_d2_weapon_order(secondary);
	cutoff = native_weapon_rank(255, secondary);
	if (classic) {
		slot = native_weapon_rank(secondary ? current : d1_in_d2_primary_selection_index(current), secondary);
		for (;;) {
			++slot;
			if (slot >= cutoff) {
				if (looped) break;
				slot = 0;
				looped = 1;
			}
			if (order[slot] == current) break;
			if (native_order_weapon_available(order[slot], secondary)) {
				select_weapon(order[slot], secondary, 1, 1);
				return 1;
			}
		}
	} else {
		for (slot = 0; slot < cutoff; ++slot) {
			if (secondary && order[slot] == PROXIMITY_INDEX) continue;
			if (native_order_weapon_available(order[slot], secondary)) {
				select_weapon(order[slot], secondary, 0, 1);
				return 1;
			}
		}
	}
	if (!secondary) select_weapon(LASER_INDEX, 0, 0, 1);
	HUD_init_message_literal(HM_DEFAULT, secondary ? "No secondary weapons available!" : "No primary weapons available!");
	return 1;
}

int d1_in_d2_reorder_weapons(int secondary)
{
	newmenu_item items[D1_IN_D2_PRIMARY_ORDER_COUNT];
	ubyte reordered[D1_IN_D2_PRIMARY_ORDER_COUNT];
	const ubyte *order;
	int count, i;
	if (!d1_in_d2_use_d1_gameplay())
		return 0;
	order = d1_in_d2_weapon_order(secondary);
	count = secondary ? D1_IN_D2_SECONDARY_ORDER_COUNT : D1_IN_D2_PRIMARY_ORDER_COUNT;
	memset(items, 0, sizeof(items));
	for (i = 0; i < count; ++i) {
		const int weapon = order[i];
		items[i].type = NM_TYPE_MENU;
		items[i].value = weapon;
		items[i].text = (char *)(weapon == 255 ? "--- Never Autoselect below ---" :
			weapon == D1_QUAD_SELECTION ? TXT_QUAD_LASERS :
			secondary ? SECONDARY_WEAPON_NAMES(weapon) : PRIMARY_WEAPON_NAMES(weapon));
	}
	newmenu_doreorder(secondary ? "Reorder Secondary" : "Reorder Primary",
#ifdef ANDROID
		"Long-press item or hold A to move",
#else
		"Shift+Up/Down arrow to move item",
#endif
		count, items, NULL, NULL);
	for (i = 0; i < count; ++i) reordered[i] = (ubyte)items[i].value;
	d1_in_d2_set_weapon_order(secondary, reordered, count);
	return 1;
}

/* Acquisition semantics from native D1 weapon.c, including the first-pickup
 * latch and the logical quad identity while a switch waits for firing release */
int d1_in_d2_pick_up_primary(int weapon_index, int is_quads)
{
	int primary_weapon_index, cutpoint, suppress_autoselect;
	unsigned flag;
	if (!d1_in_d2_use_d1_gameplay())
		return -1;
	flag = 1u << weapon_index;
	suppress_autoselect = PlayerCfg.AutoselectOnlyOnce && PrimaryWeaponPickedUp;
	if (weapon_index != LASER_INDEX && (Players[Player_num].primary_weapon_flags & flag)) {
		HUD_init_message(HM_DEFAULT | HM_REDUNDANT | HM_MAYDUPL, "%s %s!", TXT_ALREADY_HAVE_THE, PRIMARY_WEAPON_NAMES(weapon_index));
		return 0;
	}
	Players[Player_num].primary_weapon_flags |= flag;
	cutpoint = POrderList(255);
	if (is_quads)
		weapon_index = D1_QUAD_SELECTION;
	primary_weapon_index = d1_in_d2_primary_selection_index(Players[Player_num].primary_weapon);
	if (weapon_index != primary_weapon_index)
		PrimaryWeaponPickedUp = 1;
	if (!suppress_autoselect && POrderList(weapon_index) < cutpoint &&
		POrderList(weapon_index) < POrderList(primary_weapon_index)) {
		if (Controls.fire_primary_state) {
			if (PlayerCfg.SelectAfterFire) {
				if (delayed_primary_autoselect_weapon_index == -1 ||
					POrderList(weapon_index) < POrderList(delayed_primary_autoselect_weapon_index))
					delayed_primary_autoselect_weapon_index = weapon_index;
			} else if (!PlayerCfg.NoFireAutoselect)
				select_weapon(weapon_index, 0, 0, 1);
		} else
			select_weapon(weapon_index, 0, 0, 1);
	}
	if (!(Game_mode & GM_MULTI) || !Netgame.ReducedFlash)
		PALETTE_FLASH_ADD(7, 14, 21);
	if (weapon_index != LASER_INDEX)
		HUD_init_message(HM_DEFAULT, "%s!", is_quads ? TXT_QUAD_LASERS : PRIMARY_WEAPON_NAMES(weapon_index));
	/* The outer quad powerup path already sends native ship status */
	if (!is_quads && (Game_mode & GM_MULTI))
		multi_send_ship_status();
	return 1;
}

int d1_in_d2_laser_are_related(int first, int second)
{
	const object *a, *b;
	if (!d1_in_d2_use_d1_gameplay())
		return -1;
	if (first < 0 || second < 0)
		return 0;
	a = &Objects[first];
	b = &Objects[second];
	/* Preserve native D1's directional parent check and two-second mine grace */
	if (a->type == OBJ_WEAPON && a->ctype.laser_info.parent_num == second &&
		a->ctype.laser_info.parent_signature == b->signature)
		return a->id != PROXIMITY_ID || a->ctype.laser_info.creation_time + F1_0 * 2 >= GameTime64;
	if (b->type == OBJ_WEAPON && b->ctype.laser_info.parent_num == first &&
		b->ctype.laser_info.parent_signature == a->signature)
		return 1;
	if (a->type != OBJ_WEAPON || b->type != OBJ_WEAPON)
		return 0;
	/* Unlike D2, foreign projectiles participate in collision traversal even
	 * when the collision leaves both alive. Skipping it changes fixed-point travel */
	if (a->ctype.laser_info.parent_signature != b->ctype.laser_info.parent_signature)
		return 0;
	return !is_proximity_bomb_or_smart_mine(a->id) && !is_proximity_bomb_or_smart_mine(b->id);
}

int d1_in_d2_configure_homing(fix turn_time)
{
	if (!d1_in_d2_use_d1_gameplay())
		return 0;
	/* Native D1 uses this cone with both original and Redux steering */
	Min_acquirable_dot = HOMING_COMPAT_D1_ACQUISITION_DOT;
	Min_trackable_dot = homing_compat_d1_retention_dot(turn_time, Min_acquirable_dot);
	return 1;
}

static int target_hidden_from(const object *target, const object *tracker)
{
	if (target->type == OBJ_PLAYER)
		return (Players[target->id].flags & PLAYER_FLAGS_CLOAKED) != 0;
	if (target->type == OBJ_ROBOT)
		return target->ctype.ai_info.CLOAKED ||
			(Robot_info[target->id].companion && tracker->ctype.laser_info.parent_type == OBJ_PLAYER);
	return 0;
}

static int target_is_trackable(int target, object *tracker, fix *dot)
{
	vms_vector direction;
	if (target == -1 || (Game_mode & GM_MULTI_COOP))
		return 0;
	if ((target == Players[Player_num].objnum && (Players[Player_num].flags & PLAYER_FLAGS_CLOAKED)) ||
		(Objects[target].type == OBJ_ROBOT && target_hidden_from(&Objects[target], tracker)))
		return 0;
	vm_vec_sub(&direction, &Objects[target].pos, &tracker->pos);
	vm_vec_normalize_quick(&direction);
	*dot = vm_vec_dot(&direction, &tracker->orient.fvec);
	if (*dot < Min_trackable_dot && *dot > F1_0 * 9 / 10) {
		vm_vec_normalize(&direction);
		*dot = vm_vec_dot(&direction, &tracker->orient.fvec);
	}
	return *dot >= Min_trackable_dot && object_to_object_visibility(tracker, &Objects[target], FQ_TRANSWALL);
}

static void trace_target_choice(const char *phase, object *tracker, int target, fix dot)
{
	char probe[192];
	if (!input_demo_replay_homing_desync_probe_active())
		return;
	snprintf(probe, sizeof(probe), "frame=%u phase=%s result=%d dot=%d parent=%d",
		input_demo_trace_frame_index(), phase, target, dot, tracker->ctype.laser_info.parent_num);
	input_demo_append_replay_probe_message("d1_homing_target", tracker, probe);
}

int d1_in_d2_scan_homing_targets(vms_vector *position, object *tracker, int type1, int type2)
{
	int i, best = -1;
	fix best_dot = -2 * F1_0;
	if (!d1_in_d2_use_d1_gameplay())
		return D1_HOMING_NOT_HANDLED;
	if (!Weapon_info[tracker->id].homing_flag) {
		Int3();
		return 0;
	}
	for (i = 0; i <= Highest_object_index; ++i) {
		object *candidate = &Objects[i];
		vms_vector direction;
		fix distance, dot;
		/* Native D1 does not add proximity bombs to the requested object types */
		if ((candidate->type != type1 && candidate->type != type2) ||
			i == tracker->ctype.laser_info.parent_num ||
			target_hidden_from(candidate, tracker))
			continue;
#ifdef NETWORK
		if (candidate->type == OBJ_PLAYER && (Game_mode & GM_TEAM) &&
			Objects[tracker->ctype.laser_info.parent_num].type == OBJ_PLAYER &&
			get_team(candidate->id) == get_team(Objects[tracker->ctype.laser_info.parent_num].id))
			continue;
#endif
		vm_vec_sub(&direction, &candidate->pos, position);
		distance = vm_vec_mag(&direction);
		if (distance >= MAX_TRACKABLE_DIST)
			continue;
		vm_vec_normalize(&direction);
		dot = vm_vec_dot(&direction, &tracker->orient.fvec);
		if (dot > HOMING_COMPAT_D1_ACQUISITION_DOT && dot > best_dot &&
			object_to_object_visibility(tracker, candidate, FQ_TRANSWALL)) {
			best_dot = dot;
			best = i;
			trace_target_choice("scan_candidate", tracker, best, best_dot);
		}
	}
	trace_target_choice("scan_result", tracker, best, best_dot);
	return best;
}

int d1_in_d2_acquire_homing_target(vms_vector *position, object *tracker)
{
	int i, best = -1;
	fix best_dot = -2 * F1_0;
	if (!d1_in_d2_use_d1_gameplay())
		return D1_HOMING_NOT_HANDLED;
	if (Game_mode & GM_MULTI) {
		if (tracker->ctype.laser_info.parent_type == OBJ_PLAYER)
			return d1_in_d2_scan_homing_targets(position, tracker,
				(Game_mode & GM_MULTI_COOP) ? OBJ_ROBOT : OBJ_PLAYER,
				(Game_mode & GM_MULTI_COOP) ? -1 : OBJ_ROBOT);
		return d1_in_d2_scan_homing_targets(position, tracker, OBJ_PLAYER, -1);
	}
	if (tracker->ctype.laser_info.parent_num != Players[Player_num].objnum)
		return (Players[Player_num].flags & PLAYER_FLAGS_CLOAKED) ? -1 : (int)(ConsoleObject - Objects);
	/* Native D1 consumes the last main-view list, including rear/external views
	 * Window zero is the main view; HUD cameras and elapsed wall time cannot
	 * replace its candidates with another view or a complete-object scan */
	for (i = Window_rendered_data[0].num_objects - 1; i >= 0; --i) {
		const int target = Window_rendered_data[0].rendered_objects[i];
		vms_vector direction;
		fix dot;
		if (target == Players[Player_num].objnum ||
			(Objects[target].type == OBJ_ROBOT && target_hidden_from(&Objects[target], tracker)))
			continue;
		vm_vec_sub(&direction, &Objects[target].pos, position);
		vm_vec_normalize_quick(&direction);
		dot = vm_vec_dot(&direction, &tracker->orient.fvec);
		/* The original rendered-list acquisition has no complete-scan distance cap */
		if (dot > HOMING_COMPAT_D1_ACQUISITION_DOT && dot > best_dot &&
			object_to_object_visibility(tracker, &Objects[target], FQ_TRANSWALL)) {
			best = target;
			best_dot = dot;
		}
	}
	trace_target_choice("rendered_result", tracker, best, best_dot);
	return best;
}

int d1_in_d2_track_homing_target(int target, object *tracker, fix *dot, unsigned frame, int original_homing)
{
	unsigned phase;
	int type1, type2 = -1;
	if (!d1_in_d2_use_d1_gameplay())
		return D1_HOMING_NOT_HANDLED;
	if (target_is_trackable(target, tracker, dot))
		return target;
	phase = original_homing ? (unsigned)((tracker - Objects) ^ frame) :
		frame - tracker->ctype.laser_info.creation_framecount;
	if ((phase % 4) != 0)
		return -1;
	if (Objects[tracker->ctype.laser_info.parent_num].type == OBJ_PLAYER) {
		if (target == -1) {
			if (Game_mode & GM_MULTI_COOP)
				type1 = OBJ_ROBOT;
			else {
				type1 = OBJ_PLAYER;
				if (!(Game_mode & GM_MULTI) || (Game_mode & GM_MULTI_ROBOTS))
					type2 = OBJ_ROBOT;
			}
		} else {
			type1 = Objects[tracker->ctype.laser_info.track_goal].type;
			if (type1 != OBJ_PLAYER && type1 != OBJ_ROBOT)
				return -1;
		}
	} else
		type1 = target == -1 ? OBJ_PLAYER : Objects[tracker->ctype.laser_info.track_goal].type;
	return d1_in_d2_scan_homing_targets(&tracker->pos, tracker, type1, type2);
}

int d1_in_d2_primary_projectile(int default_projectile)
{
	/* D1 record 12 supplies firing costs/timing; record 20 is the visible shot */
	if (d1_in_d2_has_native_assets() && default_projectile == SPREADFIRE_ID)
		return 20;
	return default_projectile;
}

int d1_in_d2_initialize_player_weapon(object *weapon, fix fusion_charge, int game_mode)
{
	int fusion_scale;

	if (!d1_in_d2_use_d1_gameplay())
		return 0;

	/* D1 quad lasers keep full per-bolt damage */
	weapon->ctype.laser_info.multiplier = F1_0;
	if (weapon->id != FUSION_ID)
		return 1;

	fusion_scale = (game_mode & GM_MULTI) ? 2 : 4;
	if (fusion_charge > 0) {
		if (fusion_charge <= F1_0 * fusion_scale)
			weapon->ctype.laser_info.multiplier = F1_0 + fusion_charge / 2;
		else
			weapon->ctype.laser_info.multiplier = F1_0 * fusion_scale;
	}
	if (game_mode & GM_MULTI)
		weapon->ctype.laser_info.multiplier /= 2;
	return 1;
}

int d1_in_d2_position_weapon(object *weapon, const object *parent, const vms_vector *direction, fix length)
{
	vms_vector end;
	int segment;
	if (!d1_in_d2_use_d1_gameplay())
		return 0;
	if (parent->type == OBJ_WEAPON || Weapon_info[weapon->id].render_type == WEAPON_RENDER_NONE || weapon->id == FLARE_ID)
		return 1;
	/* D1 advances robot/reactor shots as well as player shots to the gun tip */
	vm_vec_scale_add(&end, &weapon->pos, direction, length / 2);
	segment = find_point_seg(&end, weapon->segnum);
	if (segment == weapon->segnum)
		weapon->pos = end;
	else if (segment != -1) {
		weapon->pos = end;
		obj_relink(weapon - Objects, segment);
	}
	return 1;
}

void d1_in_d2_notify_player_fire(object *player, int weapon_type)
{
	/* D1 generates awareness at impacts, not at this D2 firing phase */
	if (d1_in_d2_use_d1_gameplay())
		return;
	input_demo_set_awareness_source("laser_player_fire", player - Objects, weapon_type);
	create_awareness_event(player, PA_WEAPON_WALL_COLLISION);
}

int d1_in_d2_limit_weapon_speed(object *weapon)
{
	fix speed, maximum;
	if (!d1_in_d2_use_d1_gameplay())
		return 0;
	if (!Weapon_info[weapon->id].thrust)
		return 1;
	maximum = Weapon_info[weapon->id].speed[Difficulty_level];
	if ((Game_mode & GM_MULTI) && Netgame.OriginalD1Weapons && weapon->id == SPREADFIRE_ID)
		maximum = 200 * F1_0;
	speed = vm_vec_mag_quick(&weapon->mtype.phys_info.velocity);
	if (speed > maximum && maximum)
		vm_vec_scale(&weapon->mtype.phys_info.velocity, fixdiv(maximum, speed));
	return 1;
}

int d1_in_d2_weapon_wall_impact_allowed(const object *weapon)
{
	return !d1_in_d2_use_d1_gameplay() || !(weapon->mtype.phys_info.flags & PF_BOUNCE);
}

int d1_in_d2_smart_child(const object *parent, int default_child)
{
	if (d1_in_d2_use_d1_gameplay() && parent->type == OBJ_WEAPON && parent->id == SMART_ID) {
		/* Earlier D1 editions have no separate, easier-to-dodge robot child */
		if (parent->ctype.laser_info.parent_type == OBJ_ROBOT && N_weapon_types > ROBOT_SMART_HOMING_ID)
			return ROBOT_SMART_HOMING_ID;
		return PLAYER_SMART_HOMING_ID;
	}
	return default_child;
}

int d1_in_d2_turn_homing_weapon(object *weapon, int target, fix *dot, fix turn_time, int original_homing)
{
	vms_vector to_target, velocity;
	fix speed, max_speed, absdot;

	if (!d1_in_d2_use_d1_gameplay())
		return 0;

	vm_vec_sub(&to_target, &Objects[target].pos, &weapon->pos);
	vm_vec_normalize_quick(&to_target);
	velocity = weapon->mtype.phys_info.velocity;
	speed = vm_vec_normalize_quick(&velocity);
	if (original_homing)
		*dot = vm_vec_dot(&velocity, &to_target);
	max_speed = Weapon_info[weapon->id].speed[Difficulty_level];
	if ((Game_mode & GM_MULTI) && Netgame.OriginalD1Weapons && weapon->id == SPREADFIRE_ID)
		max_speed = 200 * F1_0;
	if (speed + F1_0 < max_speed) {
		speed += fixmul(max_speed, turn_time / 2);
		if (speed > max_speed)
			speed = max_speed;
	}

	vm_vec_add2(&velocity, &to_target);
	/* The boss' smart children track better */
	if (Weapon_info[weapon->id].render_type != WEAPON_RENDER_POLYMODEL)
		vm_vec_add2(&velocity, &to_target);
	vm_vec_normalize_quick(&velocity);
	vm_vec_scale(&velocity, speed);
	weapon->mtype.phys_info.velocity = velocity;

	absdot = abs(F1_0 - *dot);
	if (original_homing) {
		if (absdot > F1_0 / 8) {
			if (absdot > F1_0 / 4)
				absdot = F1_0 / 4;
			weapon->lifeleft -= fixmul(absdot * 16, turn_time);
		}
	} else
		weapon->lifeleft -= fixmul(absdot * 32, turn_time);

	if (Weapon_info[weapon->id].render_type == WEAPON_RENDER_POLYMODEL) {
		vms_vector forward = velocity;
		/* Preserve the existing D1 steering phase's fixed-point evaluation order */
		vm_vec_scale(&forward, (original_homing ? turn_time : FrameTime) * 8);
		vm_vec_add2(&forward, &weapon->orient.fvec);
		vm_vec_normalize_quick(&forward);
		vm_vector_2_matrix(&weapon->orient, &forward, NULL, NULL);
	}
	return 1;
}
