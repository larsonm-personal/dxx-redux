#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "3d.h"
#include "segment.h"
#include "object.h"
#include "automap.h"
#include "automap_metadata_overlay.h"
#include "gamefont.h"
#include "gameseg.h"
#include "gr.h"
#include "powerup.h"
#include "secretarea.h"

#define K_SECRET_LABEL_UNFOUND_COLOR      BM_XRGB(31, 0, 0)
#define K_SECRET_LABEL_FOUND_COLOR        BM_XRGB(0, 31, 0)
#define K_OBJECTIVE_LABEL_COLOR           BM_XRGB(10, 63, 63)
#define K_OBJECTIVE_LABEL_BLUE            BM_XRGB(5, 5, 63)
#define K_OBJECTIVE_LABEL_GOLD            BM_XRGB(63, 63, 10)
#define K_OBJECTIVE_LABEL_RED             BM_XRGB(63, 5, 5)
#define K_NEXT_OBJECTIVE_COLOR            BM_XRGB(63, 5, 5)
#define K_NEXT_OBJECTIVE_COUNT            3
#define K_OBJECTIVE_GUIDANCE_MAX_DISTANCE (200 * F1_0)

static int key_carrier_marker_count;
static int key_carrier_marker_objnum = -1;
static int key_carrier_marker_key_index = -1;
static int key_carrier_marker_position[3];
static int merged_objective_label_count;
static char first_merged_objective_label[LEVEL_METADATA_MAX_ROUTE_STEPS * 4];
static int next_objective_x;
static int next_objective_y;
static int objective_level_label_y;
static char first_next_objective_text[LEVEL_METADATA_ROUTE_LABEL_LEN + 16];
static int long_guidance_suppressed_count;

static int objective_key_powerup_id(int key_index)
{
	switch (key_index) {
		case 0:
			return POW_KEY_BLUE;
		case 1:
			return POW_KEY_RED;
		case 2:
			return POW_KEY_GOLD;
		default:
			return -1;
	}
}

static int objective_key_object_position(
    const level_metadata_route_step *step,
    int position[3],
    int *resolved_objnum,
    int *is_carrier)
{
	int objnum;
	int powerup_id;
	int best_objnum = -1;
	fix best_distance = 0;
	vms_vector static_pos;

	if (resolved_objnum)
		*resolved_objnum = -1;
	if (is_carrier)
		*is_carrier = 0;
	if (!step || step->kind != LEVEL_METADATA_ROUTE_KEY ||
	    step->key_carrier_objnum < 0 || !position)
		return 0;
	powerup_id = objective_key_powerup_id(step->key_index);
	if (powerup_id < 0)
		return 0;
	objnum = step->key_carrier_objnum;
	if (objnum <= Highest_object_index &&
	    Objects[objnum].type == OBJ_ROBOT &&
	    !(Objects[objnum].flags & OF_SHOULD_BE_DEAD) &&
	    Objects[objnum].contains_count > 0 &&
	    Objects[objnum].contains_type == OBJ_POWERUP &&
	    Objects[objnum].contains_id == powerup_id) {
		position[0] = Objects[objnum].pos.x;
		position[1] = Objects[objnum].pos.y;
		position[2] = Objects[objnum].pos.z;
		if (resolved_objnum)
			*resolved_objnum = objnum;
		if (is_carrier)
			*is_carrier = 1;
		return 1;
	}

	static_pos.x = step->label_pos[0];
	static_pos.y = step->label_pos[1];
	static_pos.z = step->label_pos[2];
	for (objnum = 0; objnum <= Highest_object_index; ++objnum) {
		fix distance;
		if (Objects[objnum].type != OBJ_POWERUP ||
		    Objects[objnum].id != powerup_id ||
		    (Objects[objnum].flags & OF_SHOULD_BE_DEAD))
			continue;
		distance = vm_vec_dist_quick(&static_pos, &Objects[objnum].pos);
		if (best_objnum >= 0 && distance >= best_distance)
			continue;
		best_objnum = objnum;
		best_distance = distance;
	}
	if (best_objnum < 0)
		return 0;
	position[0] = Objects[best_objnum].pos.x;
	position[1] = Objects[best_objnum].pos.y;
	position[2] = Objects[best_objnum].pos.z;
	if (resolved_objnum)
		*resolved_objnum = best_objnum;
	return 1;
}

