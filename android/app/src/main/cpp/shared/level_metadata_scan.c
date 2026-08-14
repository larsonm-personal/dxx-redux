#include "level_metadata_scan.h"
#include "route_planner_c.h"

#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int component_id[LEVEL_METADATA_MAX_SEGMENTS];
static int queue[LEVEL_METADATA_MAX_SEGMENTS];
static int energy_segments[LEVEL_METADATA_MAX_SEGMENTS];
static int energy_centers[LEVEL_METADATA_MAX_SEGMENTS][3];
static int energy_parent[LEVEL_METADATA_MAX_SEGMENTS];
static unsigned char energy_seen[LEVEL_METADATA_MAX_SEGMENTS];
static int segment_centers[LEVEL_METADATA_MAX_SEGMENTS][3];
static unsigned char segment_center_valid[LEVEL_METADATA_MAX_SEGMENTS];
static int side_to_verts[LEVEL_METADATA_MAX_SIDES][4] = {
	{ 7, 6, 2, 3 },
	{ 0, 4, 7, 3 },
	{ 0, 1, 5, 4 },
	{ 2, 6, 5, 1 },
	{ 4, 5, 6, 7 },
	{ 3, 2, 1, 0 }
};

void level_metadata_state_clear(level_metadata_state *state)
{
	if (state)
		memset(state, 0, sizeof(*state));
}

const char *level_metadata_route_status_name(int status)
{
	switch (status) {
		case LEVEL_METADATA_ROUTE_OK:
			return "ok";
		case LEVEL_METADATA_ROUTE_PARTIAL:
			return "partial";
		case LEVEL_METADATA_ROUTE_FAILED:
			return "failed";
		default:
			return "unknown";
	}
}

const char *level_metadata_route_step_kind_name(int kind)
{
	switch (kind) {
		case LEVEL_METADATA_ROUTE_START:
			return "start";
		case LEVEL_METADATA_ROUTE_KEY:
			return "key";
		case LEVEL_METADATA_ROUTE_TRIGGER:
			return "trigger";
		case LEVEL_METADATA_ROUTE_REACTOR:
			return "reactor";
		case LEVEL_METADATA_ROUTE_BOSS:
			return "boss";
		case LEVEL_METADATA_ROUTE_EXIT:
			return "exit";
		case LEVEL_METADATA_ROUTE_HIDDEN_DOOR:
			return "hidden_door";
		case LEVEL_METADATA_ROUTE_UNEXPLORED:
			return "unexplored";
		case LEVEL_METADATA_ROUTE_BLASTABLE_WALL:
			return "blastable_wall";
		default:
			return "unknown";
	}
}

const char *level_metadata_route_activation_kind_name(int kind)
{
	switch (kind) {
		case LEVEL_METADATA_ROUTE_ACTIVATION_NONE:
			return "none";
		case LEVEL_METADATA_ROUTE_ACTIVATION_PICKUP_KEY:
			return "pickup_key";
		case LEVEL_METADATA_ROUTE_ACTIVATION_SHOOT_SWITCH:
			return "shoot_switch";
		case LEVEL_METADATA_ROUTE_ACTIVATION_FLY_THROUGH_TRIGGER:
			return "fly_through_trigger";
		case LEVEL_METADATA_ROUTE_ACTIVATION_PASS_THROUGH_TRIGGER:
			return "pass_through_trigger";
		case LEVEL_METADATA_ROUTE_ACTIVATION_OPEN_HIDDEN_DOOR:
			return "open_hidden_door";
		case LEVEL_METADATA_ROUTE_ACTIVATION_DESTROY_REACTOR:
			return "destroy_reactor";
		case LEVEL_METADATA_ROUTE_ACTIVATION_DESTROY_BOSS:
			return "destroy_boss";
		case LEVEL_METADATA_ROUTE_ACTIVATION_ENTER_EXIT:
			return "enter_exit";
		case LEVEL_METADATA_ROUTE_ACTIVATION_DESTROY_BLASTABLE_WALL:
			return "destroy_blastable_wall";
		case LEVEL_METADATA_ROUTE_ACTIVATION_DESTROY_KEY_CARRIER:
			return "destroy_key_carrier";
		case LEVEL_METADATA_ROUTE_ACTIVATION_UNRESOLVED_TRIGGER:
			return "unresolved_trigger";
		default:
			return "unknown";
	}
}

