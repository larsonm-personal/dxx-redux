#include "classic_demo_json.h"

#include <inttypes.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

static int classic_demo_json_emit(classic_demo_json_writer *writer,
                                  const char *data, size_t size)
{
	if (!writer || writer->failed || !writer->write ||
	    (size && !data) || !writer->write(writer->context, data, size)) {
		if (writer)
			writer->failed = 1;
		return 0;
	}
	return 1;
}

static int classic_demo_json_emit_string(classic_demo_json_writer *writer,
                                         const char *text)
{
	return classic_demo_json_emit(writer, text, strlen(text));
}

static int classic_demo_json_emit_format(classic_demo_json_writer *writer,
                                         const char *format, ...)
{
	char buffer[1024];
	char *output = buffer;
	va_list args;
	va_list copy;
	int length;
	int ok;

	va_start(args, format);
	va_copy(copy, args);
	length = vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);
	if (length < 0) {
		writer->failed = 1;
		va_end(copy);
		return 0;
	}
	if ((size_t) length >= sizeof(buffer)) {
		output = malloc((size_t) length + 1);
		if (!output) {
			writer->failed = 1;
			va_end(copy);
			return 0;
		}
		vsnprintf(output, (size_t) length + 1, format, copy);
	}
	va_end(copy);
	ok = classic_demo_json_emit(writer, output, (size_t) length);
	if (output != buffer)
		free(output);
	return ok;
}

static int classic_demo_json_emit_escaped(classic_demo_json_writer *writer,
                                          const char *text)
{
	const unsigned char *p = (const unsigned char *) (text ? text : "");

	if (!classic_demo_json_emit_string(writer, "\""))
		return 0;
	for (; *p; ++p) {
		switch (*p) {
			case '\\':
				if (!classic_demo_json_emit_string(writer, "\\\\"))
					return 0;
				break;
			case '"':
				if (!classic_demo_json_emit_string(writer, "\\\""))
					return 0;
				break;
			case '\b':
				if (!classic_demo_json_emit_string(writer, "\\b"))
					return 0;
				break;
			case '\f':
				if (!classic_demo_json_emit_string(writer, "\\f"))
					return 0;
				break;
			case '\n':
				if (!classic_demo_json_emit_string(writer, "\\n"))
					return 0;
				break;
			case '\r':
				if (!classic_demo_json_emit_string(writer, "\\r"))
					return 0;
				break;
			case '\t':
				if (!classic_demo_json_emit_string(writer, "\\t"))
					return 0;
				break;
			default:
				if (*p < 0x20) {
					if (!classic_demo_json_emit_format(writer, "\\u%04x", *p))
						return 0;
				} else if (!classic_demo_json_emit(writer, (const char *) p, 1))
					return 0;
		}
	}
	return classic_demo_json_emit_string(writer, "\"");
}

static int classic_demo_json_emit_vector(classic_demo_json_writer *writer,
                                         const classic_demo_json_vector *vector)
{
	return classic_demo_json_emit_format(writer, "[%d,%d,%d]",
	                                     vector->x, vector->y, vector->z);
}

static int classic_demo_json_emit_matrix(classic_demo_json_writer *writer,
                                         const classic_demo_json_matrix *matrix)
{
	return classic_demo_json_emit_string(writer, "{\"f\":") &&
	       classic_demo_json_emit_vector(writer, &matrix->fvec) &&
	       classic_demo_json_emit_string(writer, ",\"r\":") &&
	       classic_demo_json_emit_vector(writer, &matrix->rvec) &&
	       classic_demo_json_emit_string(writer, ",\"u\":") &&
	       classic_demo_json_emit_vector(writer, &matrix->uvec) &&
	       classic_demo_json_emit_string(writer, "}");
}

static int classic_demo_json_file_write(void *context, const char *data, size_t size)
{
	FILE *file = context;

	return file && fwrite(data, 1, size, file) == size;
}

void classic_demo_json_writer_init(classic_demo_json_writer *writer,
                                   int (*write)(void *context, const char *data, size_t size), void *context)
{
	if (!writer)
		return;
	writer->context = context;
	writer->write = write;
	writer->failed = write ? 0 : 1;
}

void classic_demo_json_writer_init_file(classic_demo_json_writer *writer, FILE *file)
{
	classic_demo_json_writer_init(writer, classic_demo_json_file_write, file);
}