int automap_metadata_get_key_carrier_marker(
    int *objnum, int *key_index, int position[3])
{
	if (key_carrier_marker_count <= 0)
		return 0;
	if (objnum)
		*objnum = key_carrier_marker_objnum;
	if (key_index)
		*key_index = key_carrier_marker_key_index;
	if (position) {
		position[0] = key_carrier_marker_position[0];
		position[1] = key_carrier_marker_position[1];
		position[2] = key_carrier_marker_position[2];
	}
	return 1;
}

int automap_metadata_get_key_carrier_marker_count(void)
{
	return key_carrier_marker_count;
}

int automap_metadata_get_merged_objective_label_count(void)
{
	return merged_objective_label_count;
}

const char *automap_metadata_get_first_merged_objective_label(void)
{
	return first_merged_objective_label;
}

static int draw_text_label(const char *label, int color, const g3s_point *point)
{
	int w, h, aw;
	g3s_point label_point = *point;

	if (label_point.p3_codes & CC_BEHIND)
		return 0;
	g3_project_point(&label_point);
	if (!(label_point.p3_flags & PF_PROJECTED))
		return 0;

	gr_set_curfont(GAME_FONT);
	gr_set_fontcolor(color, -1);
	gr_get_string_size(label, &w, &h, &aw);
	gr_printf(f2i(label_point.p3_sx) - w / 2,
	          f2i(label_point.p3_sy) - h / 2, "%s", label);
	return 1;
}

static void get_secret_label_pos(const secret_area_entry *secret, vms_vector *pos)
{
	int vertex_list[4];

	if (secret->entry_seg >= 0 && secret->entry_seg <= Highest_segment_index &&
	    secret->entry_side >= 0 && secret->entry_side < MAX_SIDES_PER_SEGMENT) {
		get_side_verts(vertex_list, secret->entry_seg, secret->entry_side);
		pos->x = (Vertices[vertex_list[0]].x + Vertices[vertex_list[1]].x +
		          Vertices[vertex_list[2]].x + Vertices[vertex_list[3]].x) /
		         4;
		pos->y = (Vertices[vertex_list[0]].y + Vertices[vertex_list[1]].y +
		          Vertices[vertex_list[2]].y + Vertices[vertex_list[3]].y) /
		         4;
		pos->z = (Vertices[vertex_list[0]].z + Vertices[vertex_list[1]].z +
		          Vertices[vertex_list[2]].z + Vertices[vertex_list[3]].z) /
		         4;
		return;
	}

	pos->x = secret->label_pos[0];
	pos->y = secret->label_pos[1];
	pos->z = secret->label_pos[2];
}

static void draw_secret_labels(int *candidate_count, int *projected_count)
{
	const secret_area_state *state = secret_area_get_state();
	int total = secret_area_total(state);
	int reveal_unfound = secret_area_get_reveal_unfound();
	int i;

	if (!state || !state->enabled)
		return;
	if (!reveal_unfound)
		return;
	for (i = 0; i < total; ++i) {
		const secret_area_entry *secret = &state->secrets[i];
		int found = state->found[i] != 0;
		char label[8];
		vms_vector pos;
		g3s_point point;

		snprintf(label, sizeof(label), "S%d", secret->display_index);
		get_secret_label_pos(secret, &pos);
		g3_rotate_point(&point, &pos);
		(*candidate_count)++;
		if (draw_text_label(label,
		                    found ? K_SECRET_LABEL_FOUND_COLOR : K_SECRET_LABEL_UNFOUND_COLOR,
		                    &point))
			(*projected_count)++;
	}
}

static int objective_label_color(const level_metadata_route_step *step)
{
	if (!step || step->kind != LEVEL_METADATA_ROUTE_KEY)
		return K_OBJECTIVE_LABEL_COLOR;
	switch (step->key_index) {
		case 0:
			return K_OBJECTIVE_LABEL_BLUE;
		case 1:
			return K_OBJECTIVE_LABEL_RED;
		case 2:
			return K_OBJECTIVE_LABEL_GOLD;
		default:
			return K_OBJECTIVE_LABEL_COLOR;
	}
}

static int objective_has_distinct_guidance_positions(
    const level_metadata_route_step *step)
{
	return step &&
	       (step->activation_kind ==
	            LEVEL_METADATA_ROUTE_ACTIVATION_SHOOT_SWITCH ||
	        step->activation_kind ==
	            LEVEL_METADATA_ROUTE_ACTIVATION_FLY_THROUGH_TRIGGER ||
	        step->activation_kind ==
	            LEVEL_METADATA_ROUTE_ACTIVATION_PASS_THROUGH_TRIGGER) &&
	       step->activation_pos_valid && step->aim_pos_valid &&
	       (step->activation_pos[0] != step->aim_pos[0] ||
	        step->activation_pos[1] != step->aim_pos[1] ||
	        step->activation_pos[2] != step->aim_pos[2]);
}