static int level_metadata_route_wall_completed(
    const level_metadata_scan_view *view,
    int wall)
{
	int segment;
	int side;
	int type;
	int flags;

	if (!view || wall < 0 || wall >= view->num_walls ||
	    !view->wall_segment || !view->wall_side || !view->wall_type ||
	    !view->wall_flags)
		return 0;
	segment = view->wall_segment(view->user, wall);
	side = view->wall_side(view->user, wall);
	type = view->wall_type(view->user, wall);
	flags = view->wall_flags(view->user, wall);
	return type == view->wall_type_open ||
	       (flags & view->wall_flag_door_opened) != 0 ||
	       (view->side_is_flyable && segment >= 0 && side >= 0 &&
	        view->side_is_flyable(view->user, segment, side));
}

int level_metadata_route_step_completed_by_world_state(
    const level_metadata_scan_view *view,
    const level_metadata_route_step *step)
{
	int link;

	if (!view || !step)
		return 0;
	switch (step->kind) {
		case LEVEL_METADATA_ROUTE_START:
			return 1;
		case LEVEL_METADATA_ROUTE_KEY:
			return step->key_index >= 0 && step->key_index < 3 &&
			       (view->initial_key_mask & (1 << step->key_index)) != 0;
		case LEVEL_METADATA_ROUTE_REACTOR:
		case LEVEL_METADATA_ROUTE_BOSS:
			return view->initial_control_center_destroyed != 0;
		case LEVEL_METADATA_ROUTE_TRIGGER:
			if (step->opened_link_count <= 0)
				return 0;
			for (link = 0; link < step->opened_link_count; ++link) {
				int wall = step->opened_link_wall[link];
				if (step->trigger_type == view->trigger_type_unlock_door &&
				    wall >= 0 && wall < view->num_walls && view->wall_flags &&
				    (view->wall_flags(view->user, wall) &
				     view->wall_flag_door_locked) == 0)
					return 1;
				if (level_metadata_route_wall_completed(view, wall))
					return 1;
			}
			return 0;
		case LEVEL_METADATA_ROUTE_HIDDEN_DOOR:
		case LEVEL_METADATA_ROUTE_BLASTABLE_WALL:
			return level_metadata_route_wall_completed(view, step->wall_num);
		default:
			return 0;
	}
}

static int valid_segment(const level_metadata_scan_view *view, int seg)
{
	return view && seg >= 0 && seg < view->num_segments;
}

static int edge_has_valid_reverse(const level_metadata_scan_view *view, int seg, int side, int child)
{
	int reverse_side;

	if (!valid_segment(view, child) || !view->reverse_side)
		return 0;
	reverse_side = view->reverse_side(view->user, seg, child);
	if (reverse_side < 0 || reverse_side >= LEVEL_METADATA_MAX_SIDES)
		return 0;
	return view->segment_child(view->user, child, reverse_side) == seg;
}

static int energy_find(int component)
{
	int parent = energy_parent[component];

	if (parent == component)
		return component;
	parent = energy_find(parent);
	energy_parent[component] = parent;
	return parent;
}

static void energy_union(int a, int b)
{
	int root_a = energy_find(a);
	int root_b = energy_find(b);

	if (root_a != root_b)
		energy_parent[root_b] = root_a;
}

/*
 * The serialized nearest-distance field is an int, so keep squared distances
 * within that declared domain and use one larger value as an out-of-range
 * sentinel; the sentinel is important when the grouping threshold is
 * INT_MAX: a genuinely larger distance must not compare as equal after the
 * reported value is saturated
 */
static uint64_t maximum_distance_sq(void)
{
	return (uint64_t) INT_MAX * (uint64_t) INT_MAX;
}

static uint64_t distance_sq_bounded(const int a[3], const int b[3])
{
	const uint64_t limit = maximum_distance_sq();
	uint64_t sum = 0;
	int axis;

	for (axis = 0; axis < 3; ++axis) {
		int64_t delta = (int64_t) a[axis] - (int64_t) b[axis];
		uint64_t magnitude = delta < 0 ? (uint64_t) -delta : (uint64_t) delta;
		uint64_t square;

		if (magnitude > (uint64_t) INT_MAX)
			return limit + 1;
		square = magnitude * magnitude;
		if (square > limit - sum)
			return limit + 1;
		sum += square;
	}
	return sum;
}

