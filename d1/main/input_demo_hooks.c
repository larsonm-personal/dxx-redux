#include <stdio.h>
#include <string.h>

#include "args.h"
#include "ai.h"
#include "cntrlcen.h"
#include "console.h"
#include "controls.h"
#include "endlevel.h"
#include "fvi.h"
#include "game.h"
#include "gameseq.h"
#include "input_demo_control_info.h"
#include "input_demo_debug_logging.h"
#include "input_demo_direct_command_policy.h"
#include "input_demo_fp_env.h"
#include "input_demo_result.h"
#include "input_demo_state_trace.h"
#include "input_demo_hooks.h"
#include "input_demo_recorder.h"
#include "input_demo_replay.h"
#include "input_demo_rng_trace.h"
#include "laser.h"
#include "timer.h"
#include "maths.h"
#include "mission.h"
#include "newdemo.h"
#include "object.h"
#include "player.h"
#include "segment.h"
#include "wall.h"
#include "weapon.h"

extern int Num_awareness_events;
#include "input_demo_hooks_shared.h"

static int input_demo_record_event_append_logged_error = 0;

void input_demo_record_death_abort_direct_command(void)
{
	char error[256] = "";

	if (!input_demo_recorder_is_active())
		return;
	if (!input_demo_recorder_stage_direct_command_death_abort(error, sizeof(error)) &&
		error[0])
		con_printf(CON_NORMAL,
			"Input demo recorder death abort event failed: %s\n", error);
}

void input_demo_record_direct_command_select_weapon_exact(int weapon_class,
	int weapon_index)
{
	char error[256] = "";
	if (!input_demo_recorder_is_active())
		return;
	if (!input_demo_recorder_stage_direct_command_select_weapon_exact(
	        weapon_class, weapon_index, error, sizeof(error)) && error[0])
		con_printf(CON_NORMAL,
			"Input demo recorder exact weapon selection failed: %s\n", error);
}

static int input_demo_apply_death_abort_d1(void *context, int validate_only,
	char *error, size_t error_size)
{
	(void)context;
	(void)error;
	(void)error_size;
	if (!validate_only)
		Death_sequence_aborted = 1;
	return 1;
}

static int input_demo_change_difficulty_d1(void *context, int difficulty,
	int validate_only, char *error, size_t error_size)
{
	(void)context;
	if (difficulty < 0 || difficulty > 4) {
		if (error && error_size)
			snprintf(error, error_size, "%s", "difficulty is out of range");
		return 0;
	}
	if (validate_only)
		return 1;
	if (difficulty_change_to(difficulty, DIFFICULTY_CHANGE_FROM_REPLAY))
		return 1;
	if (error && error_size)
		snprintf(error, error_size, "%s", "change difficulty replay event failed");
	return 0;
}

static int input_demo_apply_d1_direct_command(void *context,
	const input_demo_replay_direct_command_event *event,
	int validate_only, char *error, size_t error_size)
{
	(void)context;
	if (event->kind != INPUT_DEMO_REPLAY_DIRECT_COMMAND_SELECT_WEAPON_EXACT ||
	    event->value0 < 0 || event->value0 > 1 ||
	    event->value1 < 0 || event->value1 >= MAX_PRIMARY_WEAPONS) {
		if (error && error_size)
			snprintf(error, error_size, "%s", "unsupported D1 direct command");
		return 0;
	}
	if (!validate_only)
		select_weapon(event->value1, event->value0, 1, 1);
	return 1;
}

int input_demo_apply_replay_direct_commands(input_demo_direct_command_phase phase)
{
	static const input_demo_direct_command_policy policy = {
		NULL,
		1,
		input_demo_apply_death_abort_d1,
		input_demo_change_difficulty_d1,
		input_demo_apply_d1_direct_command
	};
	char error[512] = "";

	if (input_demo_direct_command_apply_current_frame(
		&policy, phase, error, sizeof(error)))
		return 1;
	if (error[0])
		con_printf(CON_NORMAL,
			"Input demo replay direct command failed: %s\n", error);
	return 0;
}

static unsigned int fvi_input_demo_frame_index(void)
{
	if (input_demo_replay_is_loaded())
		return (unsigned int)input_demo_replay_next_frame_index();
	if (input_demo_recorder_is_active()) {
		const uint32_t n = input_demo_recorder_frame_count();
		return n ? (unsigned int)(n - 1) : 0;
	}
	return 0;
}

static const char *fvi_input_demo_mode_name(void)
{
	if (input_demo_replay_is_loaded())
		return "replay";
	if (input_demo_recorder_is_active())
		return "record";
	return "none";
}

void input_demo_log_fvi_weapon_robot_check(
	const vms_vector *p0, const vms_vector *p1,
	int weapon_objnum, int robot_objnum, fix fudged_rad, fix d)
{
	object *weapon;
	object *robot;
	fix center_dist;
	fix combined_rad;
	fix miss_delta;
	unsigned int frame;

	if (!input_demo_replay_is_loaded() && !input_demo_recorder_is_active())
		return;
	if (weapon_objnum < 0 || robot_objnum < 0)
		return;
	weapon = &Objects[weapon_objnum];
	robot = &Objects[robot_objnum];
	if (weapon->type != OBJ_WEAPON || weapon->ctype.laser_info.parent_type != OBJ_PLAYER)
		return;
	if (robot->type != OBJ_ROBOT)
		return;
	frame = fvi_input_demo_frame_index();
	center_dist = vm_vec_dist(p0, &robot->pos);
	combined_rad = robot->size + fudged_rad;
	miss_delta = center_dist - combined_rad;
	if (!d && miss_delta > (8 * f1_0))
		return;
	con_printf(CON_NORMAL,
		"Input demo fvi weapon robot check: mode=%s frame=%u gt=%lld "
		"weapon_obj=%d weapon_sig=%d p0=(%d,%d,%d) p1=(%d,%d,%d) "
		"robot_obj=%d robot_sig=%d robot_id=%d robot_pos=(%d,%d,%d) "
		"robot_size=%d fudged_rad=%d combined_rad=%d center_dist=%d miss_delta=%d d=%d hit=%d\n",
		fvi_input_demo_mode_name(), frame, (long long)GameTime64,
		weapon_objnum, weapon->signature,
		p0->x, p0->y, p0->z,
		p1->x, p1->y, p1->z,
		robot_objnum, robot->signature, robot->id,
		robot->pos.x, robot->pos.y, robot->pos.z,
		robot->size, fudged_rad, combined_rad, center_dist, miss_delta, d, d > 0);
}

