#include <stdio.h>
#include <string.h>

#include "3d.h"
#include "segment.h"
#include "object.h"
#include "automap.h"
#include "automap_metadata_overlay.h"
#include "gamefont.h"
#include "gameseg.h"
#include "gr.h"
#include "secretarea.h"
#if defined(DXX_BUILD_DESCENT_II) && defined(__ANDROID__)
#include "escort.h"
#endif

#define K_SECRET_LABEL_UNFOUND_COLOR BM_XRGB(31, 0, 0)
#define K_SECRET_LABEL_FOUND_COLOR   BM_XRGB(0, 31, 0)
#define K_OBJECTIVE_LABEL_COLOR      BM_XRGB(10, 63, 63)
#define K_OBJECTIVE_LABEL_BLUE       BM_XRGB(5, 5, 63)
#define K_OBJECTIVE_LABEL_GOLD       BM_XRGB(63, 63, 10)
#define K_OBJECTIVE_LABEL_RED        BM_XRGB(63, 5, 5)
#define K_GUIDANCE_POSITION_COLOR    BM_XRGB(5, 63, 20)
#define K_GUIDANCE_TARGET_COLOR      BM_XRGB(63, 25, 5)
#define K_NEXT_OBJECTIVE_COLOR       BM_XRGB(63, 5, 5)
#define K_NEXT_OBJECTIVE_COUNT       3

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
	       step->activation_kind ==
	           LEVEL_METADATA_ROUTE_ACTIVATION_SHOOT_SWITCH &&
	       step->activation_pos_valid && step->aim_pos_valid &&
	       (step->activation_pos[0] != step->aim_pos[0] ||
	        step->activation_pos[1] != step->aim_pos[1] ||
	        step->activation_pos[2] != step->aim_pos[2]);
}

static int draw_objective_label_at(
    const char *label,
    int color,
    const int position[3])
{
	vms_vector pos;
	g3s_point point;

	pos.x = position[0];
	pos.y = position[1];
	pos.z = position[2];
	g3_rotate_point(&point, &pos);
	return draw_text_label(label, color, &point);
}

static void draw_objective_labels(int *candidate_count, int *projected_count)
{
	const level_metadata_state *metadata = level_metadata_get_canonical_state();
	int objective_number = 0;
	int i;

	if (!level_metadata_get_show_objectives() || !metadata)
		return;
	for (i = 0; i < metadata->route_step_count; ++i) {
		const level_metadata_route_step *step = &metadata->route_steps[i];
		char label[12];
		int color;

		if (step->kind == LEVEL_METADATA_ROUTE_START)
			continue;
		objective_number++;
		snprintf(label, sizeof(label), "%d", objective_number);
		color = objective_label_color(step);
		if (objective_has_distinct_guidance_positions(step)) {
			(*candidate_count) += 2;
			if (draw_objective_label_at(
			        label, color, step->activation_pos))
				(*projected_count)++;
			if (draw_objective_label_at(label, color, step->aim_pos))
				(*projected_count)++;
		} else if (step->label_pos_valid) {
			(*candidate_count)++;
			if (draw_objective_label_at(label, color, step->label_pos))
				(*projected_count)++;
		}
	}
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
	const level_metadata_state *metadata = level_metadata_get_canonical_state();
	int objective_number = 0;
	int i;

	*objective_candidate_count = 0;
	*objective_drawn_count = 0;
	if (!level_metadata_get_show_objectives() || !metadata)
		return;
	for (i = 0; i < metadata->route_step_count; ++i) {
		const level_metadata_route_step *step = &metadata->route_steps[i];
		char label[12];

		if (step->kind == LEVEL_METADATA_ROUTE_START)
			continue;
		objective_number++;
		if (!objective_has_distinct_guidance_positions(step))
			continue;
		snprintf(label, sizeof(label), "%d", objective_number);
		(*objective_candidate_count)++;
		if (draw_objective_connector(
		        label, objective_label_color(step), step->activation_pos,
		        step->aim_pos))
			(*objective_drawn_count)++;
	}
}

#if defined(DXX_BUILD_DESCENT_II) && defined(__ANDROID__)
static void draw_guidebot_guidance_labels(int *candidate_count, int *projected_count)
{
	const level_metadata_state *metadata = level_metadata_get_live_route_state();
	route_planner_plan_summary plan;
	const level_metadata_route_step *step;
	vms_vector pos;
	g3s_point point;

	if (!level_metadata_get_show_objectives() || !escort_get_route_goal_active() ||
	    !metadata || !level_metadata_get_live_route_plan_summary(&plan) ||
	    plan.first_pending_step < 0 ||
	    plan.first_pending_step >= metadata->route_step_count)
		return;
	step = &metadata->route_steps[plan.first_pending_step];
	if (step->activation_pos_valid) {
		pos.x = step->activation_pos[0];
		pos.y = step->activation_pos[1];
		pos.z = step->activation_pos[2];
		g3_rotate_point(&point, &pos);
		(*candidate_count)++;
		if (draw_text_label("GO", K_GUIDANCE_POSITION_COLOR, &point))
			(*projected_count)++;
	}
	if (step->aim_pos_valid) {
		pos.x = step->aim_pos[0];
		pos.y = step->aim_pos[1];
		pos.z = step->aim_pos[2];
		g3_rotate_point(&point, &pos);
		(*candidate_count)++;
		if (draw_text_label("X", K_GUIDANCE_TARGET_COLOR, &point))
			(*projected_count)++;
	}
}
#endif

void automap_metadata_draw_labels(
    int *secret_candidate_count,
    int *secret_projected_count,
    int *objective_candidate_count,
    int *objective_projected_count)
{
	*secret_candidate_count = 0;
	*secret_projected_count = 0;
	draw_secret_labels(secret_candidate_count, secret_projected_count);

	*objective_candidate_count = 0;
	*objective_projected_count = 0;
	draw_objective_labels(objective_candidate_count, objective_projected_count);
#if defined(DXX_BUILD_DESCENT_II) && defined(__ANDROID__)
	draw_guidebot_guidance_labels(objective_candidate_count, objective_projected_count);
#endif
}

static const level_metadata_state *next_objective_route(
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

void automap_metadata_draw_next_objectives(int *objective_count)
{
	route_planner_plan_summary plan;
	const level_metadata_state *metadata;
	int first;
	int drawn = 0;
	int i;

	*objective_count = 0;
	if (!level_metadata_get_show_objectives())
		return;
	memset(&plan, 0, sizeof(plan));
	metadata = next_objective_route(&plan);
	if (!metadata)
		return;
	first = plan.first_pending_step;
	if (first < 0 || first >= metadata->route_step_count)
		return;
	gr_set_curfont(GAME_FONT);
	gr_set_fontcolor(K_NEXT_OBJECTIVE_COLOR, -1);
	for (i = first;
	     i < metadata->route_step_count && drawn < K_NEXT_OBJECTIVE_COUNT;
	     ++i) {
		const level_metadata_route_step *step = &metadata->route_steps[i];
		char text[LEVEL_METADATA_ROUTE_LABEL_LEN];

		if (step->kind == LEVEL_METADATA_ROUTE_START || !step->label[0])
			continue;
		snprintf(text, sizeof(text), "%s", step->label);
		fit_objective_text(
		    text, grd_curcanv->cv_bitmap.bm_w - FSPACX(4));
		gr_printf(FSPACX(2), FSPACY(2) + drawn * LINE_SPACING,
		          "%s", text);
		drawn++;
	}
	*objective_count = drawn;
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
