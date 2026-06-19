#include "input_demo_debug_logging.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

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
static unsigned int g_robot_watch_frame = (unsigned int) -1;
static unsigned int g_robot_watch_log_count = 0;
static int g_robot_watch_valid[MAX_OBJECTS];
static vms_vector g_robot_watch_pos[MAX_OBJECTS];
static vms_vector g_robot_watch_velocity[MAX_OBJECTS];
static int g_robot_watch_seg[MAX_OBJECTS];
static int g_robot_watch_shields[MAX_OBJECTS];
static int g_robot_watch_flags[MAX_OBJECTS];
static int g_robot_watch_record_error_logged = 0;

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

static void input_demo_debug_record_frame_event_json(const char *json_text)
{
	char error[256] = "";

	if (!input_demo_recorder_is_active() || !json_text)
		return;
	if (!input_demo_recorder_append_frame_event_json(json_text, error, sizeof(error)) &&
	    !g_robot_watch_record_error_logged) {
		g_robot_watch_record_error_logged = 1;
		con_printf(CON_NORMAL, "Input demo recorder event append failed: %s\n", error);
	}
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
	if (stage) {
		input_demo_debug_printf("Input demo player motion: stage=%s\n", stage);
	}
}

static int input_demo_debug_robot_watch_near_player(const object *obj)
{
	vms_vector obj_pos;

	if (!obj || !ConsoleObject)
		return 0;
	if (obj->segnum == ConsoleObject->segnum)
		return 1;
	obj_pos = obj->pos;
	return vm_vec_dist_quick(&obj_pos, &ConsoleObject->pos) < F1_0 * 100;
}

static int input_demo_debug_robot_watch_source_interesting(const object *source)
{
	if (!source)
		return 0;
	if (source == ConsoleObject)
		return 1;
	if (source->type == OBJ_PLAYER)
		return 1;
	if (source->type == OBJ_ROBOT)
		return 1;
	return source->type == OBJ_WEAPON &&
	       source->ctype.laser_info.parent_type == OBJ_PLAYER;
}