int input_demo_restore_checkpoint_object_links(void)
{
	int i, objnum, segnum;
	int segment_heads[MAX_SEGMENTS];
	ubyte seen[MAX_OBJECTS];

	if (!input_demo_replay_has_checkpoint())
		return 0;

	memset(seen, 0, sizeof(seen));
	for (segnum = 0; segnum <= Highest_segment_index; segnum++)
		segment_heads[segnum] = -1;

	for (i = 0; i <= Highest_object_index; i++) {
		object *obj = &Objects[i];

		if (obj->type == OBJ_NONE)
			continue;
		if (obj->segnum < 0 || obj->segnum > Highest_segment_index)
			return 0;
		if (obj->prev == -1) {
			if (segment_heads[obj->segnum] != -1)
				return 0;
			segment_heads[obj->segnum] = i;
		}
	}

	for (i = 0; i <= Highest_object_index; i++) {
		object *obj = &Objects[i];

		if (obj->type == OBJ_NONE)
			continue;
		segnum = obj->segnum;
		if (obj->prev == -1) {
			if (segment_heads[segnum] != i)
				return 0;
		} else {
			if (obj->prev < 0 || obj->prev > Highest_object_index)
				return 0;
			if (Objects[obj->prev].type == OBJ_NONE ||
				Objects[obj->prev].segnum != segnum ||
				Objects[obj->prev].next != i)
				return 0;
		}
		if (obj->next != -1) {
			if (obj->next < 0 || obj->next > Highest_object_index)
				return 0;
			if (Objects[obj->next].type == OBJ_NONE ||
				Objects[obj->next].segnum != segnum ||
				Objects[obj->next].prev != i)
				return 0;
		}
	}

	for (segnum = 0; segnum <= Highest_segment_index; segnum++)
		for (objnum = segment_heads[segnum]; objnum != -1;
			 objnum = Objects[objnum].next) {
			if (objnum < 0 || objnum > Highest_object_index)
				return 0;
			if (seen[objnum])
				return 0;
			if (Objects[objnum].type == OBJ_NONE ||
				Objects[objnum].segnum != segnum)
				return 0;
			seen[objnum] = 1;
		}

	for (i = 0; i <= Highest_object_index; i++)
		if (Objects[i].type != OBJ_NONE && !seen[i])
			return 0;

	for (i = 0; i <= Highest_object_index; i++)
		Objects[i].rtype.pobj_info.alt_textures = -1;
	for (segnum = 0; segnum <= Highest_segment_index; segnum++)
		Segments[segnum].objects = segment_heads[segnum];

	return 1;
}

static void input_demo_append_replay_probe_message_d1(const char *kind,
	object *objp, const char *message)
{
	char path[1024];
	char *slash;
	FILE *file;
	const char *result_path = input_demo_replay_actual_result_path();
	const int objnum = objp ? (int)(objp - Objects) : -1;

	if (!input_demo_replay_is_loaded() || !kind || !message ||
		!result_path || !result_path[0])
		return;

	snprintf(path, sizeof(path), "%s", result_path);
	slash = strrchr(path, '\\');
	if (!slash)
		slash = strrchr(path, '/');
	if (!slash)
		return;
	slash[1] = 0;
	if (strlen(path) + strlen("ai_schedule_probe.log") + 1 >= sizeof(path))
		return;
	strcat(path, "ai_schedule_probe.log");

	file = fopen(path, "a");
	if (!file)
		return;
	fprintf(file, "frame=%u gt=%lld kind=%s obj=%d sig=%d id=%d seg=%d %s\n",
		input_demo_debug_frame_index(),
		(long long)GameTime64,
		kind,
		objnum,
		objp ? objp->signature : -1,
		objp ? objp->id : -1,
		objp ? objp->segnum : -1,
		message);
	fclose(file);
}

static void input_demo_record_frame_event_json_d1(const char *json_text)
{
	char error[256] = "";

	if (!input_demo_recorder_is_active())
		return;
	if (!input_demo_recorder_append_frame_event_json(json_text, error, sizeof(error)) &&
		!input_demo_record_event_append_logged_error) {
		input_demo_record_event_append_logged_error = 1;
		con_printf(CON_NORMAL, "Input demo recorder event append failed: %s\n", error);
	}
}

static int input_demo_trace_motion_probe_active(void)
{
	return input_demo_recorder_is_active() || input_demo_replay_is_loaded();
}

static unsigned int input_demo_trace_motion_frame_index(void)
{
	if (input_demo_replay_is_loaded())
		return (unsigned int)input_demo_replay_next_frame_index();
	if (input_demo_recorder_is_active()) {
		const unsigned int frame_count = (unsigned int)input_demo_recorder_frame_count();

		return frame_count ? frame_count - 1 : 0;
	}
	return 0;
}

static const char *input_demo_physics_fate_name(int fate)
{
	switch (fate) {
		case HIT_NONE:
			return "none";
		case HIT_WALL:
			return "wall";
		case HIT_OBJECT:
			return "object";
		case HIT_BAD_P0:
			return "bad_p0";
		default:
			return "unknown";
	}
}

