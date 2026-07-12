#include "classic_demo_json_d2_snapshot.h"

#include <string.h>

#include "ai.h"
#include "game.h"
#include "gameseq.h"
#include "inferno.h"
#include "mission.h"
#include "object.h"
#include "player.h"

static classic_demo_json_vector snapshot_vector(const vms_vector *vector)
{
	classic_demo_json_vector snapshot = { vector->x, vector->y, vector->z };

	return snapshot;
}

static classic_demo_json_matrix snapshot_matrix(const vms_matrix *matrix)
{
	classic_demo_json_matrix snapshot;

	snapshot.fvec = snapshot_vector(&matrix->fvec);
	snapshot.rvec = snapshot_vector(&matrix->rvec);
	snapshot.uvec = snapshot_vector(&matrix->uvec);
	return snapshot;
}

void classic_demo_json_d2_snapshot_header(classic_demo_json_header *snapshot,
                                          int version, int game_type)
{
	snapshot->version = version;
	snapshot->game_type = game_type;
	snapshot->mission = Current_mission_filename;
	snapshot->score = Players[Player_num].score;
	snapshot->primary_weapon = Players[Player_num].primary_weapon;
	snapshot->secondary_weapon = Players[Player_num].secondary_weapon;
	snapshot->player_flags = Players[Player_num].flags;
	snapshot->energy = f2ir(Players[Player_num].energy);
	snapshot->shields = f2ir(Players[Player_num].shields);
}

void classic_demo_json_d2_snapshot_robot_damage(
    classic_demo_json_robot_damage *snapshot,
    int frame_number, int game_time, const struct object *robot,
    int old_shields, int damage)
{
	memset(snapshot, 0, sizeof(*snapshot));
	snapshot->frame_number = frame_number;
	snapshot->game_time = game_time;
	snapshot->objnum = (int) (robot - Objects);
	snapshot->signature = robot->signature;
	snapshot->id = robot->id;
	snapshot->size = robot->size;
	snapshot->damage = damage;
	snapshot->shields_before = old_shields;
	snapshot->shields_after = robot->shields;
	snapshot->dead = robot->shields < 0;
	snapshot->position = snapshot_vector(&robot->pos);
	snapshot->has_velocity = robot->movement_type == MT_PHYSICS;
	if (snapshot->has_velocity)
		snapshot->velocity = snapshot_vector(&robot->mtype.phys_info.velocity);
}

static void snapshot_object(classic_demo_json_object *snapshot,
                            int objnum, object *object)
{
	memset(snapshot, 0, sizeof(*snapshot));
	snapshot->objnum = objnum;
	snapshot->signature = object->signature;
	snapshot->type = object->type;
	snapshot->id = object->id;
	snapshot->segnum = object->segnum;
	snapshot->flags = object->flags;
	snapshot->size = object->size;
	snapshot->shields = object->shields;
	snapshot->lifeleft = object->lifeleft;
	snapshot->control_type = object->control_type;
	snapshot->movement_type = object->movement_type;
	snapshot->render_type = object->render_type;
	snapshot->viewer = object == Viewer;
	snapshot->position = snapshot_vector(&object->pos);
	snapshot->last_position = snapshot_vector(&object->last_pos);
	snapshot->has_physics = object->movement_type == MT_PHYSICS;
	if (snapshot->has_physics) {
		snapshot->phys_flags = object->mtype.phys_info.flags;
		snapshot->velocity = snapshot_vector(&object->mtype.phys_info.velocity);
	}
	if (object->type == OBJ_ROBOT) {
		const ai_static *ai = &object->ctype.ai_info;
		const ai_local *local = &Ai_local_info[objnum];
		classic_demo_json_robot_ai *robot = &snapshot->robot_ai;

		snapshot->has_robot_ai = 1;
		robot->companion = Robot_info[object->id].companion;
		robot->behavior = ai->behavior;
		robot->mode = local->mode;
		robot->current_state = ai->CURRENT_STATE;
		robot->goal_state = ai->GOAL_STATE;
		robot->current_gun = ai->CURRENT_GUN;
		robot->path_direction = ai->PATH_DIR;
		robot->goal_side = ai->GOALSIDE;
		robot->danger_object = ai->danger_laser_num;
		robot->danger_signature = ai->danger_laser_signature;
		robot->player_segment = ConsoleObject ? ConsoleObject->segnum : -1;
		robot->believed_segment = Believed_player_seg;
		robot->goal_segment = local->goal_segment;
		robot->previous_visibility = local->previous_visibility;
		robot->awareness_type = local->player_awareness_type;
		robot->awareness_time = local->player_awareness_time;
		robot->time_player_seen = local->time_player_seen;
		robot->time_since_processed = local->time_since_processed;
		robot->next_action_time = local->next_action_time;
		robot->next_fire = local->next_fire;
		robot->next_fire2 = local->next_fire2;
		robot->path_index = ai->cur_path_index;
		robot->path_length = ai->path_length;
		robot->hide_index = ai->hide_index;
		robot->skip_ai_count = ai->SKIP_AI_COUNT;
	}
	snapshot->orientation = snapshot_matrix(&object->orient);
}

int classic_demo_json_d2_snapshot_frame(classic_demo_json_frame *snapshot,
                                        classic_demo_json_object *objects, size_t object_capacity,
                                        int frame_number, int frame_time, int game_time,
                                        const classic_demo_json_control *control,
                                        const classic_demo_json_wiggle *wiggle)
{
	const int player_objnum = Players[Player_num].objnum;
	object *player_object;
	int i;
	size_t count = 0;

	if (!snapshot || !objects || !control || !wiggle ||
	    player_objnum < 0 || player_objnum > Highest_object_index)
		return 0;
	player_object = &Objects[player_objnum];
	memset(snapshot, 0, sizeof(*snapshot));
	snapshot->frame_number = frame_number;
	snapshot->frame_time = frame_time;
	snapshot->game_time = game_time;
	snapshot->level = Current_level_num;
	snapshot->viewer_objnum = Viewer ? (int) (Viewer - Objects) : -1;
	snapshot->player.objnum = player_objnum;
	snapshot->player.score = Players[Player_num].score;
	snapshot->player.energy = f2ir(Players[Player_num].energy);
	snapshot->player.shields = f2ir(Players[Player_num].shields);
	snapshot->player.flags = Players[Player_num].flags;
	snapshot->player.segnum = player_object->segnum;
	snapshot->player.phys_flags = player_object->mtype.phys_info.flags;
	snapshot->player.position = snapshot_vector(&player_object->pos);
	snapshot->player.last_position = snapshot_vector(&player_object->last_pos);
	snapshot->player.velocity = snapshot_vector(&player_object->mtype.phys_info.velocity);
	snapshot->player.orientation = snapshot_matrix(&player_object->orient);
	snapshot->player.control = *control;
	snapshot->player.wiggle = *wiggle;
	for (i = 0; i <= Highest_object_index; ++i) {
		object *object = &Objects[i];

		if (object->type == OBJ_NONE || object->segnum == -1)
			continue;
		if (count == object_capacity)
			return 0;
		snapshot_object(&objects[count++], i, object);
	}
	snapshot->objects = objects;
	snapshot->object_count = count;
	return 1;
}