void input_demo_debug_log_object_watch_after_slot(int moved_slot)
{
	const unsigned int frame = input_demo_debug_frame_index();
	object *source = NULL;
	int objnum;

	if (!input_demo_debug_activity_probe_active())
		return;
	if (moved_slot >= 0 && moved_slot <= Highest_object_index)
		source = &Objects[moved_slot];
	if (g_robot_watch_frame != frame) {
		memset(g_robot_watch_valid, 0, sizeof(g_robot_watch_valid));
		g_robot_watch_frame = frame;
		g_robot_watch_log_count = 0;
	}
	if (g_robot_watch_log_count >= 96)
		return;

	for (objnum = 0; objnum <= Highest_object_index; ++objnum) {
		object *watch_obj = &Objects[objnum];
		const int changed =
		    g_robot_watch_valid[objnum] &&
		    (g_robot_watch_velocity[objnum].x !=
		         watch_obj->mtype.phys_info.velocity.x ||
		     g_robot_watch_velocity[objnum].y !=
		         watch_obj->mtype.phys_info.velocity.y ||
		     g_robot_watch_velocity[objnum].z !=
		         watch_obj->mtype.phys_info.velocity.z ||
		     g_robot_watch_pos[objnum].x != watch_obj->pos.x ||
		     g_robot_watch_pos[objnum].y != watch_obj->pos.y ||
		     g_robot_watch_pos[objnum].z != watch_obj->pos.z ||
		     g_robot_watch_seg[objnum] != watch_obj->segnum ||
		     g_robot_watch_shields[objnum] != watch_obj->shields ||
		     g_robot_watch_flags[objnum] != watch_obj->flags);

		if (watch_obj->type != OBJ_ROBOT ||
		    (watch_obj->flags & OF_SHOULD_BE_DEAD)) {
			g_robot_watch_valid[objnum] = 0;
			continue;
		}
		if (changed &&
		    (input_demo_debug_robot_watch_near_player(watch_obj) ||
		     input_demo_debug_robot_watch_source_interesting(source))) {
			char json[1200];

			snprintf(
			    json,
			    sizeof(json),
			    "{\"kind\":\"robot_watch_delta\",\"gt\":%lld,\"after_slot\":%d,\"source_type\":%d,\"source_id\":%d,\"source_sig\":%d,\"source_seg\":%d,\"robot_obj\":%d,\"robot_id\":%d,\"robot_sig\":%d,\"seg\":%d,\"old_seg\":%d,\"x\":%d,\"y\":%d,\"z\":%d,\"old_x\":%d,\"old_y\":%d,\"old_z\":%d,\"vx\":%d,\"vy\":%d,\"vz\":%d,\"old_vx\":%d,\"old_vy\":%d,\"old_vz\":%d,\"flags\":%d,\"old_flags\":%d,\"shields\":%d,\"old_shields\":%d}",
			    (long long) GameTime64,
			    moved_slot,
			    source ? source->type : -1,
			    source ? source->id : -1,
			    source ? source->signature : -1,
			    source ? source->segnum : -1,
			    objnum,
			    watch_obj->id,
			    watch_obj->signature,
			    watch_obj->segnum,
			    g_robot_watch_seg[objnum],
			    watch_obj->pos.x,
			    watch_obj->pos.y,
			    watch_obj->pos.z,
			    g_robot_watch_pos[objnum].x,
			    g_robot_watch_pos[objnum].y,
			    g_robot_watch_pos[objnum].z,
			    watch_obj->mtype.phys_info.velocity.x,
			    watch_obj->mtype.phys_info.velocity.y,
			    watch_obj->mtype.phys_info.velocity.z,
			    g_robot_watch_velocity[objnum].x,
			    g_robot_watch_velocity[objnum].y,
			    g_robot_watch_velocity[objnum].z,
			    watch_obj->flags,
			    g_robot_watch_flags[objnum],
			    watch_obj->shields,
			    g_robot_watch_shields[objnum]);
			input_demo_debug_record_frame_event_json(json);
			input_demo_debug_printf(
			    "Input demo robot watch delta: mode=%s frame=%u gt=%lld after_slot=%d source=%d/%d/%d sig=%d robot=%d/%d sig=%d seg=%d old_seg=%d pos=(%d,%d,%d) old_pos=(%d,%d,%d) vel=(%d,%d,%d) old_vel=(%d,%d,%d) flags=0x%x old_flags=0x%x shields=%d old_shields=%d player_seg=%d player_pos=(%d,%d,%d)\n",
			    input_demo_debug_activity_mode_name(),
			    frame,
			    (long long) GameTime64,
			    moved_slot,
			    source ? source->type : -1,
			    source ? source->id : -1,
			    source ? source->segnum : -1,
			    source ? source->signature : -1,
			    objnum,
			    watch_obj->id,
			    watch_obj->signature,
			    watch_obj->segnum,
			    g_robot_watch_seg[objnum],
			    watch_obj->pos.x,
			    watch_obj->pos.y,
			    watch_obj->pos.z,
			    g_robot_watch_pos[objnum].x,
			    g_robot_watch_pos[objnum].y,
			    g_robot_watch_pos[objnum].z,
			    watch_obj->mtype.phys_info.velocity.x,
			    watch_obj->mtype.phys_info.velocity.y,
			    watch_obj->mtype.phys_info.velocity.z,
			    g_robot_watch_velocity[objnum].x,
			    g_robot_watch_velocity[objnum].y,
			    g_robot_watch_velocity[objnum].z,
			    watch_obj->flags,
			    g_robot_watch_flags[objnum],
			    watch_obj->shields,
			    g_robot_watch_shields[objnum],
			    ConsoleObject ? ConsoleObject->segnum : -1,
			    ConsoleObject ? ConsoleObject->pos.x : 0,
			    ConsoleObject ? ConsoleObject->pos.y : 0,
			    ConsoleObject ? ConsoleObject->pos.z : 0);
			g_robot_watch_log_count++;
			if (g_robot_watch_log_count >= 96)
				return;
		}

		g_robot_watch_valid[objnum] = 1;
		g_robot_watch_pos[objnum] = watch_obj->pos;
		g_robot_watch_velocity[objnum] = watch_obj->mtype.phys_info.velocity;
		g_robot_watch_seg[objnum] = watch_obj->segnum;
		g_robot_watch_shields[objnum] = watch_obj->shields;
		g_robot_watch_flags[objnum] = watch_obj->flags;
	}
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
	object *weapon_obj = (object *) weapon;
	object *robot_obj = (object *) robot;

	if (!input_demo_debug_activity_probe_active() || !weapon_obj || !robot_obj)
		return;
	input_demo_debug_printf(
	    "Input demo weapon robot accept: mode=%s frame=%u gt=%lld weapon=%d/%d sig=%d seg=%d parent=%d/%d parent_sig=%d vel=(%d,%d,%d) robot=%d/%d sig=%d seg=%d shields=%d vel=(%d,%d,%d) flags=0x%x\n",
	    input_demo_debug_activity_mode_name(),
	    input_demo_debug_frame_index(),
	    (long long) GameTime64,
	    (int) (weapon_obj - Objects),
	    weapon_obj->id,
	    weapon_obj->signature,
	    weapon_obj->segnum,
	    weapon_obj->ctype.laser_info.parent_type,
	    weapon_obj->ctype.laser_info.parent_num,
	    weapon_obj->ctype.laser_info.parent_signature,
	    weapon_obj->mtype.phys_info.velocity.x,
	    weapon_obj->mtype.phys_info.velocity.y,
	    weapon_obj->mtype.phys_info.velocity.z,
	    (int) (robot_obj - Objects),
	    robot_obj->id,
	    robot_obj->signature,
	    robot_obj->segnum,
	    robot_obj->shields,
	    robot_obj->mtype.phys_info.velocity.x,
	    robot_obj->mtype.phys_info.velocity.y,
	    robot_obj->mtype.phys_info.velocity.z,
	    robot_obj->flags);
}

