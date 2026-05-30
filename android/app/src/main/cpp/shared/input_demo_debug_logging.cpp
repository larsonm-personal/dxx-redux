#include "input_demo_debug_logging.h"

#include <stdarg.h>
#include <stdio.h>

#include "input_demo_recorder.h"
#include "input_demo_replay.h"

extern "C" {
#include "console.h"
#include "fix.h"
#include "game.h"
#include "object.h"
#include "player.h"
#include "vecmat.h"
#ifdef DXX_BUILD_DESCENT_II
#include "input_demo_hooks.h"
#endif
}

#if INPUT_DEMO_DEBUG_LOGGING_AVAILABLE

static int g_input_demo_debug_enabled = 0;

int input_demo_debug_is_enabled(void)
{
	return g_input_demo_debug_enabled;
}

int input_demo_debug_record_probe_active(void)
{
	return g_input_demo_debug_enabled && input_demo_recorder_is_active();
}

int input_demo_debug_activity_probe_active(void)
{
	return g_input_demo_debug_enabled &&
	       (input_demo_recorder_is_active() || input_demo_replay_is_loaded());
}

int input_demo_debug_replay_probe_active(void)
{
	return g_input_demo_debug_enabled && input_demo_replay_is_loaded();
}

int input_demo_debug_frame_in_range(unsigned int start_frame, unsigned int end_frame)
{
	const unsigned int frame = input_demo_debug_frame_index();

	return (frame >= start_frame) && (frame <= end_frame);
}

int input_demo_debug_activity_frame_in_range(unsigned int start_frame, unsigned int end_frame)
{
	return input_demo_debug_activity_probe_active() &&
	       input_demo_debug_frame_in_range(start_frame, end_frame);
}

int input_demo_debug_replay_frame_in_range(unsigned int start_frame, unsigned int end_frame)
{
	return input_demo_debug_replay_probe_active() &&
	       input_demo_debug_frame_in_range(start_frame, end_frame);
}

void input_demo_debug_set_enabled(int enabled)
{
	g_input_demo_debug_enabled = enabled ? 1 : 0;
}

void input_demo_debug_printf(const char *fmt, ...)
{
	char buffer[2048];
	va_list args;

	if (!g_input_demo_debug_enabled || !fmt)
		return;

	va_start(args, fmt);
	if (vsnprintf(buffer, sizeof(buffer), fmt, args) < 0) {
		va_end(args);
		return;
	}
	va_end(args);
	buffer[sizeof(buffer) - 1] = 0;
	con_printf(0, "%s", buffer);
}

const char *input_demo_debug_activity_mode_name(void)
{
	if (input_demo_replay_is_loaded())
		return "replay";
	if (input_demo_recorder_is_active())
		return "record";
	return "none";
}

unsigned int input_demo_debug_frame_index(void)
{
	if (input_demo_replay_is_loaded())
		return (unsigned int) input_demo_replay_next_frame_index();
	if (input_demo_recorder_is_active()) {
		const uint32_t frame_count = input_demo_recorder_frame_count();

		return frame_count ? (unsigned int) (frame_count - 1) : 0;
	}
	return 0;
}

static int input_demo_debug_exploding_object_near_player(object *obj)
{
	if (!obj || !ConsoleObject)
		return 0;

	if (obj->segnum == ConsoleObject->segnum)
		return 1;

	return vm_vec_dist_quick(&obj->pos, &ConsoleObject->pos) <
	       obj->size + ConsoleObject->size + F1_0 * 20;
}

static int input_demo_debug_exploding_object_probe_active(object *obj)
{
	if (!input_demo_debug_activity_probe_active() || !obj || !ConsoleObject)
		return 0;

	if (obj->type == OBJ_ROBOT)
		return input_demo_debug_exploding_object_near_player(obj);

#ifdef DXX_BUILD_DESCENT_II
	if (obj->type == OBJ_CLUTTER || obj->type == OBJ_CNTRLCEN ||
	    obj->type == OBJ_PLAYER)
		return input_demo_debug_exploding_object_near_player(obj);
#endif

	return 0;
}