static int sqrt_distance_sq(uint64_t value)
{
	uint64_t low = 0;
	uint64_t high = INT_MAX;
	int result = 0;

	if (value >= maximum_distance_sq())
		return INT_MAX;
	while (low <= high) {
		uint64_t mid = low + (high - low) / 2;
		uint64_t square = mid * mid;
		if (square <= value) {
			result = (int) mid;
			low = mid + 1;
		} else {
			high = mid - 1;
		}
	}
	return result;
}

static double triangle_tetra_volume(const int center[3], const int a[3], const int b[3], const int c[3])
{
	double ax = ((double) a[0] - (double) center[0]) / LEVEL_METADATA_FIX_SCALE;
	double ay = ((double) a[1] - (double) center[1]) / LEVEL_METADATA_FIX_SCALE;
	double az = ((double) a[2] - (double) center[2]) / LEVEL_METADATA_FIX_SCALE;
	double bx = ((double) b[0] - (double) center[0]) / LEVEL_METADATA_FIX_SCALE;
	double by = ((double) b[1] - (double) center[1]) / LEVEL_METADATA_FIX_SCALE;
	double bz = ((double) b[2] - (double) center[2]) / LEVEL_METADATA_FIX_SCALE;
	double cx = ((double) c[0] - (double) center[0]) / LEVEL_METADATA_FIX_SCALE;
	double cy = ((double) c[1] - (double) center[1]) / LEVEL_METADATA_FIX_SCALE;
	double cz = ((double) c[2] - (double) center[2]) / LEVEL_METADATA_FIX_SCALE;
	double cross_x = by * cz - bz * cy;
	double cross_y = bz * cx - bx * cz;
	double cross_z = bx * cy - by * cx;
	double dot = ax * cross_x + ay * cross_y + az * cross_z;

	return fabs(dot) / 6.0;
}

static void copy_pos(int dest[3], const int src[3])
{
	dest[0] = src[0];
	dest[1] = src[1];
	dest[2] = src[2];
}

static int view_is_valid(const level_metadata_scan_view *view)
{
	return view &&
	       view->num_segments > 0 &&
	       view->num_segments <= LEVEL_METADATA_MAX_SEGMENTS &&
	       view->segment_child &&
	       view->reverse_side &&
	       view->segment_special;
}

static int count_connected_special_components(const level_metadata_scan_view *view, int special, int *segment_count)
{
	int special_count = 0;
	int component_count = 0;
	int seg;
	int i;

	if (segment_count)
		*segment_count = 0;
	if (special == 0)
		return 0;
	for (seg = 0; seg < view->num_segments; ++seg) {
		component_id[seg] = -1;
		if (view->segment_special(view->user, seg) != special)
			continue;
		energy_segments[special_count++] = seg;
	}
	if (segment_count)
		*segment_count = special_count;
	for (i = 0; i < special_count; ++i) {
		int head;
		int tail;
		if (component_id[energy_segments[i]] >= 0)
			continue;
		head = 0;
		tail = 0;
		component_id[energy_segments[i]] = component_count;
		queue[tail++] = energy_segments[i];
		while (head < tail) {
			int cur = queue[head++];
			int side;
			for (side = 0; side < LEVEL_METADATA_MAX_SIDES; ++side) {
				int child = view->segment_child(view->user, cur, side);
				if (!valid_segment(view, child) ||
				    component_id[child] >= 0 ||
				    view->segment_special(view->user, child) != special ||
				    !edge_has_valid_reverse(view, cur, side, child))
					continue;
				component_id[child] = component_count;
				queue[tail++] = child;
			}
		}
		component_count++;
	}
	return component_count;
}

