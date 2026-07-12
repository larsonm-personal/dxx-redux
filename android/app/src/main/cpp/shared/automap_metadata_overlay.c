#include <stdio.h>

#include "3d.h"
#include "segment.h"
#include "object.h"
#include "automap.h"
#include "automap_metadata_overlay.h"
#include "gamefont.h"
#include "gameseg.h"
#include "gr.h"
#include "secretarea.h"

#define K_SECRET_LABEL_UNFOUND_COLOR BM_XRGB(31, 0, 0)
#define K_SECRET_LABEL_FOUND_COLOR   BM_XRGB(0, 31, 0)
#define K_OBJECTIVE_LABEL_COLOR      BM_XRGB(10, 63, 63)
#define K_OBJECTIVE_LABEL_BLUE       BM_XRGB(5, 5, 63)
#define K_OBJECTIVE_LABEL_GOLD       BM_XRGB(63, 63, 10)
#define K_OBJECTIVE_LABEL_RED        BM_XRGB(63, 5, 5)

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
		vms_vector pos;
		g3s_point point;

		if (step->kind == LEVEL_METADATA_ROUTE_START)
			continue;
		objective_number++;
		if (!step->label_pos_valid)
			continue;
		snprintf(label, sizeof(label), "%d", objective_number);
		pos.x = step->label_pos[0];
		pos.y = step->label_pos[1];
		pos.z = step->label_pos[2];
		g3_rotate_point(&point, &pos);
		(*candidate_count)++;
		if (draw_text_label(label, objective_label_color(step), &point))
			(*projected_count)++;
	}
}

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