void input_demo_log_replay_physics_fvi_fate(object *obj, int fate,
	const fvi_info *hit_info, const vms_vector *frame_vec,
	const vms_vector *new_pos, fix sim_time, int ignore_count)
{
	char path[1024];
	char *slash;
	FILE *file;
	const char *result_path = input_demo_replay_actual_result_path();
	const int objnum = obj ? (int)(obj - Objects) : -1;
	const object *hit_obj = NULL;
	const int hit_objnum = hit_info ? hit_info->hit_object : -1;
	int child_seg = -2;
	int wall_num = -1;
	int wall_type = -1;
	int wall_state = -1;
	int wall_flags = 0;
	int doorway_flags = 0;

	if (!input_demo_replay_is_loaded() || !obj || !hit_info || !frame_vec ||
		!new_pos || !result_path || !result_path[0])
		return;
	if (hit_objnum >= 0 && hit_objnum <= Highest_object_index)
		hit_obj = &Objects[hit_objnum];
	if (objnum != 13 && objnum != 15 && obj != ConsoleObject &&
		hit_obj != ConsoleObject &&
		(obj->type != OBJ_WEAPON ||
			obj->ctype.laser_info.parent_type != OBJ_PLAYER ||
			(obj->flags & (OF_SHOULD_BE_DEAD | OF_HARMLESS))))
		return;
	if (hit_info->hit_side_seg >= 0 && hit_info->hit_side_seg <= Highest_segment_index &&
		hit_info->hit_side >= 0 && hit_info->hit_side < MAX_SIDES_PER_SEGMENT) {
		segment *wall_seg = &Segments[hit_info->hit_side_seg];

		child_seg = wall_seg->children[hit_info->hit_side];
		wall_num = wall_seg->sides[hit_info->hit_side].wall_num;
		doorway_flags = WALL_IS_DOORWAY(wall_seg, hit_info->hit_side);
		if (wall_num >= 0 && wall_num < Num_walls) {
			wall_type = Walls[wall_num].type;
			wall_state = Walls[wall_num].state;
			wall_flags = Walls[wall_num].flags;
		}
	}
	snprintf(path, sizeof(path), "%s", result_path);
	slash = strrchr(path, '\\');
	if (!slash)
		slash = strrchr(path, '/');
	if (!slash)
		return;
	slash[1] = 0;
	if (strlen(path) + strlen("ai_schedule_probe.log") + 1 >= sizeof(path))
		return;
	strcat(path, "ai_schedule_probe.log");
	file = fopen(path, "a");
	if (!file)
		return;
	fprintf(file,
		"frame=%u gt=%lld kind=physics_fvi_fate obj=%d sig=%d id=%d seg=%d "
		"fate=%s hit_seg=%d hit_side=%d hit_side_seg=%d hit_obj=%d "
		"child=%d wall=%d wall_type=%d wall_state=%d wall_flags=0x%x doorway=0x%x "
		"hit_type=%d hit_id=%d hit_sig=%d hit_segnum=%d "
		"start=(%d,%d,%d) target=(%d,%d,%d) frame_vec=(%d,%d,%d) "
		"vel=(%d,%d,%d) sim_time=%d ignore_count=%d\n",
		input_demo_trace_motion_frame_index(),
		(long long)GameTime64,
		objnum,
		obj->signature,
		obj->id,
		obj->segnum,
		input_demo_physics_fate_name(fate),
		hit_info->hit_seg,
		hit_info->hit_side,
		hit_info->hit_side_seg,
		hit_objnum,
		child_seg,
		wall_num,
		wall_type,
		wall_state,
		wall_flags,
		doorway_flags,
		hit_obj ? hit_obj->type : -1,
		hit_obj ? hit_obj->id : -1,
		hit_obj ? hit_obj->signature : -1,
		hit_obj ? hit_obj->segnum : -1,
		obj->pos.x,
		obj->pos.y,
		obj->pos.z,
		new_pos->x,
		new_pos->y,
		new_pos->z,
		frame_vec->x,
		frame_vec->y,
		frame_vec->z,
		obj->mtype.phys_info.velocity.x,
		obj->mtype.phys_info.velocity.y,
		obj->mtype.phys_info.velocity.z,
		sim_time,
		ignore_count);
	fclose(file);
}

static const char *input_demo_trace_motion_mode_name(void)
{
	if (input_demo_replay_is_loaded())
		return "replay";
	if (input_demo_recorder_is_active())
		return "record";
	return "none";
}

static int input_demo_player_robot_hit_object_probe_active(object *obj, int hit_object)
{
	object *other;

	if (!input_demo_trace_motion_probe_active() ||
		!ConsoleObject ||
		hit_object < 0 ||
		hit_object > Highest_object_index)
		return 0;

	other = &Objects[hit_object];
	if (obj != ConsoleObject && other != ConsoleObject)
		return 0;

	if (obj == ConsoleObject)
		return other->type == OBJ_ROBOT;

	return obj->type == OBJ_ROBOT;
}

void input_demo_log_player_robot_hit_object_probe(
	const char *step,
	object *moving_obj,
	int hit_object,
	const vms_vector *collision_point,
	const vms_vector *old_velocity,
	int ignore_count,
	int will_retry,
	int ignored_hit)
{
	object *other;
	object *player;
	object *robot;
	const int unchanged_velocity = old_velocity &&
		old_velocity->x == moving_obj->mtype.phys_info.velocity.x &&
		old_velocity->y == moving_obj->mtype.phys_info.velocity.y &&
		old_velocity->z == moving_obj->mtype.phys_info.velocity.z;

	if (!input_demo_player_robot_hit_object_probe_active(moving_obj, hit_object))
		return;

	other = &Objects[hit_object];
	player = moving_obj == ConsoleObject ? moving_obj : other;
	robot = moving_obj == ConsoleObject ? other : moving_obj;
	con_printf(CON_NORMAL,
		"Input demo physics object contact: mode=%s frame=%u gt=%lld step=%s move_obj=%d/%d/%d sig=%d seg=%d pos=(%d,%d,%d) last=(%d,%d,%d) vel=(%d,%d,%d) flags=0x%x ctype=%d mtype=%d persistent=%d unchanged=%d retry=%d ignored=%d ignore_count=%d old_vel=(%d,%d,%d) player=%d/%d/%d seg=%d pos=(%d,%d,%d) vel=(%d,%d,%d) shields=%d flags=0x%x robot=%d/%d/%d sig=%d seg=%d pos=(%d,%d,%d) vel=(%d,%d,%d) shields=%d flags=0x%x ctype=%d mtype=%d cp=(%d,%d,%d)\n",
		input_demo_trace_motion_mode_name(),
		input_demo_trace_motion_frame_index(),
		(long long)GameTime64,
		step,
		moving_obj - Objects,
		moving_obj->type,
		moving_obj->id,
		moving_obj->signature,
		moving_obj->segnum,
		moving_obj->pos.x,
		moving_obj->pos.y,
		moving_obj->pos.z,
		moving_obj->last_pos.x,
		moving_obj->last_pos.y,
		moving_obj->last_pos.z,
		moving_obj->mtype.phys_info.velocity.x,
		moving_obj->mtype.phys_info.velocity.y,
		moving_obj->mtype.phys_info.velocity.z,
		moving_obj->flags,
		moving_obj->control_type,
		moving_obj->movement_type,
		(moving_obj->mtype.phys_info.flags & PF_PERSISTENT) != 0,
		unchanged_velocity,
		will_retry,
		ignored_hit,
		ignore_count,
		old_velocity ? old_velocity->x : 0,
		old_velocity ? old_velocity->y : 0,
		old_velocity ? old_velocity->z : 0,
		player - Objects,
		player->type,
		player->id,
		player->segnum,
		player->pos.x,
		player->pos.y,
		player->pos.z,
		player->mtype.phys_info.velocity.x,
		player->mtype.phys_info.velocity.y,
		player->mtype.phys_info.velocity.z,
		Players[Player_num].shields,
		player->flags,
		robot - Objects,
		robot->type,
		robot->id,
		robot->signature,
		robot->segnum,
		robot->pos.x,
		robot->pos.y,
		robot->pos.z,
		robot->mtype.phys_info.velocity.x,
		robot->mtype.phys_info.velocity.y,
		robot->mtype.phys_info.velocity.z,
		robot->shields,
		robot->flags,
		robot->control_type,
		robot->movement_type,
		collision_point ? collision_point->x : 0,
		collision_point ? collision_point->y : 0,
		collision_point ? collision_point->z : 0);
}

