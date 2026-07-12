#ifndef CLASSIC_DEMO_JSON_H
#define CLASSIC_DEMO_JSON_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef struct classic_demo_json_vector {
	int x;
	int y;
	int z;
} classic_demo_json_vector;

typedef struct classic_demo_json_matrix {
	classic_demo_json_vector fvec;
	classic_demo_json_vector rvec;
	classic_demo_json_vector uvec;
} classic_demo_json_matrix;

typedef struct classic_demo_json_writer {
	void *context;
	int (*write)(void *context, const char *data, size_t size);
	int failed;
} classic_demo_json_writer;

typedef struct classic_demo_json_header {
	int version;
	int game_type;
	const char *mission;
	int score;
	int primary_weapon;
	int secondary_weapon;
	int player_flags;
	int energy;
	int shields;
} classic_demo_json_header;

typedef struct classic_demo_json_control {
	int valid;
	int segnum;
	int phys_flags;
	int player_flags;
	int frame_time;
	int resolved_forward_thrust_time;
	int control_pitch;
	int control_heading;
	int control_bank;
	int control_forward;
	int control_sideways;
	int control_vertical;
	int afterburner_state;
	int afterburner_charge;
	int wiggle_applied;
	int raw_swiggle;
	int scaled_swiggle;
	int wiggle_amount;
	int ship_wiggle;
	classic_demo_json_vector pre_scale_thrust;
	classic_demo_json_vector thrust;
	classic_demo_json_vector pre_scale_rotthrust;
	classic_demo_json_vector rotthrust;
	classic_demo_json_vector velocity_before_wiggle;
	classic_demo_json_vector wiggle_delta;
	classic_demo_json_vector velocity_after_wiggle;
} classic_demo_json_control;

typedef struct classic_demo_json_wiggle {
	int valid;
	int applied;
	int segnum;
	int phys_flags;
	int frame_time;
	int raw_swiggle;
	int scaled_swiggle;
	int wiggle_amount;
	classic_demo_json_vector velocity_before;
	classic_demo_json_vector wiggle_delta;
	classic_demo_json_vector velocity_after;
	classic_demo_json_vector uvec;
} classic_demo_json_wiggle;

typedef struct classic_demo_json_player {
	int objnum;
	int score;
	int energy;
	int shields;
	int flags;
	int segnum;
	int phys_flags;
	classic_demo_json_vector position;
	classic_demo_json_vector last_position;
	classic_demo_json_vector velocity;
	classic_demo_json_matrix orientation;
	classic_demo_json_control control;
	classic_demo_json_wiggle wiggle;
} classic_demo_json_player;

typedef struct classic_demo_json_robot_ai {
	int companion;
	int behavior;
	int mode;
	int current_state;
	int goal_state;
	int current_gun;
	int path_direction;
	int goal_side;
	int danger_object;
	int danger_signature;
	int player_segment;
	int believed_segment;
	int goal_segment;
	int previous_visibility;
	int awareness_type;
	int awareness_time;
	int64_t time_player_seen;
	int time_since_processed;
	int next_action_time;
	int next_fire;
	int next_fire2;
	int path_index;
	int path_length;
	int hide_index;
	int skip_ai_count;
} classic_demo_json_robot_ai;

typedef struct classic_demo_json_object {
	int objnum;
	int signature;
	int type;
	int id;
	int segnum;
	int flags;
	int size;
	int shields;
	int lifeleft;
	int control_type;
	int movement_type;
	int render_type;
	int viewer;
	classic_demo_json_vector position;
	classic_demo_json_vector last_position;
	int has_physics;
	int phys_flags;
	classic_demo_json_vector velocity;
	int has_robot_ai;
	classic_demo_json_robot_ai robot_ai;
	classic_demo_json_matrix orientation;
} classic_demo_json_object;

typedef struct classic_demo_json_frame {
	int frame_number;
	int frame_time;
	int game_time;
	int level;
	int viewer_objnum;
	classic_demo_json_player player;
	const classic_demo_json_object *objects;
	size_t object_count;
} classic_demo_json_frame;

typedef struct classic_demo_json_robot_damage {
	int frame_number;
	int game_time;
	int objnum;
	int signature;
	int id;
	int size;
	int damage;
	int shields_before;
	int shields_after;
	int dead;
	classic_demo_json_vector position;
	int has_velocity;
	classic_demo_json_vector velocity;
} classic_demo_json_robot_damage;

typedef struct classic_demo_json_result {
	int frames_decoded;
	int objects_emitted;
	int truncated;
} classic_demo_json_result;

void classic_demo_json_writer_init(classic_demo_json_writer *writer,
                                   int (*write)(void *context, const char *data, size_t size), void *context);
void classic_demo_json_writer_init_file(classic_demo_json_writer *writer, FILE *file);
int classic_demo_json_write_header(classic_demo_json_writer *writer,
                                   const classic_demo_json_header *header);
int classic_demo_json_write_frame(classic_demo_json_writer *writer,
                                  const classic_demo_json_frame *frame);
int classic_demo_json_write_robot_damage(classic_demo_json_writer *writer,
                                         const classic_demo_json_robot_damage *damage);
int classic_demo_json_write_result(classic_demo_json_writer *writer,
                                   const classic_demo_json_result *result);

#endif