static int objective_guidance_positions_are_readable(
    const level_metadata_route_step *step)
{
	vms_vector activation;
	vms_vector aim;

	if (!objective_has_distinct_guidance_positions(step))
		return 0;
	activation.x = step->activation_pos[0];
	activation.y = step->activation_pos[1];
	activation.z = step->activation_pos[2];
	aim.x = step->aim_pos[0];
	aim.y = step->aim_pos[1];
	aim.z = step->aim_pos[2];
	return vm_vec_dist_quick(&activation, &aim) <=
	       K_OBJECTIVE_GUIDANCE_MAX_DISTANCE;
}

static const level_metadata_state *current_objective_route(
    route_planner_plan_summary *plan)
{
	const level_metadata_state *metadata = level_metadata_get_live_route_state();

	if (metadata && level_metadata_get_live_route_plan_summary(plan))
		return metadata;
	metadata = level_metadata_get_canonical_state();
	if (metadata && level_metadata_get_canonical_route_plan_summary(plan))
		return metadata;
	return NULL;
}

static const level_metadata_state *objective_label_route(
    int *first_step,
    int *end_step)
{
	route_planner_plan_summary plan;
	const level_metadata_state *metadata;
	int mode = level_metadata_get_objective_mode();

	if (mode == LEVEL_METADATA_OBJECTIVES_OFF)
		return NULL;
	if (mode == LEVEL_METADATA_OBJECTIVES_ALL) {
		metadata = level_metadata_get_canonical_state();
		if (!metadata)
			return NULL;
		*first_step = 0;
		*end_step = metadata->route_step_count;
		return metadata;
	}
	memset(&plan, 0, sizeof(plan));
	metadata = current_objective_route(&plan);
	if (!metadata || plan.first_pending_step < 0 ||
	    plan.first_pending_step >= metadata->route_step_count)
		return NULL;
	*first_step = plan.first_pending_step;
	while (*first_step < metadata->route_step_count &&
	       metadata->route_steps[*first_step].kind == LEVEL_METADATA_ROUTE_START)
		(*first_step)++;
	*end_step = mode == LEVEL_METADATA_OBJECTIVES_NEXT
	                ? *first_step + 1
	                : metadata->route_step_count;
	return metadata;
}

static int objective_steps_match(
    const level_metadata_route_step *first,
    const level_metadata_route_step *second)
{
	if (!first || !second || first->kind != second->kind ||
	    first->activation_kind != second->activation_kind)
		return 0;
	if (first->trigger_num >= 0 || second->trigger_num >= 0)
		return first->trigger_num == second->trigger_num;
	if (first->key_index >= 0 || second->key_index >= 0)
		return first->key_index == second->key_index;
	if (first->wall_num >= 0 || second->wall_num >= 0)
		return first->wall_num == second->wall_num;
	return first->seg == second->seg && first->side == second->side;
}

static int objective_display_number(
    const level_metadata_state *metadata, int step_index)
{
	const level_metadata_state *canonical = level_metadata_get_canonical_state();
	const level_metadata_route_step *step;
	int i;

	if (!metadata || step_index < 0 || step_index >= metadata->route_step_count)
		return 0;
	step = &metadata->route_steps[step_index];
	if (canonical)
		for (i = 0; i < canonical->route_step_count; ++i) {
			if (objective_steps_match(step, &canonical->route_steps[i]))
				return i + 1;
		}
	return step_index + 1;
}

typedef struct objective_label_candidate {
	int objective_number;
	int color;
	int screen_x;
	int screen_y;
	int width;
	int height;
	int group;
} objective_label_candidate;

