#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ai.h"
#include "cntrlcen.h"
#include "console.h"
#include "endlevel.h"
#include "game.h"
#include "gameseq.h"
#include "input_demo_debug_logging.h"
#include "input_demo_hooks.h"
#include "input_demo_recorder.h"
#include "input_demo_replay.h"
#include "input_demo_result.h"
#include "input_demo_state_trace.h"
#include "laser.h"
#include "maths.h"
#include "mission.h"
#include "newdemo.h"
#include "object.h"
#include "player.h"

#include "input_demo_hooks_shared.h"

#ifdef DXX_BUILD_DESCENT_II
extern void input_demo_capture_current_result_prep_d2(player *current_player);
extern void input_demo_capture_current_result_after_game_time_d2(
    input_demo_result *result);
extern int input_demo_capture_current_result_robots_killed_d2(
    player *current_player);
extern void input_demo_capture_current_result_after_powerups_d2(
    input_demo_result *result);
#endif

static int input_demo_collision_trace_enabled(void)
{
#ifdef DXX_BUILD_DESCENT_II
	return input_demo_debug_is_enabled();
#else
	return 1;
#endif
}

static const char *input_demo_result_game_name(void)
{
#ifdef DXX_BUILD_DESCENT_II
	return "d2";
#else
	return "d1";
#endif
}

static const char *input_demo_result_mission_id(void)
{
#ifdef DXX_BUILD_DESCENT_II
	return (Current_mission && Current_mission_filename) ? Current_mission_filename : "";
#else
	return input_demo_current_mission_id();
#endif
}

static void input_demo_prepare_current_result(player *current_player)
{
#ifdef DXX_BUILD_DESCENT_II
	input_demo_capture_current_result_prep_d2(current_player);
#else
	(void) current_player;
#endif
}

static void input_demo_finish_current_result_game_time(input_demo_result *result)
{
#ifdef DXX_BUILD_DESCENT_II
	input_demo_capture_current_result_after_game_time_d2(result);
#else
	(void) result;
#endif
}

static int input_demo_current_result_robots_killed(player *current_player)
{
#ifdef DXX_BUILD_DESCENT_II
	return input_demo_capture_current_result_robots_killed_d2(current_player);
#else
	return current_player->num_robots_level;
#endif
}

static void input_demo_finish_current_result_powerups(input_demo_result *result)
{
#ifdef DXX_BUILD_DESCENT_II
	input_demo_capture_current_result_after_powerups_d2(result);
#else
	(void) result;
#endif
}

static int input_demo_robot_is_camera_awake(int objnum, const object *obj)
{
#ifdef DXX_BUILD_DESCENT_II
	return ((obj->ctype.ai_info.SUB_FLAGS & SUB_FLAGS_CAMERA_AWAKE) != 0);
#else
	return (Ai_local_info[objnum].player_awareness_type > 0);
#endif
}

static int input_demo_count_live_objects_of_type(int object_type)
{
	int count = 0;
	int i;

	for (i = 0; i <= Highest_object_index; ++i) {
		if (Objects[i].type != object_type)
			continue;
		if (Objects[i].flags & OF_SHOULD_BE_DEAD)
			continue;
		count++;
	}
	return count;
}

static unsigned int input_demo_state_trace_hash_update(unsigned int hash,
                                                       unsigned int value)
{
	hash ^= value + 0x9e3779b9u + (hash << 6) + (hash >> 2);
	return hash;
}

static unsigned int input_demo_state_trace_hash_i64(unsigned int hash,
                                                    int64_t value)
{
	uint64_t bits = (uint64_t) value;

	hash = input_demo_state_trace_hash_update(hash, (unsigned int) bits);
	hash = input_demo_state_trace_hash_update(hash, (unsigned int) (bits >> 32));
	return hash;
}