int classic_demo_json_write_header(classic_demo_json_writer *writer,
                                   const classic_demo_json_header *header)
{
	if (!writer || !header) {
		if (writer)
			writer->failed = 1;
		return 0;
	}
	return classic_demo_json_emit_format(writer,
	                                     "{\"type\":\"header\",\"format\":\"classic_dem_runtime_dump\",\"game\":\"d2\",\"version\":%d,\"game_type\":%d,\"mission\":",
	                                     header->version, header->game_type) &&
	       classic_demo_json_emit_escaped(writer, header->mission) &&
	       classic_demo_json_emit_format(writer,
	                                     ",\"score\":%d,\"primary_weapon\":%d,\"secondary_weapon\":%d,\"player_flags\":%d,\"energy\":%d,\"shields\":%d}\n",
	                                     header->score, header->primary_weapon, header->secondary_weapon,
	                                     header->player_flags, header->energy, header->shields);
}

static int classic_demo_json_write_control(classic_demo_json_writer *writer,
                                           const classic_demo_json_control *control)
{
	if (!classic_demo_json_emit_string(writer, "\"control\":{\"valid\":"))
		return 0;
	if (!classic_demo_json_emit_string(writer, control->valid ? "true" : "false"))
		return 0;
	if (control->valid) {
		if (!classic_demo_json_emit_format(writer,
		                                   ",\"seg\":%d,\"phys_flags\":%d,\"player_flags\":%d,\"ft\":%d,\"resolved_forward\":%d,\"pitch\":%d,\"heading\":%d,\"bank\":%d,\"forward\":%d,\"sideways\":%d,\"vertical\":%d,\"afterburner_state\":%d,\"afterburner_charge\":%d,\"wiggle_applied\":%d,\"wiggle_raw\":%d,\"wiggle_scaled\":%d,\"wiggle_amount\":%d,\"ship_wiggle\":%d,\"pre_thrust\":",
		                                   control->segnum, control->phys_flags, control->player_flags,
		                                   control->frame_time, control->resolved_forward_thrust_time,
		                                   control->control_pitch, control->control_heading, control->control_bank,
		                                   control->control_forward, control->control_sideways,
		                                   control->control_vertical, control->afterburner_state,
		                                   control->afterburner_charge, control->wiggle_applied,
		                                   control->raw_swiggle, control->scaled_swiggle,
		                                   control->wiggle_amount, control->ship_wiggle) ||
		    !classic_demo_json_emit_vector(writer, &control->pre_scale_thrust) ||
		    !classic_demo_json_emit_string(writer, ",\"thrust\":") ||
		    !classic_demo_json_emit_vector(writer, &control->thrust) ||
		    !classic_demo_json_emit_string(writer, ",\"pre_rot\":") ||
		    !classic_demo_json_emit_vector(writer, &control->pre_scale_rotthrust) ||
		    !classic_demo_json_emit_string(writer, ",\"rot\":") ||
		    !classic_demo_json_emit_vector(writer, &control->rotthrust) ||
		    !classic_demo_json_emit_string(writer, ",\"vel_before_wiggle\":") ||
		    !classic_demo_json_emit_vector(writer, &control->velocity_before_wiggle) ||
		    !classic_demo_json_emit_string(writer, ",\"wiggle_delta\":") ||
		    !classic_demo_json_emit_vector(writer, &control->wiggle_delta) ||
		    !classic_demo_json_emit_string(writer, ",\"vel_after_wiggle\":") ||
		    !classic_demo_json_emit_vector(writer, &control->velocity_after_wiggle))
			return 0;
	}
	return classic_demo_json_emit_string(writer, "}");
}