static void collect_objective_label(
    objective_label_candidate labels[LEVEL_METADATA_MAX_ROUTE_STEPS * 2],
    int *label_count,
    int *candidate_count,
    int objective_number,
    int color,
    const int position[3])
{
	vms_vector pos;
	g3s_point point;
	char text[12];
	int average_width;
	objective_label_candidate *label;

	(*candidate_count)++;
	if (*label_count >= LEVEL_METADATA_MAX_ROUTE_STEPS * 2)
		return;
	pos.x = position[0];
	pos.y = position[1];
	pos.z = position[2];
	g3_rotate_point(&point, &pos);
	if (point.p3_codes & CC_BEHIND)
		return;
	g3_project_point(&point);
	if (!(point.p3_flags & PF_PROJECTED) || (point.p3_flags & PF_OVERFLOW))
		return;

	label = &labels[(*label_count)++];
	label->objective_number = objective_number;
	label->color = color;
	label->screen_x = f2i(point.p3_sx);
	label->screen_y = f2i(point.p3_sy);
	snprintf(text, sizeof(text), "%d", objective_number);
	gr_set_curfont(GAME_FONT);
	gr_get_string_size(text, &label->width, &label->height, &average_width);
	label->group = *label_count - 1;
}

static int objective_label_group_root(
    objective_label_candidate *labels, int index)
{
	while (labels[index].group != index)
		index = labels[index].group;
	return index;
}

static int objective_labels_overlap(
    const objective_label_candidate *first,
    const objective_label_candidate *second)
{
	return abs(first->screen_x - second->screen_x) * 2 <=
	           first->width + second->width + 4 &&
	       abs(first->screen_y - second->screen_y) * 2 <=
	           first->height + second->height + 4;
}

static void draw_collected_objective_labels(
    objective_label_candidate *labels,
    int label_count,
    int *projected_count)
{
	int i;
	int j;

	for (i = 0; i < label_count; ++i) {
		for (j = i + 1; j < label_count; ++j) {
			int first_root;
			int second_root;
			if (!objective_labels_overlap(&labels[i], &labels[j]))
				continue;
			first_root = objective_label_group_root(labels, i);
			second_root = objective_label_group_root(labels, j);
			if (first_root != second_root)
				labels[second_root].group = first_root;
		}
	}
	for (i = 0; i < label_count; ++i)
		labels[i].group = objective_label_group_root(labels, i);

	for (i = 0; i < label_count; ++i) {
		char text[LEVEL_METADATA_MAX_ROUTE_STEPS * 4];
		int text_length = 0;
		int count = 0;
		int objective_count = 0;
		int color = labels[i].color;
		int screen_x = 0;
		int screen_y = 0;
		int width;
		int height;
		int average_width;

		if (labels[i].group != i)
			continue;
		text[0] = '\0';
		for (j = 0; j < label_count; ++j) {
			int k;
			int duplicate = 0;
			if (labels[j].group != i)
				continue;
			screen_x += labels[j].screen_x;
			screen_y += labels[j].screen_y;
			count++;
			if (labels[j].color != color)
				color = K_OBJECTIVE_LABEL_COLOR;
			for (k = 0; k < j; ++k)
				if (labels[k].group == i &&
				    labels[k].objective_number == labels[j].objective_number) {
					duplicate = 1;
					break;
				}
			if (!duplicate) {
				objective_count++;
				text_length += snprintf(
				    text + text_length, sizeof(text) - text_length,
				    "%s%d", text_length ? "&" : "",
				    labels[j].objective_number);
			}
		}
		if (!count)
			continue;
		gr_set_curfont(GAME_FONT);
		gr_set_fontcolor(color, -1);
		gr_get_string_size(text, &width, &height, &average_width);
		gr_printf(
		    screen_x / count - width / 2, screen_y / count - height / 2,
		    "%s", text);
		if (objective_count > 1) {
			merged_objective_label_count++;
			if (!first_merged_objective_label[0])
				snprintf(
				    first_merged_objective_label,
				    sizeof(first_merged_objective_label), "%s", text);
		}
		(*projected_count)++;
	}
}