void input_demo_log_powerup_spawn_probe(object *source, object *created, int created_objnum)
{
	char probe[256];

	if (!source || !created)
		return;
	snprintf(probe, sizeof(probe),
		"source=%d/%d/%d/%d seg=%d flags=0x%x contains=%d/%d/%d created=%d/%d/%d/%d/%d count=%d",
		(int)(source - Objects),
		source->signature,
		source->type,
		source->id,
		source->segnum,
		source->flags,
		source->contains_type,
		source->contains_id,
		source->contains_count,
		created_objnum,
		created->signature,
		created->id,
		created->segnum,
		created->flags,
		created->ctype.powerup_info.count);
	input_demo_append_replay_probe_message_d1("powerup_spawn", created, probe);
}

void input_demo_log_weapon_lifetime(const char *step, object *obj)
{
	char probe[512];
	char json[768];

	if (!step || !obj || obj->type != OBJ_WEAPON)
		return;
	snprintf(json, sizeof(json),
		"{\"kind\":\"probe_weapon_life\",\"gt\":%lld,\"step\":\"%s\",\"obj\":%d,\"id\":%d,\"sig\":%d,\"seg\":%d,\"life\":%d,\"shields\":%d,\"flags\":%d,\"parent_type\":%d,\"parent\":%d,\"parent_sig\":%d,\"last_hit\":%d,\"track_goal\":%d,\"creation_frame\":%u,\"ctime\":%lld,\"homing\":%s,\"vx\":%d,\"vy\":%d,\"vz\":%d,\"x\":%d,\"y\":%d,\"z\":%d,\"lastx\":%d,\"lasty\":%d,\"lastz\":%d}",
		(long long)GameTime64,
		step,
		(int)(obj - Objects),
		obj->id,
		obj->signature,
		obj->segnum,
		obj->lifeleft,
		obj->shields,
		obj->flags,
		obj->ctype.laser_info.parent_type,
		obj->ctype.laser_info.parent_num,
		obj->ctype.laser_info.parent_signature,
		obj->ctype.laser_info.last_hitobj,
		obj->ctype.laser_info.track_goal,
		obj->ctype.laser_info.creation_framecount,
		(long long)obj->ctype.laser_info.creation_time,
		Weapon_info[obj->id].homing_flag ? "true" : "false",
		obj->mtype.phys_info.velocity.x,
		obj->mtype.phys_info.velocity.y,
		obj->mtype.phys_info.velocity.z,
		obj->pos.x,
		obj->pos.y,
		obj->pos.z,
		obj->last_pos.x,
		obj->last_pos.y,
		obj->last_pos.z);
	input_demo_record_frame_event_json_d1(json);
	snprintf(probe, sizeof(probe),
		"step=%s id=%d homing=%d parent_type=%d parent=%d parent_sig=%d "
		"last_hit=%d track_goal=%d life=%d seg=%d vel=(%d,%d,%d) "
		"pos=(%d,%d,%d) last=(%d,%d,%d)",
		step,
		obj->id,
		Weapon_info[obj->id].homing_flag,
		obj->ctype.laser_info.parent_type,
		obj->ctype.laser_info.parent_num,
		obj->ctype.laser_info.parent_signature,
		obj->ctype.laser_info.last_hitobj,
		obj->ctype.laser_info.track_goal,
		obj->lifeleft,
		obj->segnum,
		obj->mtype.phys_info.velocity.x,
		obj->mtype.phys_info.velocity.y,
		obj->mtype.phys_info.velocity.z,
		obj->pos.x,
		obj->pos.y,
		obj->pos.z,
		obj->last_pos.x,
		obj->last_pos.y,
		obj->last_pos.z);
	input_demo_append_replay_probe_message_d1("weapon_life", obj, probe);
}