void input_demo_debug_log_weapon_robot_path_probe(const char *step, void *weapon, void *robot, const void *collision_point)
{
	object *weapon_obj = (object *) weapon;
	object *robot_obj = (object *) robot;
	const vms_vector *cp = (const vms_vector *) collision_point;

	if (!input_demo_debug_activity_probe_active() || !step || !weapon_obj || !robot_obj)
		return;
	input_demo_debug_printf(
	    "Input demo weapon robot path: mode=%s frame=%u gt=%lld step=%s weapon=%d/%d sig=%d seg=%d flags=0x%x life=%d parent=%d/%d parent_sig=%d last_hit=%d track_goal=%d vel=(%d,%d,%d) pos=(%d,%d,%d) robot=%d/%d sig=%d seg=%d flags=0x%x shields=%d vel=(%d,%d,%d) pos=(%d,%d,%d) cp=(%d,%d,%d)\n",
	    input_demo_debug_activity_mode_name(),
	    input_demo_debug_frame_index(),
	    (long long) GameTime64,
	    step,
	    (int) (weapon_obj - Objects),
	    weapon_obj->id,
	    weapon_obj->signature,
	    weapon_obj->segnum,
	    weapon_obj->flags,
	    weapon_obj->lifeleft,
	    weapon_obj->ctype.laser_info.parent_type,
	    weapon_obj->ctype.laser_info.parent_num,
	    weapon_obj->ctype.laser_info.parent_signature,
	    weapon_obj->ctype.laser_info.last_hitobj,
	    weapon_obj->ctype.laser_info.track_goal,
	    weapon_obj->mtype.phys_info.velocity.x,
	    weapon_obj->mtype.phys_info.velocity.y,
	    weapon_obj->mtype.phys_info.velocity.z,
	    weapon_obj->pos.x,
	    weapon_obj->pos.y,
	    weapon_obj->pos.z,
	    (int) (robot_obj - Objects),
	    robot_obj->id,
	    robot_obj->signature,
	    robot_obj->segnum,
	    robot_obj->flags,
	    robot_obj->shields,
	    robot_obj->mtype.phys_info.velocity.x,
	    robot_obj->mtype.phys_info.velocity.y,
	    robot_obj->mtype.phys_info.velocity.z,
	    robot_obj->pos.x,
	    robot_obj->pos.y,
	    robot_obj->pos.z,
	    cp ? cp->x : 0,
	    cp ? cp->y : 0,
	    cp ? cp->z : 0);
}

void input_demo_debug_log_weapon_robot_reason_probe(const char *step, void *weapon, void *robot, const void *collision_point)
{
	input_demo_debug_log_weapon_robot_path_probe(step, weapon, robot, collision_point);
}

void input_demo_debug_log_weapon_robot_collision_pose(const char *step, void *weapon, void *robot, void *collision_point, int damage, int robot_old_shields)
{
	object *robot_obj = (object *) robot;

	input_demo_debug_log_weapon_robot_path_probe(step, weapon, robot, collision_point);
	if (!input_demo_debug_activity_probe_active() || !step || !robot_obj)
		return;
	input_demo_debug_printf(
	    "Input demo weapon robot damage pose: mode=%s frame=%u gt=%lld step=%s damage=%d old_shields=%d new_shields=%d robot=%d sig=%d vel=(%d,%d,%d)\n",
	    input_demo_debug_activity_mode_name(),
	    input_demo_debug_frame_index(),
	    (long long) GameTime64,
	    step,
	    damage,
	    robot_old_shields,
	    robot_obj->shields,
	    (int) (robot_obj - Objects),
	    robot_obj->signature,
	    robot_obj->mtype.phys_info.velocity.x,
	    robot_obj->mtype.phys_info.velocity.y,
	    robot_obj->mtype.phys_info.velocity.z);
}

void input_demo_debug_log_weapon_robot_dispatch_probe(void *weapon, void *robot, const void *collision_point)
{
	input_demo_debug_log_weapon_robot_path_probe("dispatch", weapon, robot, collision_point);
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