static void draw_objective_labels(
    int *visible_step_count,
    int *candidate_count,
    int *projected_count)
{
	objective_label_candidate labels[LEVEL_METADATA_MAX_ROUTE_STEPS * 2];
	int label_count = 0;
	int first_step = 0;
	int end_step = 0;
	const level_metadata_state *metadata =
	    objective_label_route(&first_step, &end_step);
	int i;

	long_guidance_suppressed_count = 0;
	if (!metadata)
		return;
	for (i = 0; i < metadata->route_step_count; ++i) {
		const level_metadata_route_step *step = &metadata->route_steps[i];
		int color;
		int dynamic_position[3];
		int dynamic_objnum;
		int dynamic_is_carrier;

		if (step->kind == LEVEL_METADATA_ROUTE_START)
			continue;
		if (i < first_step || i >= end_step)
			continue;
		const int objective_number = objective_display_number(metadata, i);
		(*visible_step_count)++;
		color = objective_label_color(step);
		if (objective_guidance_positions_are_readable(step)) {
			collect_objective_label(
			    labels, &label_count, candidate_count, objective_number, color,
			    step->activation_pos);
			collect_objective_label(
			    labels, &label_count, candidate_count, objective_number, color,
			    step->aim_pos);
		} else if (objective_key_object_position(
		               step, dynamic_position, &dynamic_objnum,
		               &dynamic_is_carrier)) {
			if (dynamic_is_carrier) {
				key_carrier_marker_count++;
				if (key_carrier_marker_objnum < 0) {
					key_carrier_marker_objnum = dynamic_objnum;
					key_carrier_marker_key_index = step->key_index;
					memcpy(
					    key_carrier_marker_position, dynamic_position,
					    sizeof(key_carrier_marker_position));
				}
			}
			collect_objective_label(
			    labels, &label_count, candidate_count, objective_number, color,
			    dynamic_position);
		} else if (step->label_pos_valid) {
			if (objective_has_distinct_guidance_positions(step))
				long_guidance_suppressed_count++;
			collect_objective_label(
			    labels, &label_count, candidate_count, objective_number, color,
			    step->label_pos);
		}
	}
	draw_collected_objective_labels(labels, label_count, projected_count);
}

static int draw_objective_connector(
    const char *label,
    int color,
    const int from_position[3],
    const int to_position[3])
{
	vms_vector from_pos;
	vms_vector to_pos;
	vms_vector screen_delta;
	g3s_point from_point;
	g3s_point to_point;
	fix screen_distance;
	fix inset;
	fix inset_ratio;
	int w, h, aw;

	from_pos.x = from_position[0];
	from_pos.y = from_position[1];
	from_pos.z = from_position[2];
	to_pos.x = to_position[0];
	to_pos.y = to_position[1];
	to_pos.z = to_position[2];
	g3_rotate_point(&from_point, &from_pos);
	g3_rotate_point(&to_point, &to_pos);
	if ((from_point.p3_codes | to_point.p3_codes) & CC_BEHIND)
		return 0;
	g3_project_point(&from_point);
	g3_project_point(&to_point);
	if (!(from_point.p3_flags & PF_PROJECTED) ||
	    !(to_point.p3_flags & PF_PROJECTED) ||
	    ((from_point.p3_flags | to_point.p3_flags) & PF_OVERFLOW))
		return 0;

	screen_delta.x = to_point.p3_sx - from_point.p3_sx;
	screen_delta.y = to_point.p3_sy - from_point.p3_sy;
	screen_delta.z = 0;
	screen_distance = vm_vec_mag_quick(&screen_delta);
	gr_set_curfont(GAME_FONT);
	gr_get_string_size(label, &w, &h, &aw);
	inset = i2f(w / 2 + 2);
	if (screen_distance <= inset * 2)
		return 0;
	inset_ratio = fixdiv(inset, screen_distance);
	from_point.p3_sx += fixmul(screen_delta.x, inset_ratio);
	from_point.p3_sy += fixmul(screen_delta.y, inset_ratio);
	to_point.p3_sx -= fixmul(screen_delta.x, inset_ratio);
	to_point.p3_sy -= fixmul(screen_delta.y, inset_ratio);
	gr_setcolor(color);
	return g3_draw_line(&from_point, &to_point);
}

void automap_metadata_draw_connectors(
    int *objective_candidate_count,
    int *objective_drawn_count)
{
	int first_step = 0;
	int end_step = 0;
	const level_metadata_state *metadata =
	    objective_label_route(&first_step, &end_step);
	int i;

	*objective_candidate_count = 0;
	*objective_drawn_count = 0;
	if (!metadata)
		return;
	for (i = 0; i < metadata->route_step_count; ++i) {
		const level_metadata_route_step *step = &metadata->route_steps[i];
		char label[12];

		if (step->kind == LEVEL_METADATA_ROUTE_START)
			continue;
		if (i < first_step || i >= end_step)
			continue;
		const int objective_number = objective_display_number(metadata, i);
		if (!objective_guidance_positions_are_readable(step))
			continue;
		snprintf(label, sizeof(label), "%d", objective_number);
		(*objective_candidate_count)++;
		if (draw_objective_connector(
		        label, objective_label_color(step), step->activation_pos,
		        step->aim_pos))
			(*objective_drawn_count)++;
	}
}