void input_demo_capture_runtime_state_diag(input_demo_state_trace_diag *diag)
{
	object_runtime_state object_state;
	laser_runtime_state laser_state;
	unsigned int runtime_hash = 0;
	int free_start;

	if (!diag)
		return;

	object_get_runtime_state(&object_state);
	laser_get_runtime_state(&laser_state);
	free_start = object_state.num_objects;
	diag->object_allocator_num_objects = object_state.num_objects;
	diag->object_signature_seed = object_state.signature_seed;
	diag->object_free_head0 = -1;
	diag->object_free_head1 = -1;
	diag->object_free_head2 = -1;
	diag->object_free_head3 = -1;
	diag->object_homer_frame_count = object_state.homer_frame_count;
	diag->object_current_homer_frame_time = object_state.current_homer_frame_time;
	diag->object_do_homer_frame = object_state.do_homer_frame;

	if (free_start >= 0 && free_start <= MAX_OBJECTS) {
		int free_index;

		diag->object_free_list_count = MAX_OBJECTS - free_start;
		for (free_index = free_start; free_index < MAX_OBJECTS; ++free_index) {
			const int head_index = free_index - free_start;
			const short free_slot = object_state.free_obj_list[free_index];

			diag->object_free_list_hash = input_demo_state_trace_hash_update(
			    diag->object_free_list_hash, (unsigned int) (unsigned short) free_slot);
			if (head_index == 0)
				diag->object_free_head0 = free_slot;
			else if (head_index == 1)
				diag->object_free_head1 = free_slot;
			else if (head_index == 2)
				diag->object_free_head2 = free_slot;
			else if (head_index == 3)
				diag->object_free_head3 = free_slot;
		}
	} else
		diag->object_free_list_count = -1;

	diag->weapon_next_laser_delta = (int64_t) (Next_laser_fire_time - GameTime64);
	diag->weapon_next_missile_delta = (int64_t) (Next_missile_fire_time - GameTime64);
	diag->weapon_last_laser_delta = (int64_t) (Last_laser_fired_time - GameTime64);
	diag->weapon_next_flare_delta = (int64_t) (Next_flare_fire_time - GameTime64);
	diag->weapon_auto_fusion_delta = (int64_t) (Auto_fire_fusion_cannon_time - GameTime64);
	diag->weapon_last_omega_delta = (int64_t) laser_state.last_omega_fire_time - GameTime64;
	diag->weapon_global_laser_firing_count = Global_laser_firing_count;
	diag->weapon_global_missile_firing_count = Global_missile_firing_count;
	diag->weapon_fusion_charge = laser_state.fusion_charge;
	diag->weapon_spreadfire_toggle = laser_state.spreadfire_toggle;
	diag->weapon_missile_gun = laser_state.missile_gun;
	diag->weapon_proximity_dropped = laser_state.proximity_dropped;
	diag->weapon_helix_orientation = laser_state.helix_orientation;
	diag->weapon_smartmines_dropped = laser_state.smartmines_dropped;

	runtime_hash = input_demo_state_trace_hash_update(runtime_hash,
	                                                  (unsigned int) diag->object_allocator_num_objects);
	runtime_hash = input_demo_state_trace_hash_update(runtime_hash,
	                                                  (unsigned int) object_state.highest_object_index);
	runtime_hash = input_demo_state_trace_hash_update(runtime_hash,
	                                                  (unsigned int) diag->object_signature_seed);
	runtime_hash = input_demo_state_trace_hash_update(runtime_hash,
	                                                  (unsigned int) diag->object_free_list_count);
	runtime_hash = input_demo_state_trace_hash_update(runtime_hash,
	                                                  diag->object_free_list_hash);
	runtime_hash = input_demo_state_trace_hash_update(runtime_hash,
	                                                  diag->object_homer_frame_count);
	runtime_hash = input_demo_state_trace_hash_update(runtime_hash,
	                                                  (unsigned int) diag->object_current_homer_frame_time);
	runtime_hash = input_demo_state_trace_hash_update(runtime_hash,
	                                                  (unsigned int) diag->object_do_homer_frame);
	runtime_hash = input_demo_state_trace_hash_i64(runtime_hash,
	                                               diag->weapon_next_laser_delta);
	runtime_hash = input_demo_state_trace_hash_i64(runtime_hash,
	                                               diag->weapon_next_missile_delta);
	runtime_hash = input_demo_state_trace_hash_i64(runtime_hash,
	                                               diag->weapon_last_laser_delta);
	runtime_hash = input_demo_state_trace_hash_i64(runtime_hash,
	                                               diag->weapon_next_flare_delta);
	runtime_hash = input_demo_state_trace_hash_i64(runtime_hash,
	                                               diag->weapon_auto_fusion_delta);
	runtime_hash = input_demo_state_trace_hash_i64(runtime_hash,
	                                               diag->weapon_last_omega_delta);
	runtime_hash = input_demo_state_trace_hash_update(runtime_hash,
	                                                  (unsigned int) diag->weapon_global_laser_firing_count);
	runtime_hash = input_demo_state_trace_hash_update(runtime_hash,
	                                                  (unsigned int) diag->weapon_global_missile_firing_count);
	runtime_hash = input_demo_state_trace_hash_update(runtime_hash,
	                                                  (unsigned int) diag->weapon_fusion_charge);
	runtime_hash = input_demo_state_trace_hash_update(runtime_hash,
	                                                  (unsigned int) diag->weapon_spreadfire_toggle);
	runtime_hash = input_demo_state_trace_hash_update(runtime_hash,
	                                                  (unsigned int) diag->weapon_missile_gun);
	runtime_hash = input_demo_state_trace_hash_update(runtime_hash,
	                                                  (unsigned int) diag->weapon_proximity_dropped);
	runtime_hash = input_demo_state_trace_hash_update(runtime_hash,
	                                                  (unsigned int) diag->weapon_helix_orientation);
	runtime_hash = input_demo_state_trace_hash_update(runtime_hash,
	                                                  (unsigned int) diag->weapon_smartmines_dropped);
	diag->runtime_state_hash = runtime_hash;
}

