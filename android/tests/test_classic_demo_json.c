#include "classic_demo_json.h"

#include <stdio.h>
#include <string.h>

typedef struct capture_buffer {
	char data[16384];
	size_t size;
	size_t fail_after;
} capture_buffer;

static int capture_write(void *context, const char *data, size_t size)
{
	capture_buffer *capture = context;

	if (capture->size + size > sizeof(capture->data) ||
	    capture->size + size > capture->fail_after)
		return 0;
	memcpy(capture->data + capture->size, data, size);
	capture->size += size;
	capture->data[capture->size] = 0;
	return 1;
}

static classic_demo_json_vector vector(int x, int y, int z)
{
	classic_demo_json_vector result = {x, y, z};

	return result;
}

static classic_demo_json_matrix matrix(int start)
{
	classic_demo_json_matrix result;

	result.fvec = vector(start, start + 1, start + 2);
	result.rvec = vector(start + 3, start + 4, start + 5);
	result.uvec = vector(start + 6, start + 7, start + 8);
	return result;
}

static void fill_frame(classic_demo_json_frame *frame,
	classic_demo_json_object objects[2])
{
	classic_demo_json_control *control;
	classic_demo_json_robot_ai *ai;

	memset(frame, 0, sizeof(*frame));
	memset(objects, 0, sizeof(*objects) * 2);
	frame->frame_number = 9;
	frame->frame_time = 10;
	frame->game_time = 11;
	frame->level = -2;
	frame->viewer_objnum = 3;
	frame->player.objnum = 3;
	frame->player.score = 20;
	frame->player.energy = 21;
	frame->player.shields = 22;
	frame->player.flags = 23;
	frame->player.segnum = 24;
	frame->player.phys_flags = 25;
	frame->player.position = vector(1, 2, 3);
	frame->player.last_position = vector(4, 5, 6);
	frame->player.velocity = vector(-1, -2, -3);
	frame->player.orientation = matrix(10);
	control = &frame->player.control;
	control->valid = 1;
	control->segnum = 31;
	control->phys_flags = 32;
	control->player_flags = 33;
	control->frame_time = 34;
	control->resolved_forward_thrust_time = 35;
	control->control_pitch = 36;
	control->control_heading = 37;
	control->control_bank = 38;
	control->control_forward = 39;
	control->control_sideways = 40;
	control->control_vertical = 41;
	control->afterburner_state = 42;
	control->afterburner_charge = 43;
	control->wiggle_applied = 44;
	control->raw_swiggle = 45;
	control->scaled_swiggle = 46;
	control->wiggle_amount = 47;
	control->ship_wiggle = 48;
	control->pre_scale_thrust = vector(49, 50, 51);
	control->thrust = vector(52, 53, 54);
	control->pre_scale_rotthrust = vector(55, 56, 57);
	control->rotthrust = vector(58, 59, 60);
	control->velocity_before_wiggle = vector(61, 62, 63);
	control->wiggle_delta = vector(64, 65, 66);
	control->velocity_after_wiggle = vector(67, 68, 69);
	frame->player.wiggle.valid = 1;
	frame->player.wiggle.applied = 999;

	objects[0].objnum = 3;
	objects[0].signature = 100;
	objects[0].type = 4;
	objects[0].id = 5;
	objects[0].segnum = 6;
	objects[0].flags = 7;
	objects[0].size = 8;
	objects[0].shields = 9;
	objects[0].lifeleft = 10;
	objects[0].control_type = 11;
	objects[0].movement_type = 12;
	objects[0].render_type = 13;
	objects[0].viewer = 1;
	objects[0].position = vector(70, 71, 72);
	objects[0].last_position = vector(73, 74, 75);
	objects[0].orientation = matrix(80);

	objects[1].objnum = 4;
	objects[1].signature = -100;
	objects[1].type = 2;
	objects[1].id = 6;
	objects[1].segnum = 7;
	objects[1].flags = 8;
	objects[1].size = 9;
	objects[1].shields = -1;
	objects[1].lifeleft = 11;
	objects[1].control_type = 12;
	objects[1].movement_type = 13;
	objects[1].render_type = 14;
	objects[1].position = vector(90, 91, 92);
	objects[1].last_position = vector(93, 94, 95);
	objects[1].has_physics = 1;
	objects[1].phys_flags = 15;
	objects[1].velocity = vector(96, 97, 98);
	objects[1].has_robot_ai = 1;
	ai = &objects[1].robot_ai;
	ai->companion = 1;
	ai->behavior = 2;
	ai->mode = 3;
	ai->current_state = 4;
	ai->goal_state = 5;
	ai->current_gun = 6;
	ai->path_direction = 7;
	ai->goal_side = 8;
	ai->danger_object = 9;
	ai->danger_signature = 10;
	ai->player_segment = 11;
	ai->believed_segment = 12;
	ai->goal_segment = 13;
	ai->previous_visibility = 14;
	ai->awareness_type = 15;
	ai->awareness_time = 16;
	ai->time_player_seen = -17;
	ai->time_since_processed = 18;
	ai->next_action_time = 19;
	ai->next_fire = 20;
	ai->next_fire2 = 21;
	ai->path_index = 22;
	ai->path_length = 23;
	ai->hide_index = 24;
	ai->skip_ai_count = 25;
	objects[1].orientation = matrix(100);
	frame->objects = objects;
	frame->object_count = 2;
}