void input_demo_record_homing_state(const char *step, object *obj,
	int straight_time_active, int do_homer_frame, int track_goal_before,
	int track_goal_after, int dot, int32_t ideal_homer_frame_time,
	unsigned int homer_frame_count)
{
	char probe[512];
	char json[768];

	if (!obj || obj->type != OBJ_WEAPON || !Weapon_info[obj->id].homing_flag)
		return;
	snprintf(json, sizeof(json),
		"{\"kind\":\"probe_homing\",\"gt\":%lld,\"step\":\"%s\",\"obj\":%d,\"id\":%d,\"sig\":%d,\"seg\":%d,\"life\":%d,\"parent_type\":%d,\"parent\":%d,\"parent_sig\":%d,\"straight\":%s,\"do_homer_frame\":%s,\"track_goal_before\":%d,\"track_goal_after\":%d,\"dot\":%d,\"ideal_frame_time\":%d,\"homer_frame_count\":%u,\"creation_frame\":%u,\"vx\":%d,\"vy\":%d,\"vz\":%d,\"x\":%d,\"y\":%d,\"z\":%d}",
		(long long)GameTime64,
		step ? step : "unset",
		(int)(obj - Objects),
		obj->id,
		obj->signature,
		obj->segnum,
		obj->lifeleft,
		obj->ctype.laser_info.parent_type,
		obj->ctype.laser_info.parent_num,
		obj->ctype.laser_info.parent_signature,
		straight_time_active ? "true" : "false",
		do_homer_frame ? "true" : "false",
		track_goal_before,
		track_goal_after,
		dot,
		ideal_homer_frame_time,
		homer_frame_count,
		obj->ctype.laser_info.creation_framecount,
		obj->mtype.phys_info.velocity.x,
		obj->mtype.phys_info.velocity.y,
		obj->mtype.phys_info.velocity.z,
		obj->pos.x,
		obj->pos.y,
		obj->pos.z);
	input_demo_record_frame_event_json_d1(json);
	snprintf(probe, sizeof(probe),
		"step=%s straight=%d do_homer_frame=%d "
		"track_goal_before=%d track_goal_after=%d dot=%d "
		"ideal_frame_time=%d homer_frame_count=%u creation_frame=%u "
		"fvec=(%d,%d,%d) vel=(%d,%d,%d) pos=(%d,%d,%d)",
		step ? step : "unset",
		straight_time_active,
		do_homer_frame,
		track_goal_before,
		track_goal_after,
		dot,
		ideal_homer_frame_time,
		homer_frame_count,
		obj->ctype.laser_info.creation_framecount,
		obj->orient.fvec.x,
		obj->orient.fvec.y,
		obj->orient.fvec.z,
		obj->mtype.phys_info.velocity.x,
		obj->mtype.phys_info.velocity.y,
		obj->mtype.phys_info.velocity.z,
		obj->pos.x,
		obj->pos.y,
		obj->pos.z);
	input_demo_append_replay_probe_message_d1("homing_state", obj, probe);
}

void input_demo_log_player_shot_create_probe(object *shooter, object *weapon,
	int laser_type, int gun_num, int harmless, int make_sound,
	const vms_vector *direction)
{
	char probe[512];

	if (!shooter || !weapon)
		return;
	snprintf(probe, sizeof(probe),
		"shooter=%d laser_type=%d gun=%d harmless=%d sound=%d "
		"pos=(%d,%d,%d) dir=(%d,%d,%d)",
		(int)(shooter - Objects),
		laser_type,
		gun_num,
		harmless,
		make_sound,
		weapon->pos.x,
		weapon->pos.y,
		weapon->pos.z,
		direction ? direction->x : 0,
		direction ? direction->y : 0,
		direction ? direction->z : 0);
	input_demo_append_replay_probe_message_d1("player_shot_create", weapon, probe);
}

void input_demo_log_replay_collision_pair(const char *kind, object *obj0,
	object *obj1, const vms_vector *collision_point)
{
	char probe[768];

	if (!kind || !obj0 || !obj1)
		return;
	snprintf(probe, sizeof(probe),
		"a=%d/%d/%d sig=%d seg=%d pos=(%d,%d,%d) last=(%d,%d,%d) "
		"vel=(%d,%d,%d) size=%d shields=%d flags=0x%x "
		"b=%d/%d/%d sig=%d seg=%d pos=(%d,%d,%d) last=(%d,%d,%d) "
		"vel=(%d,%d,%d) size=%d shields=%d flags=0x%x cp=(%d,%d,%d)",
		(int)(obj0 - Objects),
		obj0->type,
		obj0->id,
		obj0->signature,
		obj0->segnum,
		obj0->pos.x,
		obj0->pos.y,
		obj0->pos.z,
		obj0->last_pos.x,
		obj0->last_pos.y,
		obj0->last_pos.z,
		obj0->mtype.phys_info.velocity.x,
		obj0->mtype.phys_info.velocity.y,
		obj0->mtype.phys_info.velocity.z,
		obj0->size,
		obj0->shields,
		obj0->flags,
		(int)(obj1 - Objects),
		obj1->type,
		obj1->id,
		obj1->signature,
		obj1->segnum,
		obj1->pos.x,
		obj1->pos.y,
		obj1->pos.z,
		obj1->last_pos.x,
		obj1->last_pos.y,
		obj1->last_pos.z,
		obj1->mtype.phys_info.velocity.x,
		obj1->mtype.phys_info.velocity.y,
		obj1->mtype.phys_info.velocity.z,
		obj1->size,
		obj1->shields,
		obj1->flags,
		collision_point ? collision_point->x : 0,
		collision_point ? collision_point->y : 0,
		collision_point ? collision_point->z : 0);
	input_demo_append_replay_probe_message_d1(kind, obj0, probe);
}

void input_demo_log_weapon_robot_path_probe(const char *step, object *weapon,
	object *robot, const vms_vector *collision_point)
{
	char probe[512];
	char json[768];

	if (!step || !weapon || !robot)
		return;
	input_demo_debug_log_weapon_robot_path_probe(step, weapon, robot,
		collision_point);
	snprintf(json, sizeof(json),
		"{\"kind\":\"weapon_robot_path\",\"gt\":%lld,\"step\":\"%s\",\"weapon_obj\":%d,\"weapon_id\":%d,\"weapon_sig\":%d,\"weapon_seg\":%d,\"weapon_flags\":%d,\"parent_type\":%d,\"parent\":%d,\"parent_sig\":%d,\"last_hit\":%d,\"track_goal\":%d,\"robot_obj\":%d,\"robot_id\":%d,\"robot_sig\":%d,\"robot_seg\":%d,\"robot_flags\":%d,\"robot_shields\":%d,\"weapon_vx\":%d,\"weapon_vy\":%d,\"weapon_vz\":%d,\"robot_vx\":%d,\"robot_vy\":%d,\"robot_vz\":%d}",
		(long long)GameTime64,
		step,
		(int)(weapon - Objects),
		weapon->id,
		weapon->signature,
		weapon->segnum,
		weapon->flags,
		weapon->ctype.laser_info.parent_type,
		weapon->ctype.laser_info.parent_num,
		weapon->ctype.laser_info.parent_signature,
		weapon->ctype.laser_info.last_hitobj,
		weapon->ctype.laser_info.track_goal,
		(int)(robot - Objects),
		robot->id,
		robot->signature,
		robot->segnum,
		robot->flags,
		robot->shields,
		weapon->mtype.phys_info.velocity.x,
		weapon->mtype.phys_info.velocity.y,
		weapon->mtype.phys_info.velocity.z,
		robot->mtype.phys_info.velocity.x,
		robot->mtype.phys_info.velocity.y,
		robot->mtype.phys_info.velocity.z);
	input_demo_record_frame_event_json_d1(json);
	snprintf(probe, sizeof(probe),
		"step=%s weapon=%d/%d/%d/%d/%d robot=%d/%d/%d/%d/%d "
		"robot_flags=0x%x robot_shields=%d weapon_flags=0x%x "
		"weapon_life=%d cp=(%d,%d,%d)",
		step,
		(int)(weapon - Objects),
		weapon->signature,
		weapon->type,
		weapon->id,
		weapon->segnum,
		(int)(robot - Objects),
		robot->signature,
		robot->type,
		robot->id,
		robot->segnum,
		robot->flags,
		robot->shields,
		weapon->flags,
		weapon->lifeleft,
		collision_point ? collision_point->x : 0,
		collision_point ? collision_point->y : 0,
		collision_point ? collision_point->z : 0);
	input_demo_append_replay_probe_message_d1("weapon_robot_path", weapon, probe);
}