static void collect_energy_center_stats(const level_metadata_scan_view *view, level_metadata_state *state)
{
	int fuel_count = 0;
	int raw_count = 0;
	int group_count = 0;
	const uint64_t out_of_range_distance_sq = maximum_distance_sq() + 1;
	uint64_t nearest_raw_distance_sq = out_of_range_distance_sq;
	int threshold;
	uint64_t threshold_sq;
	int seg;
	int i;
	int j;

	if (view->segment_special_fuelcen == 0 || !view->segment_center)
		return;
	threshold = view->energy_center_group_distance > 0 ? view->energy_center_group_distance : LEVEL_METADATA_DEFAULT_ENERGY_CENTER_GROUP_DISTANCE;
	threshold_sq = (uint64_t) threshold * (uint64_t) threshold;
	state->energy_center_group_distance = threshold;
	for (seg = 0; seg < view->num_segments; ++seg) {
		component_id[seg] = -1;
		if (view->segment_special(view->user, seg) != view->segment_special_fuelcen)
			continue;
		energy_segments[fuel_count] = seg;
		if (!view->segment_center(view->user, seg, energy_centers[fuel_count])) {
			energy_centers[fuel_count][0] = 0;
			energy_centers[fuel_count][1] = 0;
			energy_centers[fuel_count][2] = 0;
		}
		fuel_count++;
	}
	state->energy_center_segment_count = fuel_count;
	for (i = 0; i < fuel_count; ++i) {
		int head;
		int tail;
		if (component_id[energy_segments[i]] >= 0)
			continue;
		head = 0;
		tail = 0;
		energy_parent[raw_count] = raw_count;
		component_id[energy_segments[i]] = raw_count;
		queue[tail++] = energy_segments[i];
		while (head < tail) {
			int cur = queue[head++];
			int side;
			for (side = 0; side < LEVEL_METADATA_MAX_SIDES; ++side) {
				int child = view->segment_child(view->user, cur, side);
				if (!valid_segment(view, child) ||
				    component_id[child] >= 0 ||
				    view->segment_special(view->user, child) != view->segment_special_fuelcen ||
				    !edge_has_valid_reverse(view, cur, side, child))
					continue;
				component_id[child] = raw_count;
				queue[tail++] = child;
			}
		}
		raw_count++;
	}
	state->energy_center_raw_count = raw_count;
	for (i = 0; i < fuel_count; ++i) {
		int seg_i = energy_segments[i];
		for (j = i + 1; j < fuel_count; ++j) {
			int seg_j = energy_segments[j];
			int component_i = component_id[seg_i];
			int component_j = component_id[seg_j];
			uint64_t center_distance_sq;
			if (component_i == component_j)
				continue;
			center_distance_sq = distance_sq_bounded(energy_centers[i], energy_centers[j]);
			if (center_distance_sq < nearest_raw_distance_sq)
				nearest_raw_distance_sq = center_distance_sq;
			if (center_distance_sq <= threshold_sq)
				energy_union(component_i, component_j);
		}
	}
	if (raw_count > 1)
		state->energy_center_nearest_raw_distance = sqrt_distance_sq(nearest_raw_distance_sq);
	memset(energy_seen, 0, sizeof(energy_seen));
	for (i = 0; i < raw_count; ++i) {
		int root = energy_find(i);
		if (energy_seen[root])
			continue;
		energy_seen[root] = 1;
		group_count++;
	}
	state->energy_center_count = group_count;
}

static void collect_segment_centers(const level_metadata_scan_view *view)
{
	int seg;

	memset(segment_center_valid, 0, sizeof(segment_center_valid));
	if (!view->segment_center)
		return;
	for (seg = 0; seg < view->num_segments; ++seg)
		if (view->segment_center(view->user, seg, segment_centers[seg]))
			segment_center_valid[seg] = 1;
}

static void collect_mine_volume(const level_metadata_scan_view *view, level_metadata_state *state)
{
	double volume = 0.0;
	int seg;

	if (!view->segment_vertex)
		return;
	for (seg = 0; seg < view->num_segments; ++seg) {
		int center[3];
		int side;

		if (!segment_center_valid[seg])
			continue;
		copy_pos(center, segment_centers[seg]);
		for (side = 0; side < LEVEL_METADATA_MAX_SIDES; ++side) {
			int v[4][3];
			int i;
			for (i = 0; i < 4; ++i)
				if (!view->segment_vertex(view->user, seg, side_to_verts[side][i], v[i]))
					break;
			if (i != 4)
				continue;
			volume += triangle_tetra_volume(center, v[0], v[1], v[2]);
			volume += triangle_tetra_volume(center, v[0], v[2], v[3]);
		}
	}
	state->mine_volume = volume;
	if (LEVEL_METADATA_D1_LEVEL1_VOLUME_BASELINE > 0.0)
		state->mine_volume_normalized = volume / LEVEL_METADATA_D1_LEVEL1_VOLUME_BASELINE;
}