static int classic_demo_json_write_wiggle(classic_demo_json_writer *writer,
                                          const classic_demo_json_player *player)
{
	const classic_demo_json_control *control = &player->control;
	const classic_demo_json_wiggle *wiggle = &player->wiggle;
	const int valid = control->valid || wiggle->valid;

	if (!classic_demo_json_emit_string(writer, ",\"wiggle\":{\"valid\":") ||
	    !classic_demo_json_emit_string(writer, valid ? "true" : "false"))
		return 0;
	if (control->valid) {
		if (!classic_demo_json_emit_format(writer,
		                                   ",\"applied\":%d,\"seg\":%d,\"phys_flags\":%d,\"ft\":%d,\"raw\":%d,\"scaled\":%d,\"amount\":%d,\"vel_before\":",
		                                   control->wiggle_applied, control->segnum, control->phys_flags,
		                                   control->frame_time, control->raw_swiggle, control->scaled_swiggle,
		                                   control->wiggle_amount) ||
		    !classic_demo_json_emit_vector(writer, &control->velocity_before_wiggle) ||
		    !classic_demo_json_emit_string(writer, ",\"delta\":") ||
		    !classic_demo_json_emit_vector(writer, &control->wiggle_delta) ||
		    !classic_demo_json_emit_string(writer, ",\"vel_after\":") ||
		    !classic_demo_json_emit_vector(writer, &control->velocity_after_wiggle) ||
		    !classic_demo_json_emit_string(writer, ",\"uvec\":") ||
		    !classic_demo_json_emit_vector(writer, &player->orientation.uvec))
			return 0;
	} else if (wiggle->valid) {
		if (!classic_demo_json_emit_format(writer,
		                                   ",\"applied\":%d,\"seg\":%d,\"phys_flags\":%d,\"ft\":%d,\"raw\":%d,\"scaled\":%d,\"amount\":%d,\"vel_before\":",
		                                   wiggle->applied, wiggle->segnum, wiggle->phys_flags,
		                                   wiggle->frame_time, wiggle->raw_swiggle, wiggle->scaled_swiggle,
		                                   wiggle->wiggle_amount) ||
		    !classic_demo_json_emit_vector(writer, &wiggle->velocity_before) ||
		    !classic_demo_json_emit_string(writer, ",\"delta\":") ||
		    !classic_demo_json_emit_vector(writer, &wiggle->wiggle_delta) ||
		    !classic_demo_json_emit_string(writer, ",\"vel_after\":") ||
		    !classic_demo_json_emit_vector(writer, &wiggle->velocity_after) ||
		    !classic_demo_json_emit_string(writer, ",\"uvec\":") ||
		    !classic_demo_json_emit_vector(writer, &wiggle->uvec))
			return 0;
	}
	return classic_demo_json_emit_string(writer, "}");
}

static int classic_demo_json_write_object(classic_demo_json_writer *writer,
                                          const classic_demo_json_object *object)
{
	if (!classic_demo_json_emit_format(writer,
	                                   "{\"objnum\":%d,\"sig\":%d,\"obj_type\":%d,\"id\":%d,\"seg\":%d,\"flags\":%d,\"size\":%d,\"shields\":%d,\"lifeleft\":%d,\"control_type\":%d,\"movement_type\":%d,\"render_type\":%d,\"viewer\":%s,\"pos\":",
	                                   object->objnum, object->signature, object->type, object->id,
	                                   object->segnum, object->flags, object->size, object->shields,
	                                   object->lifeleft, object->control_type, object->movement_type,
	                                   object->render_type, object->viewer ? "true" : "false") ||
	    !classic_demo_json_emit_vector(writer, &object->position) ||
	    !classic_demo_json_emit_string(writer, ",\"last_pos\":") ||
	    !classic_demo_json_emit_vector(writer, &object->last_position))
		return 0;
	if (object->has_physics &&
	    (!classic_demo_json_emit_format(writer, ",\"phys_flags\":%d,\"vel\":",
	                                    object->phys_flags) ||
	     !classic_demo_json_emit_vector(writer, &object->velocity)))
		return 0;
	if (object->has_robot_ai) {
		const classic_demo_json_robot_ai *ai = &object->robot_ai;

		if (!classic_demo_json_emit_format(writer,
		                                   ",\"robot_ai\":{\"companion\":%d,\"behavior\":%d,\"mode\":%d,\"cur_state\":%d,\"goal_state\":%d,\"gun\":%d,\"path_dir\":%d,\"goal_side\":%d,\"danger_obj\":%d,\"danger_sig\":%d,\"player_seg\":%d,\"believed_seg\":%d,\"goal_seg\":%d,\"prev_vis\":%d,\"aware\":%d,\"aware_time\":%d,\"seen\":%" PRId64 ",\"since\":%d,\"next_action\":%d,\"next_fire\":%d,\"next_fire2\":%d,\"path_index\":%d,\"path_length\":%d,\"hide\":%d,\"skip\":%d}",
		                                   ai->companion, ai->behavior, ai->mode, ai->current_state,
		                                   ai->goal_state, ai->current_gun, ai->path_direction, ai->goal_side,
		                                   ai->danger_object, ai->danger_signature, ai->player_segment,
		                                   ai->believed_segment, ai->goal_segment, ai->previous_visibility,
		                                   ai->awareness_type, ai->awareness_time, ai->time_player_seen,
		                                   ai->time_since_processed, ai->next_action_time, ai->next_fire,
		                                   ai->next_fire2, ai->path_index, ai->path_length, ai->hide_index,
		                                   ai->skip_ai_count))
			return 0;
	}
	return classic_demo_json_emit_string(writer, ",\"orient\":") &&
	       classic_demo_json_emit_matrix(writer, &object->orientation) &&
	       classic_demo_json_emit_string(writer, "}");
}