void input_demo_capture_state_trace_diag(input_demo_state_trace_diag *diag)
{
	if (!diag)
		return;
	memset(diag, 0, sizeof(*diag));
	diag->awareness_events = Num_awareness_events;
	diag->d_tick_count = d_tick_count;
	diag->ai_probe_skip_obj = -1;
	diag->ai_probe_skip_sig = -1;
	diag->ai_probe_skip_id = -1;
	diag->ai_probe_timeslice_obj = -1;
	diag->ai_probe_timeslice_sig = -1;
	diag->ai_probe_timeslice_id = -1;
	diag->ai_probe_process_obj = -1;
	diag->ai_probe_process_sig = -1;
	diag->ai_probe_process_id = -1;
	diag->ai_probe_phys_skip_obj = -1;
	diag->ai_probe_phys_skip_sig = -1;
	diag->ai_probe_phys_skip_id = -1;
	diag->ai_probe_phys_skip_before = -1;
	diag->ai_probe_phys_skip_after = -1;
	input_demo_capture_runtime_state_diag(diag);
	input_demo_capture_object_state_diag(diag);
	input_demo_capture_robot_ai_local_diag(diag);
	input_demo_capture_player_weapon_diag(diag);
}
void input_demo_log_player_robot_contact_probe(const char *step,
	object *player,
	object *robot,
	const vms_vector *collision_point,
	int32_t damage)
{
	if (!input_demo_trace_collision_pose_active() ||
		!player ||
		player != ConsoleObject ||
		player->type != OBJ_PLAYER ||
		!robot ||
		robot->type != OBJ_ROBOT)
		return;

	con_printf(CON_NORMAL,
		"Input demo player robot contact: mode=%s frame=%u gt=%lld step=%s player=%d/%d/%d seg=%d pos=(%d,%d,%d) vel=(%d,%d,%d) shields=%d robot=%d/%d/%d seg=%d pos=(%d,%d,%d) vel=(%d,%d,%d) flags=0x%x companion=%d thief=%d kamikaze=%d drain=%d cp=(%d,%d,%d) damage=%d\n",
		input_demo_trace_collision_mode_name(),
		input_demo_trace_collision_frame_index(),
		(long long)GameTime64,
		step,
		(int)(player - Objects),
		player->type,
		player->id,
		player->segnum,
		player->pos.x,
		player->pos.y,
		player->pos.z,
		player->mtype.phys_info.velocity.x,
		player->mtype.phys_info.velocity.y,
		player->mtype.phys_info.velocity.z,
		Players[player->id].shields,
		(int)(robot - Objects),
		robot->type,
		robot->id,
		robot->segnum,
		robot->pos.x,
		robot->pos.y,
		robot->pos.z,
		robot->mtype.phys_info.velocity.x,
		robot->mtype.phys_info.velocity.y,
		robot->mtype.phys_info.velocity.z,
		robot->flags,
		0,
		0,
		0,
		0,
		collision_point ? collision_point->x : 0,
		collision_point ? collision_point->y : 0,
		collision_point ? collision_point->z : 0,
		damage);
}

static int input_demo_trace_local_player_weapon_robot_active(object *weapon, object *robot)
{
	if (!input_demo_trace_collision_pose_active() || !weapon || !robot)
		return 0;
	if (weapon->type != OBJ_WEAPON || robot->type != OBJ_ROBOT)
		return 0;
	if (weapon->ctype.laser_info.parent_type != OBJ_PLAYER)
		return 0;
	return weapon->ctype.laser_info.parent_num == Players[Player_num].objnum;
}

void input_demo_log_weapon_robot_accept_seq(object *weapon, object *robot)
{
	static unsigned int last_frame = (unsigned int)-1;
	static int accept_seq = 0;
	unsigned int frame;

	if (!input_demo_trace_local_player_weapon_robot_active(weapon, robot))
		return;

	frame = input_demo_trace_collision_frame_index();
	input_demo_debug_log_weapon_robot_accept_seq(weapon, robot);
	if (frame != last_frame) {
		last_frame = frame;
		accept_seq = 0;
	}
	con_printf(CON_NORMAL,
		"Input demo weapon robot accept seq: mode=%s frame=%u accept_seq=%d weapon_obj=%d weapon_sig=%d robot_obj=%d robot_sig=%d\n",
		input_demo_trace_collision_mode_name(),
		frame,
		accept_seq,
		(int)(weapon - Objects),
		weapon->signature,
		(int)(robot - Objects),
		robot->signature);
	accept_seq++;
}