static void input_demo_set_player_weapon_diag_slot(input_demo_state_trace_diag *diag,
                                                   int slot, int objnum, const object *obj)
{
	if (!diag || !obj)
		return;

	switch (slot) {
		case 0:
			diag->player_weapon_obj0 = objnum;
			diag->player_weapon_sig0 = obj->signature;
			diag->player_weapon_id0 = obj->id;
			break;
		case 1:
			diag->player_weapon_obj1 = objnum;
			diag->player_weapon_sig1 = obj->signature;
			diag->player_weapon_id1 = obj->id;
			break;
		case 2:
			diag->player_weapon_obj2 = objnum;
			diag->player_weapon_sig2 = obj->signature;
			diag->player_weapon_id2 = obj->id;
			break;
		case 3:
			diag->player_weapon_obj3 = objnum;
			diag->player_weapon_sig3 = obj->signature;
			diag->player_weapon_id3 = obj->id;
			break;
	}
}

void input_demo_capture_player_weapon_diag(input_demo_state_trace_diag *diag)
{
	int i;
	int player_objnum;

	if (!diag)
		return;

	diag->player_weapon_obj0 = -1;
	diag->player_weapon_sig0 = -1;
	diag->player_weapon_id0 = -1;
	diag->player_weapon_obj1 = -1;
	diag->player_weapon_sig1 = -1;
	diag->player_weapon_id1 = -1;
	diag->player_weapon_obj2 = -1;
	diag->player_weapon_sig2 = -1;
	diag->player_weapon_id2 = -1;
	diag->player_weapon_obj3 = -1;
	diag->player_weapon_sig3 = -1;
	diag->player_weapon_id3 = -1;
	player_objnum = Players[Player_num].objnum;

	for (i = 0; i <= Highest_object_index; ++i) {
		object *obj = &Objects[i];

		if (obj->type != OBJ_WEAPON)
			continue;
		if (obj->flags & OF_SHOULD_BE_DEAD)
			continue;
		if (obj->ctype.laser_info.parent_type != OBJ_PLAYER)
			continue;
		if (obj->ctype.laser_info.parent_num != player_objnum)
			continue;

		diag->player_weapon_hash = input_demo_state_trace_hash_update(
		    diag->player_weapon_hash, (unsigned int) i);
		diag->player_weapon_hash = input_demo_state_trace_hash_update(
		    diag->player_weapon_hash, (unsigned int) obj->signature);
		diag->player_weapon_hash = input_demo_state_trace_hash_update(
		    diag->player_weapon_hash, (unsigned int) obj->id);
		diag->player_weapon_hash = input_demo_state_trace_hash_update(
		    diag->player_weapon_hash, (unsigned int) obj->segnum);
		diag->player_weapon_hash = input_demo_state_trace_hash_update(
		    diag->player_weapon_hash,
		    (unsigned int) obj->ctype.laser_info.parent_signature);
		if (diag->player_weapon_count < 4)
			input_demo_set_player_weapon_diag_slot(diag,
			                                       diag->player_weapon_count, i, obj);
		diag->player_weapon_count++;
	}
}