void input_demo_debug_log_player_motion_state(const char *stage)
{
	if (stage)
		input_demo_debug_printf("Input demo player motion: stage=%s\n", stage);
}

void input_demo_debug_log_warning_probe(const char *label, void *obj, int view_x, int view_y, int view_z, int near_center, int prev_danger_obj, int prev_danger_sig)
{
	(void) obj;
	(void) view_x;
	(void) view_y;
	(void) view_z;
	(void) near_center;
	(void) prev_danger_obj;
	(void) prev_danger_sig;
	if (label)
		input_demo_debug_printf("Input demo warning probe: step=%s\n", label);
}

void input_demo_debug_log_replay_frame_state_mismatch(const void *replay_frame)
{
	(void) replay_frame;
	input_demo_debug_printf("Input demo replay state mismatch\n");
}

void input_demo_debug_log_result_state(const char *label)
{
	if (label)
		input_demo_debug_printf("Input demo result state: label=%s\n", label);
}

void input_demo_debug_log_replay_fire_state(const char *label, int can_fire_laser)
{
	(void) can_fire_laser;
	if (label)
		input_demo_debug_printf("Input demo fire state: label=%s\n", label);
}

void input_demo_debug_log_replay_energy_stage(const char *label)
{
	if (label)
		input_demo_debug_printf("Input demo energy stage: label=%s\n", label);
}

void input_demo_debug_write_replay_frame_state_trace(const void *replay_frame)
{
	(void) replay_frame;
	input_demo_debug_printf("Input demo state trace\n");
}

void input_demo_debug_log_player_bump_probe(const char *step, void *obj0, void *obj1, const void *relative_velocity, const void *force, int scale_num, int scale_den, int damage_flag)
{
	(void) obj0;
	(void) obj1;
	(void) relative_velocity;
	(void) force;
	(void) scale_num;
	(void) scale_den;
	(void) damage_flag;
	if (step)
		input_demo_debug_printf("Input demo bump probe: step=%s\n", step);
}

void input_demo_debug_log_player_robot_contact_probe(const char *step, void *playerobj, void *robot, const void *collision_point, int damage)
{
	(void) playerobj;
	(void) robot;
	(void) collision_point;
	(void) damage;
	if (step)
		input_demo_debug_printf("Input demo robot contact: step=%s\n", step);
}

void input_demo_debug_log_weapon_robot_accept_seq(void *weapon, void *robot)
{
	(void) weapon;
	(void) robot;
	input_demo_debug_printf("Input demo weapon accept\n");
}

void input_demo_debug_log_weapon_robot_path_probe(const char *step, void *weapon, void *robot, const void *collision_point)
{
	(void) weapon;
	(void) robot;
	(void) collision_point;
	if (step)
		input_demo_debug_printf("Input demo weapon path: step=%s\n", step);
}

void input_demo_debug_log_weapon_robot_reason_probe(const char *step, void *weapon, void *robot, const void *collision_point)
{
	(void) weapon;
	(void) robot;
	(void) collision_point;
	if (step)
		input_demo_debug_printf("Input demo weapon reason: step=%s\n", step);
}

void input_demo_debug_log_weapon_robot_collision_pose(const char *step, void *weapon, void *robot, void *collision_point, int damage, int robot_old_shields)
{
	(void) weapon;
	(void) robot;
	(void) collision_point;
	(void) damage;
	(void) robot_old_shields;
	if (step)
		input_demo_debug_printf("Input demo collision pose: step=%s\n", step);
}

void input_demo_debug_log_weapon_robot_dispatch_probe(void *weapon, void *robot, const void *collision_point)
{
	(void) weapon;
	(void) robot;
	(void) collision_point;
	input_demo_debug_printf("Input demo dispatch probe\n");
}

void input_demo_debug_log_ai_robot_state(const char *label, void *objp)
{
	(void) objp;
	if (label)
		input_demo_debug_printf("Input demo ai state: label=%s\n", label);
}