static int test_exact_output(void)
{
	static const char expected[] =
		"{\"type\":\"header\",\"format\":\"classic_dem_runtime_dump\",\"game\":\"d2\",\"version\":16,\"game_type\":3,\"mission\":\"mine\\\"\\\\\\n\\t\\u0001\",\"score\":-7,\"primary_weapon\":1,\"secondary_weapon\":2,\"player_flags\":3,\"energy\":4,\"shields\":5}\n"
		"{\"type\":\"frame\",\"f\":9,\"ft\":10,\"gt\":11,\"level\":-2,\"viewer_objnum\":3,\"object_count\":2,\"player\":{\"objnum\":3,\"score\":20,\"energy\":21,\"shields\":22,\"flags\":23,\"seg\":24,\"phys_flags\":25,\"pos\":[1,2,3],\"last_pos\":[4,5,6],\"vel\":[-1,-2,-3],\"orient\":{\"f\":[10,11,12],\"r\":[13,14,15],\"u\":[16,17,18]},\"control\":{\"valid\":true,\"seg\":31,\"phys_flags\":32,\"player_flags\":33,\"ft\":34,\"resolved_forward\":35,\"pitch\":36,\"heading\":37,\"bank\":38,\"forward\":39,\"sideways\":40,\"vertical\":41,\"afterburner_state\":42,\"afterburner_charge\":43,\"wiggle_applied\":44,\"wiggle_raw\":45,\"wiggle_scaled\":46,\"wiggle_amount\":47,\"ship_wiggle\":48,\"pre_thrust\":[49,50,51],\"thrust\":[52,53,54],\"pre_rot\":[55,56,57],\"rot\":[58,59,60],\"vel_before_wiggle\":[61,62,63],\"wiggle_delta\":[64,65,66],\"vel_after_wiggle\":[67,68,69]},\"wiggle\":{\"valid\":true,\"applied\":44,\"seg\":31,\"phys_flags\":32,\"ft\":34,\"raw\":45,\"scaled\":46,\"amount\":47,\"vel_before\":[61,62,63],\"delta\":[64,65,66],\"vel_after\":[67,68,69],\"uvec\":[16,17,18]}},\"objects\":[{\"objnum\":3,\"sig\":100,\"obj_type\":4,\"id\":5,\"seg\":6,\"flags\":7,\"size\":8,\"shields\":9,\"lifeleft\":10,\"control_type\":11,\"movement_type\":12,\"render_type\":13,\"viewer\":true,\"pos\":[70,71,72],\"last_pos\":[73,74,75],\"orient\":{\"f\":[80,81,82],\"r\":[83,84,85],\"u\":[86,87,88]}},{\"objnum\":4,\"sig\":-100,\"obj_type\":2,\"id\":6,\"seg\":7,\"flags\":8,\"size\":9,\"shields\":-1,\"lifeleft\":11,\"control_type\":12,\"movement_type\":13,\"render_type\":14,\"viewer\":false,\"pos\":[90,91,92],\"last_pos\":[93,94,95],\"phys_flags\":15,\"vel\":[96,97,98],\"robot_ai\":{\"companion\":1,\"behavior\":2,\"mode\":3,\"cur_state\":4,\"goal_state\":5,\"gun\":6,\"path_dir\":7,\"goal_side\":8,\"danger_obj\":9,\"danger_sig\":10,\"player_seg\":11,\"believed_seg\":12,\"goal_seg\":13,\"prev_vis\":14,\"aware\":15,\"aware_time\":16,\"seen\":-17,\"since\":18,\"next_action\":19,\"next_fire\":20,\"next_fire2\":21,\"path_index\":22,\"path_length\":23,\"hide\":24,\"skip\":25},\"orient\":{\"f\":[100,101,102],\"r\":[103,104,105],\"u\":[106,107,108]}}]}\n"
		"{\"type\":\"robot_damage\",\"f\":9,\"gt\":11,\"objnum\":4,\"sig\":-100,\"id\":6,\"size\":9,\"damage\":12,\"shields_before\":10,\"shields_after\":-2,\"dead\":true,\"pos\":[90,91,92],\"vel\":[96,97,98]}\n"
		"{\"type\":\"result\",\"frames_decoded\":1,\"objects_emitted\":2,\"truncated\":false}\n";
	capture_buffer capture = {{0}, 0, sizeof(capture.data) - 1};
	classic_demo_json_writer writer;
	classic_demo_json_header header = {16, 3, "mine\"\\\n\t\001", -7, 1, 2, 3, 4, 5};
	classic_demo_json_frame frame;
	classic_demo_json_object objects[2];
	classic_demo_json_robot_damage damage;
	classic_demo_json_result result = {1, 2, 0};

	fill_frame(&frame, objects);
	memset(&damage, 0, sizeof(damage));
	damage.frame_number = 9;
	damage.game_time = 11;
	damage.objnum = 4;
	damage.signature = -100;
	damage.id = 6;
	damage.size = 9;
	damage.damage = 12;
	damage.shields_before = 10;
	damage.shields_after = -2;
	damage.dead = 1;
	damage.position = vector(90, 91, 92);
	damage.has_velocity = 1;
	damage.velocity = vector(96, 97, 98);
	classic_demo_json_writer_init(&writer, capture_write, &capture);
	if (!classic_demo_json_write_header(&writer, &header) ||
	    !classic_demo_json_write_frame(&writer, &frame) ||
	    !classic_demo_json_write_robot_damage(&writer, &damage) ||
	    !classic_demo_json_write_result(&writer, &result) || writer.failed)
		return 0;
	if (strcmp(capture.data, expected)) {
		fprintf(stderr, "classic demo json mismatch\nexpected:\n%sactual:\n%s", expected, capture.data);
		return 0;
	}
	return 1;
}