void automap_metadata_draw_labels(
    int *secret_candidate_count,
    int *secret_projected_count,
    int *objective_visible_step_count,
    int *objective_candidate_count,
    int *objective_projected_count)
{
	*secret_candidate_count = 0;
	*secret_projected_count = 0;
	draw_secret_labels(secret_candidate_count, secret_projected_count);

	*objective_visible_step_count = 0;
	*objective_candidate_count = 0;
	*objective_projected_count = 0;
	key_carrier_marker_count = 0;
	key_carrier_marker_objnum = -1;
	key_carrier_marker_key_index = -1;
	memset(key_carrier_marker_position, 0, sizeof(key_carrier_marker_position));
	merged_objective_label_count = 0;
	first_merged_objective_label[0] = '\0';
	draw_objective_labels(
	    objective_visible_step_count, objective_candidate_count,
	    objective_projected_count);
}

static void fit_objective_text(char *text, int max_width)
{
	int width;
	int height;
	int average_width;
	int length = (int) strlen(text);
	int truncated = 0;

	gr_get_string_size(text, &width, &height, &average_width);
	while (length > 3 && width > max_width) {
		text[--length] = '\0';
		truncated = 1;
		gr_get_string_size(text, &width, &height, &average_width);
	}
	if (truncated && length >= 3) {
		text[length - 3] = '.';
		text[length - 2] = '.';
		text[length - 1] = '.';
	}
}

void automap_metadata_draw_next_objectives(
    int *objective_count, int x, int level_label_y)
{
	route_planner_plan_summary plan;
	const level_metadata_state *metadata;
	int first;
	int max_count;
	int mode = level_metadata_get_objective_mode();
	int drawn = 0;
	int i;

	next_objective_x = x;
	objective_level_label_y = level_label_y;
	next_objective_y = level_label_y + LINE_SPACING;
	first_next_objective_text[0] = '\0';
	*objective_count = 0;
	if (mode == LEVEL_METADATA_OBJECTIVES_OFF)
		return;
	memset(&plan, 0, sizeof(plan));
	metadata = current_objective_route(&plan);
	if (!metadata)
		return;
	first = plan.first_pending_step;
	if (first < 0 || first >= metadata->route_step_count)
		return;
	max_count = mode == LEVEL_METADATA_OBJECTIVES_NEXT ? 1 : K_NEXT_OBJECTIVE_COUNT;
	gr_set_curfont(GAME_FONT);
	gr_set_fontcolor(K_NEXT_OBJECTIVE_COLOR, -1);
	for (i = first;
	     i < metadata->route_step_count && drawn < max_count;
	     ++i) {
		const level_metadata_route_step *step = &metadata->route_steps[i];
		char text[LEVEL_METADATA_ROUTE_LABEL_LEN + 16];
		int objective_number;

		if (step->kind == LEVEL_METADATA_ROUTE_START || !step->label[0])
			continue;
		objective_number = objective_display_number(metadata, i);
		snprintf(
		    text, sizeof(text), "%d: %s", objective_number, step->label);
		if (!drawn)
			snprintf(
			    first_next_objective_text,
			    sizeof(first_next_objective_text), "%s", text);
		fit_objective_text(
		    text, grd_curcanv->cv_bitmap.bm_w - FSPACX(4));
		gr_printf(next_objective_x, next_objective_y + drawn * LINE_SPACING,
		          "%s", text);
		drawn++;
	}
	*objective_count = drawn;
}

int automap_metadata_get_next_objective_x(void)
{
	return next_objective_x;
}

int automap_metadata_get_next_objective_y(void)
{
	return next_objective_y;
}

int automap_metadata_get_level_label_y(void)
{
	return objective_level_label_y;
}

const char *automap_metadata_get_first_next_objective_text(void)
{
	return first_next_objective_text;
}

int automap_metadata_get_long_guidance_suppressed_count(void)
{
	return long_guidance_suppressed_count;
}

int secret_area_should_draw_segment_edges(int segnum)
{
	const secret_area_state *state;
	int secret_index;

	if (segnum < 0 || segnum >= SECRET_AREA_MAX_SEGMENTS)
		return 0;
	state = secret_area_get_state();
	if (!state || !state->enabled)
		return 0;
	secret_index = state->segment_to_secret[segnum] - 1;
	if (secret_index < 0 || secret_index >= secret_area_total(state))
		return 0;
	return state->found[secret_index] || secret_area_get_reveal_unfound();
}

int automap_segment_is_within_limit(int segnum, int segment_limit)
{
	return Automap_visited[segnum] <= segment_limit ||
	       secret_area_should_draw_segment_edges(segnum);
}