static unsigned int input_demo_state_trace_hash_object(unsigned int hash,
                                                       const object *obj)
{
	if (!obj)
		return hash;

	hash = input_demo_state_trace_hash_update(hash, (unsigned int) obj->signature);
	hash = input_demo_state_trace_hash_update(hash, (unsigned int) obj->type);
	hash = input_demo_state_trace_hash_update(hash, (unsigned int) obj->id);
	hash = input_demo_state_trace_hash_update(hash, (unsigned int) obj->segnum);
	hash = input_demo_state_trace_hash_update(hash, (unsigned int) obj->control_type);
	hash = input_demo_state_trace_hash_update(hash, (unsigned int) obj->movement_type);
	hash = input_demo_state_trace_hash_update(hash, (unsigned int) obj->render_type);
	hash = input_demo_state_trace_hash_update(hash, (unsigned int) obj->flags);
	hash = input_demo_state_trace_hash_update(hash, (unsigned int) obj->size);
	hash = input_demo_state_trace_hash_update(hash, (unsigned int) obj->shields);
	hash = input_demo_state_trace_hash_update(hash, (unsigned int) obj->lifeleft);
	hash = input_demo_state_trace_hash_update(hash, (unsigned int) obj->attached_obj);
	hash = input_demo_state_trace_hash_update(hash, (unsigned int) obj->pos.x);
	hash = input_demo_state_trace_hash_update(hash, (unsigned int) obj->pos.y);
	hash = input_demo_state_trace_hash_update(hash, (unsigned int) obj->pos.z);
	hash = input_demo_state_trace_hash_update(hash, (unsigned int) obj->last_pos.x);
	hash = input_demo_state_trace_hash_update(hash, (unsigned int) obj->last_pos.y);
	hash = input_demo_state_trace_hash_update(hash, (unsigned int) obj->last_pos.z);

	if (obj->movement_type == MT_PHYSICS) {
		hash = input_demo_state_trace_hash_update(hash,
		                                          (unsigned int) obj->mtype.phys_info.velocity.x);
		hash = input_demo_state_trace_hash_update(hash,
		                                          (unsigned int) obj->mtype.phys_info.velocity.y);
		hash = input_demo_state_trace_hash_update(hash,
		                                          (unsigned int) obj->mtype.phys_info.velocity.z);
		hash = input_demo_state_trace_hash_update(hash,
		                                          (unsigned int) obj->mtype.phys_info.rotvel.x);
		hash = input_demo_state_trace_hash_update(hash,
		                                          (unsigned int) obj->mtype.phys_info.rotvel.y);
		hash = input_demo_state_trace_hash_update(hash,
		                                          (unsigned int) obj->mtype.phys_info.rotvel.z);
		hash = input_demo_state_trace_hash_update(hash,
		                                          (unsigned int) obj->mtype.phys_info.mass);
		hash = input_demo_state_trace_hash_update(hash,
		                                          (unsigned int) obj->mtype.phys_info.drag);
		hash = input_demo_state_trace_hash_update(hash,
		                                          (unsigned int) obj->mtype.phys_info.brakes);
		hash = input_demo_state_trace_hash_update(hash,
		                                          (unsigned int) obj->mtype.phys_info.turnroll);
		hash = input_demo_state_trace_hash_update(hash,
		                                          (unsigned int) obj->mtype.phys_info.flags);
	}

	if (obj->type == OBJ_WEAPON) {
		hash = input_demo_state_trace_hash_update(hash,
		                                          (unsigned int) obj->ctype.laser_info.parent_type);
		hash = input_demo_state_trace_hash_update(hash,
		                                          (unsigned int) obj->ctype.laser_info.parent_num);
		hash = input_demo_state_trace_hash_update(hash,
		                                          (unsigned int) obj->ctype.laser_info.parent_signature);
	}

	if (obj->render_type == RT_POLYOBJ) {
		hash = input_demo_state_trace_hash_update(hash,
		                                          (unsigned int) obj->rtype.pobj_info.model_num);
		hash = input_demo_state_trace_hash_update(hash,
		                                          (unsigned int) obj->rtype.pobj_info.subobj_flags);
		hash = input_demo_state_trace_hash_update(hash,
		                                          (unsigned int) obj->rtype.pobj_info.tmap_override);
	}

	return hash;
}