static int test_standalone_wiggle(void)
{
	static const char expected[] =
		"{\"type\":\"frame\",\"f\":1,\"ft\":2,\"gt\":3,\"level\":4,\"viewer_objnum\":-1,\"object_count\":0,\"player\":{\"objnum\":5,\"score\":0,\"energy\":0,\"shields\":0,\"flags\":0,\"seg\":6,\"phys_flags\":7,\"pos\":[0,0,0],\"last_pos\":[0,0,0],\"vel\":[0,0,0],\"orient\":{\"f\":[0,0,0],\"r\":[0,0,0],\"u\":[0,0,0]},\"control\":{\"valid\":false},\"wiggle\":{\"valid\":true,\"applied\":8,\"seg\":9,\"phys_flags\":10,\"ft\":11,\"raw\":12,\"scaled\":13,\"amount\":14,\"vel_before\":[15,16,17],\"delta\":[18,19,20],\"vel_after\":[21,22,23],\"uvec\":[24,25,26]}},\"objects\":[]}\n";
	capture_buffer capture = {{0}, 0, sizeof(capture.data) - 1};
	classic_demo_json_writer writer;
	classic_demo_json_frame frame;

	memset(&frame, 0, sizeof(frame));
	frame.frame_number = 1;
	frame.frame_time = 2;
	frame.game_time = 3;
	frame.level = 4;
	frame.viewer_objnum = -1;
	frame.player.objnum = 5;
	frame.player.segnum = 6;
	frame.player.phys_flags = 7;
	frame.player.wiggle.valid = 1;
	frame.player.wiggle.applied = 8;
	frame.player.wiggle.segnum = 9;
	frame.player.wiggle.phys_flags = 10;
	frame.player.wiggle.frame_time = 11;
	frame.player.wiggle.raw_swiggle = 12;
	frame.player.wiggle.scaled_swiggle = 13;
	frame.player.wiggle.wiggle_amount = 14;
	frame.player.wiggle.velocity_before = vector(15, 16, 17);
	frame.player.wiggle.wiggle_delta = vector(18, 19, 20);
	frame.player.wiggle.velocity_after = vector(21, 22, 23);
	frame.player.wiggle.uvec = vector(24, 25, 26);
	classic_demo_json_writer_init(&writer, capture_write, &capture);
	return classic_demo_json_write_frame(&writer, &frame) &&
		!writer.failed && !strcmp(capture.data, expected);
}

static int test_failure_contract(void)
{
	capture_buffer capture = {{0}, 0, 16};
	classic_demo_json_writer writer;
	classic_demo_json_header header = {16, 3, "mission", 0, 0, 0, 0, 0, 0};
	classic_demo_json_result result = {0, 0, 0};
	classic_demo_json_frame invalid_frame;

	classic_demo_json_writer_init(&writer, capture_write, &capture);
	if (classic_demo_json_write_header(&writer, &header) || !writer.failed ||
	    classic_demo_json_write_result(&writer, &result))
		return 0;
	memset(&invalid_frame, 0, sizeof(invalid_frame));
	invalid_frame.object_count = 1;
	capture.size = 0;
	capture.fail_after = sizeof(capture.data) - 1;
	classic_demo_json_writer_init(&writer, capture_write, &capture);
	return !classic_demo_json_write_frame(&writer, &invalid_frame) &&
		writer.failed && capture.size == 0;
}

int main(void)
{
	if (!test_exact_output())
		return 1;
	if (!test_standalone_wiggle())
		return 2;
	if (!test_failure_contract())
		return 3;
	return 0;
}