void input_demo_log_robot_fire_probe(object *objp, const vms_vector *fire_vec,
	int weapon_type)
{
	unsigned int frame;

	if (!input_demo_replay_is_loaded() || !objp || objp->type != OBJ_ROBOT || !fire_vec)
		return;

	frame = (unsigned int)input_demo_replay_next_frame_index();
	if (frame < 200 || frame > 260)
		return;

	con_printf(CON_NORMAL,
		"Input demo replay fire probe: frame=%u kind=robot_fire robot_obj=%d sig=%d robot_id=%d seg=%d gun=%d weapon=%d player_seg=%d pos=(%d,%d,%d) vec=(%d,%d,%d)\n",
		frame,
		(int)(objp - Objects),
		objp->signature,
		objp->id,
		objp->segnum,
		objp->ctype.ai_info.CURRENT_GUN,
		weapon_type,
		ConsoleObject ? ConsoleObject->segnum : -1,
		objp->pos.x,
		objp->pos.y,
		objp->pos.z,
		fire_vec->x,
		fire_vec->y,
		fire_vec->z);
}

int input_demo_trace_ai_visibility_active(object *objp)
{
	return input_demo_debug_is_enabled() &&
		objp &&
		(objp->type == OBJ_ROBOT) &&
		(input_demo_recorder_is_active() || input_demo_replay_is_loaded());
}

static void input_demo_hit_object_details(int hit_object, int *hit_obj_type,
	int *hit_obj_id, int *hit_obj_sig, int *hit_obj_seg)
{
	if (hit_obj_type)
		*hit_obj_type = -1;
	if (hit_obj_id)
		*hit_obj_id = -1;
	if (hit_obj_sig)
		*hit_obj_sig = -1;
	if (hit_obj_seg)
		*hit_obj_seg = -1;

	if (hit_object < 0 || hit_object > Highest_object_index)
		return;

	if (hit_obj_type)
		*hit_obj_type = Objects[hit_object].type;
	if (hit_obj_id)
		*hit_obj_id = Objects[hit_object].id;
	if (hit_obj_sig)
		*hit_obj_sig = Objects[hit_object].signature;
	if (hit_obj_seg)
		*hit_obj_seg = Objects[hit_object].segnum;
}

static int input_demo_trace_ai_visibility_fvi_active(object *objp)
{
	return input_demo_trace_ai_visibility_active(objp) &&
		((int)(objp - Objects) == 151);
}

void input_demo_log_ai_visibility_probe(object *objp, const char *step_label,
	int previous_visibility, int raw_player_visibility,
	int final_player_visibility, int sight_sound_gate,
	int attack_sound_gate, int misc_sound_gate, const vms_vector *pos,
	const vms_vector *believed_player_pos)
{
	const int objnum = objp ? (int)(objp - Objects) : -1;
	ai_static *aip;
	ai_local *ailp;
	char probe[640];

	if (!input_demo_trace_ai_visibility_active(objp) || (objnum < 0) || !pos ||
		!believed_player_pos)
		return;

	aip = &objp->ctype.ai_info;
	ailp = &Ai_local_info[objnum];
	snprintf(probe, sizeof(probe),
		"step=%s prev_vis=%d raw_vis=%d final_vis=%d behavior=%d mode=%d cur_state=%d goal_state=%d gun=%d player_seg=%d believed_seg=%d goal_seg=%d aware=%d aware_time=%d next_fire=%d next_fire2=%d next_misc=%lld seen=%lld sound_gates=%d/%d/%d pos=(%d,%d,%d) believed=(%d,%d,%d)",
		step_label ? step_label : "unset",
		previous_visibility,
		raw_player_visibility,
		final_player_visibility,
		aip->behavior,
		ailp->mode,
		aip->CURRENT_STATE,
		aip->GOAL_STATE,
		aip->CURRENT_GUN,
		ConsoleObject ? ConsoleObject->segnum : -1,
		ConsoleObject ? ConsoleObject->segnum : -1,
		ailp->goal_segment,
		ailp->player_awareness_type,
		ailp->player_awareness_time,
		ailp->next_fire,
		-1,
		(long long)ailp->next_misc_sound_time,
		(long long)ailp->time_player_seen,
		sight_sound_gate,
		attack_sound_gate,
		misc_sound_gate,
		pos->x,
		pos->y,
		pos->z,
		believed_player_pos->x,
		believed_player_pos->y,
		believed_player_pos->z);
	input_demo_append_replay_probe_message_d1("probe_ai_visibility", objp, probe);
	con_printf(CON_NORMAL,
		"Input demo AI visibility: mode=%s frame=%u gt=%lld step=%s obj=%d sig=%d id=%d seg=%d prev_vis=%d raw_vis=%d final_vis=%d behavior=%d mode_ai=%d cur_state=%d goal_state=%d gun=%d aware=%d aware_time=%d next_fire=%d next_fire2=%d next_misc=%lld seen=%lld sound_gates=%d/%d/%d pos=(%d,%d,%d) believed=(%d,%d,%d)\n",
		input_demo_debug_activity_mode_name(),
		input_demo_debug_frame_index(),
		(long long)GameTime64,
		step_label ? step_label : "unset",
		objnum,
		objp->signature,
		objp->id,
		objp->segnum,
		previous_visibility,
		raw_player_visibility,
		final_player_visibility,
		aip->behavior,
		ailp->mode,
		aip->CURRENT_STATE,
		aip->GOAL_STATE,
		aip->CURRENT_GUN,
		ailp->player_awareness_type,
		ailp->player_awareness_time,
		ailp->next_fire,
		-1,
		(long long)ailp->next_misc_sound_time,
		(long long)ailp->time_player_seen,
		sight_sound_gate,
		attack_sound_gate,
		misc_sound_gate,
		pos->x,
		pos->y,
		pos->z,
		believed_player_pos->x,
		believed_player_pos->y,
		believed_player_pos->z);
}