int classic_demo_json_write_frame(classic_demo_json_writer *writer,
                                  const classic_demo_json_frame *frame)
{
	const classic_demo_json_player *player;
	size_t i;

	if (!writer || !frame || (frame->object_count && !frame->objects)) {
		if (writer)
			writer->failed = 1;
		return 0;
	}
	player = &frame->player;
	if (!classic_demo_json_emit_format(writer,
	                                   "{\"type\":\"frame\",\"f\":%d,\"ft\":%d,\"gt\":%d,\"level\":%d,\"viewer_objnum\":%d,\"object_count\":%zu,\"player\":{\"objnum\":%d,\"score\":%d,\"energy\":%d,\"shields\":%d,\"flags\":%d,\"seg\":%d,\"phys_flags\":%d,\"pos\":",
	                                   frame->frame_number, frame->frame_time, frame->game_time, frame->level,
	                                   frame->viewer_objnum, frame->object_count, player->objnum, player->score,
	                                   player->energy, player->shields, player->flags, player->segnum,
	                                   player->phys_flags) ||
	    !classic_demo_json_emit_vector(writer, &player->position) ||
	    !classic_demo_json_emit_string(writer, ",\"last_pos\":") ||
	    !classic_demo_json_emit_vector(writer, &player->last_position) ||
	    !classic_demo_json_emit_string(writer, ",\"vel\":") ||
	    !classic_demo_json_emit_vector(writer, &player->velocity) ||
	    !classic_demo_json_emit_string(writer, ",\"orient\":") ||
	    !classic_demo_json_emit_matrix(writer, &player->orientation) ||
	    !classic_demo_json_emit_string(writer, ",") ||
	    !classic_demo_json_write_control(writer, &player->control) ||
	    !classic_demo_json_write_wiggle(writer, player) ||
	    !classic_demo_json_emit_string(writer, "},\"objects\":["))
		return 0;
	for (i = 0; i < frame->object_count; ++i) {
		if ((i && !classic_demo_json_emit_string(writer, ",")) ||
		    !classic_demo_json_write_object(writer, &frame->objects[i]))
			return 0;
	}
	return classic_demo_json_emit_string(writer, "]}\n");
}

int classic_demo_json_write_robot_damage(classic_demo_json_writer *writer,
                                         const classic_demo_json_robot_damage *damage)
{
	if (!writer || !damage) {
		if (writer)
			writer->failed = 1;
		return 0;
	}
	if (!classic_demo_json_emit_format(writer,
	                                   "{\"type\":\"robot_damage\",\"f\":%d,\"gt\":%d,\"objnum\":%d,\"sig\":%d,\"id\":%d,\"size\":%d,\"damage\":%d,\"shields_before\":%d,\"shields_after\":%d,\"dead\":%s,\"pos\":",
	                                   damage->frame_number, damage->game_time, damage->objnum,
	                                   damage->signature, damage->id, damage->size, damage->damage,
	                                   damage->shields_before, damage->shields_after,
	                                   damage->dead ? "true" : "false") ||
	    !classic_demo_json_emit_vector(writer, &damage->position))
		return 0;
	if (damage->has_velocity &&
	    (!classic_demo_json_emit_string(writer, ",\"vel\":") ||
	     !classic_demo_json_emit_vector(writer, &damage->velocity)))
		return 0;
	return classic_demo_json_emit_string(writer, "}\n");
}

int classic_demo_json_write_result(classic_demo_json_writer *writer,
                                   const classic_demo_json_result *result)
{
	if (!writer || !result) {
		if (writer)
			writer->failed = 1;
		return 0;
	}
	return classic_demo_json_emit_format(writer,
	                                     "{\"type\":\"result\",\"frames_decoded\":%d,\"objects_emitted\":%d,\"truncated\":%s}\n",
	                                     result->frames_decoded, result->objects_emitted,
	                                     result->truncated ? "true" : "false");
}
