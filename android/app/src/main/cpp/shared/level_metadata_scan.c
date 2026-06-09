#include "level_metadata_scan.h"

#include <limits.h>
#include <string.h>

static int component_id[LEVEL_METADATA_MAX_SEGMENTS];
static int queue[LEVEL_METADATA_MAX_SEGMENTS];
static int energy_segments[LEVEL_METADATA_MAX_SEGMENTS];
static int energy_centers[LEVEL_METADATA_MAX_SEGMENTS][3];
static int energy_parent[LEVEL_METADATA_MAX_SEGMENTS];
static unsigned char energy_seen[LEVEL_METADATA_MAX_SEGMENTS];

void level_metadata_state_clear(level_metadata_state *state)
{
	if (state)
		memset(state, 0, sizeof(*state));
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

static long long distance_sq(const int a[3], const int b[3])
{
	long long dx = (long long) a[0] - b[0];
	long long dy = (long long) a[1] - b[1];
	long long dz = (long long) a[2] - b[2];

	return dx * dx + dy * dy + dz * dz;
}

static int sqrt_ll(long long value)
{
	int low = 0;
	int high = INT_MAX;
	int result = 0;

	while (low <= high) {
		int mid = low + (high - low) / 2;
		long long square = (long long) mid * mid;
		if (square <= value) {
			result = mid;
			low = mid + 1;
		} else {
			high = mid - 1;
		}
	}
	return result;
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
	long long nearest_raw_distance_sq = LLONG_MAX;
	int threshold;
	long long threshold_sq;
	int seg;
	int i;
	int j;

	if (view->segment_special_fuelcen == 0 || !view->segment_center)
		return;
	threshold = view->energy_center_group_distance > 0 ? view->energy_center_group_distance : LEVEL_METADATA_DEFAULT_ENERGY_CENTER_GROUP_DISTANCE;
	threshold_sq = (long long) threshold * threshold;
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
			long long center_distance_sq;
			if (component_i == component_j)
				continue;
			center_distance_sq = distance_sq(energy_centers[i], energy_centers[j]);
			if (center_distance_sq < nearest_raw_distance_sq)
				nearest_raw_distance_sq = center_distance_sq;
			if (center_distance_sq <= threshold_sq)
				energy_union(component_i, component_j);
		}
	}
	if (nearest_raw_distance_sq != LLONG_MAX)
		state->energy_center_nearest_raw_distance = sqrt_ll(nearest_raw_distance_sq);
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

int level_metadata_scan_level(const level_metadata_scan_view *view, level_metadata_state *state)
{
	level_metadata_state_clear(state);
	if (!state || !view_is_valid(view))
		return 0;
	collect_energy_center_stats(view, state);
	state->matcen_raw_count = count_connected_special_components(view, view->segment_special_robotmaker, &state->matcen_segment_count);
	state->matcen_count = state->matcen_segment_count;
	return state->energy_center_count;
}