void input_demo_log_ai_visibility_fvi_probe(object *objp,
	const char *step_label, int visibility_result, int hit_type,
	int hit_seg, int hit_object, int startseg, int flags, int32_t dot,
	int32_t field_of_view, const vms_vector *hit_pos, const vms_vector *pos,
	const vms_vector *believed_player_pos)
{
	const int objnum = objp ? (int)(objp - Objects) : -1;
	int hit_obj_type;
	int hit_obj_id;
	int hit_obj_sig;
	int hit_obj_seg;
	char probe[640];

	if (!input_demo_trace_ai_visibility_fvi_active(objp) || (objnum < 0) || !pos ||
		!believed_player_pos)
		return;

	input_demo_hit_object_details(hit_object, &hit_obj_type, &hit_obj_id,
		&hit_obj_sig, &hit_obj_seg);
	snprintf(probe, sizeof(probe),
		"step=%s result=%d hit=%d hit_seg=%d hit_obj=%d/%d/%d/%d/%d startseg=%d flags=0x%x dot=%d fov=%d agitation=%d pos=(%d,%d,%d) believed=(%d,%d,%d) hit_pos=(%d,%d,%d)",
		step_label ? step_label : "unset",
		visibility_result,
		hit_type,
		hit_seg,
		hit_object,
		hit_obj_type,
		hit_obj_id,
		hit_obj_sig,
		hit_obj_seg,
		startseg,
		flags,
		dot,
		field_of_view,
		Overall_agitation,
		pos->x,
		pos->y,
		pos->z,
		believed_player_pos->x,
		believed_player_pos->y,
		believed_player_pos->z,
		hit_pos ? hit_pos->x : 0,
		hit_pos ? hit_pos->y : 0,
		hit_pos ? hit_pos->z : 0);
	input_demo_append_replay_probe_message_d1("probe_ai_visibility_fvi", objp,
		probe);
	con_printf(CON_NORMAL,
		"Input demo AI visibility FVI: mode=%s frame=%u gt=%lld step=%s obj=%d sig=%d id=%d seg=%d result=%d hit=%d hit_seg=%d hit_obj=%d/%d/%d/%d/%d startseg=%d flags=0x%x dot=%d fov=%d agitation=%d pos=(%d,%d,%d) believed=(%d,%d,%d) hit_pos=(%d,%d,%d)\n",
		input_demo_debug_activity_mode_name(),
		input_demo_debug_frame_index(),
		(long long)GameTime64,
		step_label ? step_label : "unset",
		objnum,
		objp->signature,
		objp->id,
		objp->segnum,
		visibility_result,
		hit_type,
		hit_seg,
		hit_object,
		hit_obj_type,
		hit_obj_id,
		hit_obj_sig,
		hit_obj_seg,
		startseg,
		flags,
		dot,
		field_of_view,
		Overall_agitation,
		pos->x,
		pos->y,
		pos->z,
		believed_player_pos->x,
		believed_player_pos->y,
		believed_player_pos->z,
		hit_pos ? hit_pos->x : 0,
		hit_pos ? hit_pos->y : 0,
		hit_pos ? hit_pos->z : 0);
}

const char *input_demo_current_mission_id(void)
{
	if (Current_mission && Current_mission_filename && Current_mission_filename[0])
		return Current_mission_filename;
	return "d1";
}

static int input_demo_replay_logged_state_mismatch = 0;
static int input_demo_replay_logged_state_trace_error = 0;
static fix64 input_demo_replay_last_timer_value = 0;
static unsigned int input_demo_replay_result_frame_count_override = 0;

void input_demo_log_current_replay_frame_state_mismatch(void)
{
	input_demo_replay_frame replay_frame;
	char error[256] = "";

	if (!input_demo_replay_is_loaded())
		return;
	if (!input_demo_replay_get_current_frame(&replay_frame, error, sizeof(error)))
		return;
	input_demo_write_replay_frame_state_trace_shared(
		&replay_frame,
		&input_demo_replay_logged_state_trace_error);
	input_demo_log_replay_frame_state_mismatch_shared(
		&replay_frame,
		&input_demo_replay_logged_state_mismatch);
}

static void input_demo_write_replay_result(void)
{
	input_demo_result result;
	char error[256] = "";
	const char *result_path;

	if (!input_demo_replay_is_loaded())
		return;
	result_path = input_demo_replay_actual_result_path();
	if (!(result_path && result_path[0]))
		return;
	input_demo_capture_current_result(&result);
	if (input_demo_replay_result_frame_count_override)
		result.frame_count = input_demo_replay_result_frame_count_override;
	input_demo_replay_result_frame_count_override = 0;
	if (!input_demo_result_write_json_file(result_path, &result, error, sizeof(error)))
		con_printf(CON_NORMAL, "Input demo replay result write failed: %s\n", error);
	else {
		input_demo_debug_printf("Input demo replay result written: %s\n", result_path);
		if (!input_demo_replay_compare_result(&result, error, sizeof(error)))
			con_printf(CON_NORMAL, "Input demo replay result mismatch: %s\n", error);
		else
			con_printf(CON_NORMAL, "Input demo replay result matched embedded trailer\n");
	}
}

void input_demo_finish_replay_without_close(void)
{
	(void)input_demo_finish_replay_shared(0,
		&input_demo_replay_last_timer_value,
		NULL,
		input_demo_write_replay_result);
}

static void input_demo_prepare_finish_replay_from_game_over(void)
{
	input_demo_replay_result_frame_count_override = input_demo_replay_frame_count();
}

int input_demo_finish_replay_from_game_over(void)
{
	return input_demo_finish_replay_shared(0,
		&input_demo_replay_last_timer_value,
		input_demo_prepare_finish_replay_from_game_over,
		input_demo_write_replay_result);
}

static void input_demo_stop_replay(int write_result)
{
	input_demo_stop_replay_shared(write_result,
		&input_demo_replay_last_timer_value,
		input_demo_write_replay_result,
		NULL);
}

static int input_demo_sync_replay_rng_to_current_frame(void)
{
	return input_demo_sync_replay_rng_to_current_frame_shared(
		NULL,
		input_demo_stop_replay);
}

int input_demo_prepare_replay_frame(void)
{
	return input_demo_prepare_replay_frame_shared(
		&input_demo_replay_logged_state_mismatch,
		&input_demo_replay_logged_state_trace_error,
		&input_demo_replay_last_timer_value,
		NULL,
		input_demo_stop_replay);
}

int input_demo_step_replay_frame(void)
{
	return input_demo_step_replay_frame_shared(
		input_demo_prepare_replay_frame,
		input_demo_sync_replay_rng_to_current_frame);
}

void input_demo_advance_replay_frame(void)
{
	input_demo_advance_replay_frame_shared(NULL, input_demo_stop_replay);
}

int input_demo_finish_replay_from_mine_exit(void)
{
	return input_demo_finish_replay_shared(1,
		&input_demo_replay_last_timer_value,
		NULL,
		input_demo_write_replay_result);
}