void input_demo_capture_object_state_diag(input_demo_state_trace_diag *diag)
{
	int i;
	int segnum;

	if (!diag)
		return;
	diag->highest_object_index = Highest_object_index;
	diag->object_slot_bucket_size = INPUT_DEMO_OBJECT_SLOT_BUCKET_SIZE;

	for (i = 0; i <= Highest_object_index; ++i) {
		object *obj = &Objects[i];
		int bucket;

		if (obj->type == OBJ_NONE)
			continue;
		if (obj->flags & OF_SHOULD_BE_DEAD)
			continue;

		bucket = i >> INPUT_DEMO_OBJECT_SLOT_BUCKET_BITS;
		if (bucket >= 0 && bucket < INPUT_DEMO_OBJECT_SLOT_BUCKET_COUNT) {
			diag->object_slot_counts[bucket]++;
			diag->object_slot_hashes[bucket] = input_demo_state_trace_hash_update(
			    diag->object_slot_hashes[bucket], (unsigned int) i);
			diag->object_slot_hashes[bucket] = input_demo_state_trace_hash_object(
			    diag->object_slot_hashes[bucket], obj);
		}

		diag->live_object_count++;
		diag->live_object_hash = input_demo_state_trace_hash_object(
		    diag->live_object_hash, obj);

		switch (obj->type) {
			case OBJ_ROBOT:
				diag->robot_object_count++;
				diag->robot_state_hash = input_demo_state_trace_hash_object(
				    diag->robot_state_hash, obj);
				if (input_demo_robot_is_camera_awake(i, obj))
					diag->camera_awake_robots++;
				if (obj->ctype.ai_info.danger_laser_num != -1)
					diag->danger_laser_robots++;
				break;
			case OBJ_WEAPON:
				diag->weapon_object_count++;
				diag->weapon_state_hash = input_demo_state_trace_hash_object(
				    diag->weapon_state_hash, obj);
				break;
			case OBJ_FIREBALL:
				diag->fireball_object_count++;
				diag->fireball_state_hash = input_demo_state_trace_hash_object(
				    diag->fireball_state_hash, obj);
				break;
			case OBJ_DEBRIS:
				diag->debris_object_count++;
				diag->debris_state_hash = input_demo_state_trace_hash_object(
				    diag->debris_state_hash, obj);
				break;
		}
	}

	if (Highest_segment_index < 0)
		return;
	diag->segment_object_list_hash = input_demo_state_trace_hash_update(
	    diag->segment_object_list_hash, (unsigned int) Highest_segment_index);
	for (segnum = 0; segnum <= Highest_segment_index; ++segnum) {
		int objnum = Segments[segnum].objects;
		int guard = 0;
		int segment_count = 0;
		unsigned int segment_hash = 0;

		while (objnum != -1) {
			object *obj;

			if (objnum < 0 || objnum > Highest_object_index) {
				diag->segment_object_link_error_count++;
				break;
			}
			if (++guard > MAX_OBJECTS) {
				diag->segment_object_link_error_count++;
				break;
			}
			obj = &Objects[objnum];
			if (obj->type == OBJ_NONE || obj->segnum != segnum)
				diag->segment_object_link_error_count++;
			segment_count++;
			segment_hash = input_demo_state_trace_hash_update(
			    segment_hash, (unsigned int) objnum);
			segment_hash = input_demo_state_trace_hash_update(
			    segment_hash, (unsigned int) obj->signature);
			segment_hash = input_demo_state_trace_hash_update(
			    segment_hash, (unsigned int) obj->type);
			segment_hash = input_demo_state_trace_hash_update(
			    segment_hash, (unsigned int) obj->id);
			segment_hash = input_demo_state_trace_hash_update(
			    segment_hash, (unsigned int) obj->flags);
			segment_hash = input_demo_state_trace_hash_update(
			    segment_hash, (unsigned int) obj->prev);
			segment_hash = input_demo_state_trace_hash_update(
			    segment_hash, (unsigned int) obj->next);
			objnum = obj->next;
		}
		if (!segment_count)
			continue;
		diag->segment_object_list_count += segment_count;
		diag->segment_object_list_hash = input_demo_state_trace_hash_update(
		    diag->segment_object_list_hash, (unsigned int) segnum);
		diag->segment_object_list_hash = input_demo_state_trace_hash_update(
		    diag->segment_object_list_hash, (unsigned int) segment_count);
		diag->segment_object_list_hash = input_demo_state_trace_hash_update(
		    diag->segment_object_list_hash, segment_hash);
	}
}