void input_demo_debug_log_fvi_weapon_robot_check(int p0, int p1, int thisobjnum, int objnum, int fudged_rad, int d)
{
	(void) p0;
	(void) p1;
	(void) thisobjnum;
	(void) objnum;
	(void) fudged_rad;
	(void) d;
	input_demo_debug_printf("Input demo fvi check\n");
}

void input_demo_debug_log_exploding_object_probe(const char *step, void *obj, int delay_time, int spawned_objnum)
{
	object *source_obj = (object *) obj;
	fix player_dist;
	const char *step_label = step ? step : "";

	if (!input_demo_debug_exploding_object_probe_active(source_obj))
		return;

	player_dist = vm_vec_dist_quick(&source_obj->pos, &ConsoleObject->pos);

#ifdef DXX_BUILD_DESCENT_II
	{
		char probe[512];
		object *spawned_obj = NULL;

		if (spawned_objnum >= 0 && spawned_objnum <= Highest_object_index)
			spawned_obj = &Objects[spawned_objnum];

		snprintf(probe, sizeof(probe),
		         "step=%s source=%d/%d/%d/%d/%d flags=0x%x shields=%d life=%d delay=%d spawned=%d/%d/%d/%d/%d player_seg=%d player_dist=%d",
		         step_label,
		         (int) (source_obj - Objects),
		         source_obj->signature,
		         source_obj->type,
		         source_obj->id,
		         source_obj->segnum,
		         source_obj->flags,
		         source_obj->shields,
		         source_obj->lifeleft,
		         delay_time,
		         spawned_obj ? (int) (spawned_obj - Objects) : spawned_objnum,
		         spawned_obj ? spawned_obj->signature : -1,
		         spawned_obj ? spawned_obj->type : -1,
		         spawned_obj ? spawned_obj->id : -1,
		         spawned_obj ? spawned_obj->segnum : -1,
		         ConsoleObject->segnum,
		         player_dist);
		input_demo_append_replay_probe_message("explode_object", spawned_obj ? spawned_obj : source_obj,
		                                       probe);
	}
#endif

	con_printf(CON_NORMAL,
	           "Input demo exploding object probe: mode=%s frame=%u gt=%lld step=%s obj=%d/%d/%d sig=%d seg=%d pos=(%d,%d,%d) last=(%d,%d,%d) vel=(%d,%d,%d) shields=%d size=%d life=%d flags=0x%x ctype=%d mtype=%d rtype=%d delay=%d spawned=%d player_seg=%d player_dist=%d player_pos=(%d,%d,%d) player_vel=(%d,%d,%d) player_shields=%d\n",
	           input_demo_debug_activity_mode_name(),
	           input_demo_debug_frame_index(),
	           (long long) GameTime64,
	           step_label,
	           (int) (source_obj - Objects),
	           source_obj->type,
	           source_obj->id,
	           source_obj->signature,
	           source_obj->segnum,
	           source_obj->pos.x,
	           source_obj->pos.y,
	           source_obj->pos.z,
	           source_obj->last_pos.x,
	           source_obj->last_pos.y,
	           source_obj->last_pos.z,
	           source_obj->mtype.phys_info.velocity.x,
	           source_obj->mtype.phys_info.velocity.y,
	           source_obj->mtype.phys_info.velocity.z,
	           source_obj->shields,
	           source_obj->size,
	           source_obj->lifeleft,
	           source_obj->flags,
	           source_obj->control_type,
	           source_obj->movement_type,
	           source_obj->render_type,
	           delay_time,
	           spawned_objnum,
	           ConsoleObject->segnum,
	           player_dist,
	           ConsoleObject->pos.x,
	           ConsoleObject->pos.y,
	           ConsoleObject->pos.z,
	           ConsoleObject->mtype.phys_info.velocity.x,
	           ConsoleObject->mtype.phys_info.velocity.y,
	           ConsoleObject->mtype.phys_info.velocity.z,
	           Players[Player_num].shields);
}

#endif