static void collect_guidebot_info(const level_metadata_scan_view *view, level_metadata_state *state)
{
	int obj_count;
	int objnum;

	if (!view->object_is_companion || !view->object_count || !view->object_segment)
		return;
	obj_count = view->object_count(view->user);
	for (objnum = 0; objnum < obj_count; ++objnum) {
		int obj_seg;
		if (view->object_flags &&
		    (view->object_flags(view->user, objnum) & view->obj_flag_should_be_dead))
			continue;
		if (!view->object_is_companion(view->user, objnum))
			continue;
		state->guidebot_count++;
		if (state->guidebot_accessible)
			continue;
		obj_seg = view->object_segment(view->user, objnum);
		if (!valid_segment(view, obj_seg))
			continue;
		if (obj_seg == view->start_segment)
			state->guidebot_accessible = 1;
		else if (!view->defer_guidebot_accessibility &&
		         route_planner_segment_reachable_view(view, obj_seg))
			state->guidebot_accessible = 1;
	}
	if (state->guidebot_count <= 0) {
		snprintf(
		    state->guidebot_placement_note,
		    sizeof(state->guidebot_placement_note),
		    "%s",
		    "no guidebot or guidebot start cage placed in this level");
	} else if (!state->guidebot_accessible &&
	           !view->defer_guidebot_accessibility) {
		state->guidebot_placed = 1;
		snprintf(
		    state->guidebot_placement_note,
		    sizeof(state->guidebot_placement_note),
		    "%s",
		    "guidebot or guidebot start cage placed in this level");
		snprintf(state->guidebot_note, sizeof(state->guidebot_note), "%s", "guidebot is present but not reachable from the start");
	} else {
		state->guidebot_placed = 1;
		snprintf(
		    state->guidebot_placement_note,
		    sizeof(state->guidebot_placement_note),
		    "%s",
		    "guidebot or guidebot start cage placed in this level");
	}
}

int level_metadata_scan_level_summary(const level_metadata_scan_view *view, level_metadata_state *state)
{
	level_metadata_state_clear(state);
	if (!state || !view_is_valid(view))
		return 0;
	collect_segment_centers(view);
	collect_mine_volume(view, state);
	collect_energy_center_stats(view, state);
	state->matcen_raw_count = count_connected_special_components(view, view->segment_special_robotmaker, &state->matcen_segment_count);
	state->matcen_count = state->matcen_segment_count;
	collect_guidebot_info(view, state);
	return state->energy_center_count;
}

int level_metadata_scan_level(const level_metadata_scan_view *view, level_metadata_state *state)
{
	level_metadata_state route;
	route_planner_plan_summary summary;
	char problem[sizeof(state->route_problem)];
	int energy_center_count = level_metadata_scan_level_summary(view, state);

	if (!state || !view_is_valid(view))
		return 0;
	level_metadata_state_clear(&route);
	memset(&summary, 0, sizeof(summary));
	if (!route_planner_plan_view(
	        view, ROUTE_PLANNER_ENDPOINT_END_OF_LEVEL, -1, &route, NULL,
	        &summary, problem, sizeof(problem))) {
		state->route_status = LEVEL_METADATA_ROUTE_FAILED;
		snprintf(state->route_problem, sizeof(state->route_problem), "%s",
		         problem[0] ? problem : "shared route planning failed");
		return energy_center_count;
	}
	state->travel_distance = route.travel_distance;
	state->travel_time_seconds = route.travel_time_seconds;
	state->route_status = route.route_status;
	snprintf(state->route_problem, sizeof(state->route_problem), "%s",
	         route.route_problem);
	snprintf(state->route_note, sizeof(state->route_note), "%s",
	         route.route_note);
	state->unnecessary_key_mask = route.unnecessary_key_mask;
	state->route_step_count = route.route_step_count;
	memcpy(state->route_steps, route.route_steps,
	       sizeof(state->route_steps[0]) * route.route_step_count);
	return energy_center_count;
}