void input_demo_capture_current_result(input_demo_result *result)
{
	player *current_player = &Players[Player_num];
	int robots_killed;
	int i;

	input_demo_prepare_current_result(current_player);
	input_demo_result_clear(result);
	snprintf(result->game, sizeof(result->game), "%s",
	         input_demo_result_game_name());
	snprintf(result->mission, sizeof(result->mission), "%s",
	         input_demo_result_mission_id());
	result->level = Current_level_num;
	result->difficulty = Difficulty_level;
	if (input_demo_replay_is_loaded())
		result->frame_count = (uint32_t) input_demo_replay_next_frame_index();
	else if (input_demo_recorder_is_active())
		result->frame_count = (uint32_t) input_demo_recorder_frame_count();
	else
		result->frame_count = 0;
	result->has_game_time64 = 1;
	result->game_time64 = GameTime64;
	input_demo_finish_current_result_game_time(result);

	result->player0.present = 1;
	result->player0.energy = f2i(current_player->energy);
	result->player0.shields = f2i(current_player->shields);
	result->player0.score = current_player->score;
	result->player0.lives = current_player->lives;
	result->player0.laser_level = current_player->laser_level;
	result->player0.primary_weapon = current_player->primary_weapon;
	result->player0.secondary_weapon = current_player->secondary_weapon;
	result->player0.flags = current_player->flags;
	result->player0.hostages = current_player->hostages_on_board;
	for (i = 0; i < INPUT_DEMO_RESULT_MAX_PRIMARY_AMMO; ++i)
		result->player0.primary_ammo[i] =
		    i < MAX_PRIMARY_WEAPONS ? current_player->primary_ammo[i] : 0;
	for (i = 0; i < INPUT_DEMO_RESULT_MAX_SECONDARY_AMMO; ++i)
		result->player0.secondary_ammo[i] =
		    i < MAX_SECONDARY_WEAPONS ? current_player->secondary_ammo[i] : 0;

	if (ConsoleObject) {
		result->position.present = 1;
		result->position.segment = ConsoleObject->segnum;
		result->position.x = ConsoleObject->pos.x;
		result->position.y = ConsoleObject->pos.y;
		result->position.z = ConsoleObject->pos.z;
		result->position.has_forward = 1;
		result->position.fx = ConsoleObject->orient.fvec.x;
		result->position.fy = ConsoleObject->orient.fvec.y;
		result->position.fz = ConsoleObject->orient.fvec.z;
	}

	result->level_summary.present = 1;
	result->level_summary.robots_alive =
	    input_demo_count_live_objects_of_type(OBJ_ROBOT);
	robots_killed = input_demo_current_result_robots_killed(current_player);
	if (robots_killed < 0)
		robots_killed = 0;
	result->level_summary.robots_killed = robots_killed;
	result->level_summary.hostages_remaining =
	    input_demo_count_live_objects_of_type(OBJ_HOSTAGE);
	result->level_summary.powerups_remaining =
	    input_demo_count_live_objects_of_type(OBJ_POWERUP);
	input_demo_finish_current_result_powerups(result);
	result->level_summary.control_center_destroyed =
	    Control_center_destroyed ? 1 : 0;
	result->level_summary.endlevel_completed = Endlevel_sequence ? 1 : 0;
}

int input_demo_trace_collision_pose_active(void)
{
	return input_demo_collision_trace_enabled() &&
	       (input_demo_recorder_is_active() || input_demo_replay_is_loaded());
}

unsigned int input_demo_trace_collision_frame_index(void)
{
	if (input_demo_replay_is_loaded())
		return (unsigned int) input_demo_replay_next_frame_index();
	if (input_demo_recorder_is_active()) {
		const uint32_t frame_count = input_demo_recorder_frame_count();

		return frame_count ? (unsigned int) (frame_count - 1) : 0;
	}
	return 0;
}

const char *input_demo_trace_collision_mode_name(void)
{
	if (input_demo_replay_is_loaded())
		return "replay";
	if (input_demo_recorder_is_active())
		return "record";
	return "none";
}

void input_demo_log_player_bump_probe(const char *step, object *obj0,
                                      object *obj1, const vms_vector *relative_velocity,
                                      const vms_vector *float_force, fix scale_num, fix scale_den,
                                      int damage_flag)
{
	const object *player = NULL;
	const object *other = NULL;
	const char *mode_name;
	unsigned int frame_index;
	vms_vector fix_force = { 0, 0, 0 };
	vms_vector force_delta = { 0, 0, 0 };

	if (!input_demo_trace_collision_pose_active())
		return;
	mode_name = input_demo_trace_collision_mode_name();
	frame_index = input_demo_trace_collision_frame_index();

	if (obj0 == ConsoleObject && obj0 && obj0->type == OBJ_PLAYER &&
	    obj1 && (obj1->type == OBJ_WEAPON || obj1->type == OBJ_ROBOT)) {
		player = obj0;
		other = obj1;
	} else if (obj1 == ConsoleObject && obj1 && obj1->type == OBJ_PLAYER &&
	           obj0 && (obj0->type == OBJ_WEAPON || obj0->type == OBJ_ROBOT)) {
		player = obj1;
		other = obj0;
	}
	if (!player || !other)
		return;
	if (relative_velocity && scale_den) {
		fix_force.x = fixmuldiv(relative_velocity->x, scale_num, scale_den);
		fix_force.y = fixmuldiv(relative_velocity->y, scale_num, scale_den);
		fix_force.z = fixmuldiv(relative_velocity->z, scale_num, scale_den);
	}
	if (float_force) {
		force_delta.x = float_force->x - fix_force.x;
		force_delta.y = float_force->y - fix_force.y;
		force_delta.z = float_force->z - fix_force.z;
	}

	con_printf(CON_NORMAL,
	           "Input demo bump probe: mode=%s frame=%u gt=%lld step=%s obj0=%d/%d/%d seg=%d pos=(%d,%d,%d) obj1=%d/%d/%d seg=%d pos=(%d,%d,%d) damage=%d rel_vel=(%d,%d,%d) scale=(%d,%d) float_force=(%d,%d,%d) fix_force=(%d,%d,%d) delta=(%d,%d,%d) obj0_vel=(%d,%d,%d) obj1_vel=(%d,%d,%d) obj0_mass=%d obj1_mass=%d\n",
	           mode_name,
	           frame_index,
	           (long long) GameTime64,
	           step,
	           obj0 ? (int) (obj0 - Objects) : -1,
	           obj0 ? obj0->type : -1,
	           obj0 ? obj0->id : -1,
	           obj0 ? obj0->segnum : -1,
	           obj0 ? obj0->pos.x : 0,
	           obj0 ? obj0->pos.y : 0,
	           obj0 ? obj0->pos.z : 0,
	           obj1 ? (int) (obj1 - Objects) : -1,
	           obj1 ? obj1->type : -1,
	           obj1 ? obj1->id : -1,
	           obj1 ? obj1->segnum : -1,
	           obj1 ? obj1->pos.x : 0,
	           obj1 ? obj1->pos.y : 0,
	           obj1 ? obj1->pos.z : 0,
	           damage_flag,
	           relative_velocity ? relative_velocity->x : 0,
	           relative_velocity ? relative_velocity->y : 0,
	           relative_velocity ? relative_velocity->z : 0,
	           scale_num,
	           scale_den,
	           float_force ? float_force->x : 0,
	           float_force ? float_force->y : 0,
	           float_force ? float_force->z : 0,
	           fix_force.x,
	           fix_force.y,
	           fix_force.z,
	           force_delta.x,
	           force_delta.y,
	           force_delta.z,
	           obj0 ? obj0->mtype.phys_info.velocity.x : 0,
	           obj0 ? obj0->mtype.phys_info.velocity.y : 0,
	           obj0 ? obj0->mtype.phys_info.velocity.z : 0,
	           obj1 ? obj1->mtype.phys_info.velocity.x : 0,
	           obj1 ? obj1->mtype.phys_info.velocity.y : 0,
	           obj1 ? obj1->mtype.phys_info.velocity.z : 0,
	           obj0 ? obj0->mtype.phys_info.mass : 0,
	           obj1 ? obj1->mtype.phys_info.mass : 0);
}