#include "level_metadata_scan.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

enum level_metadata_travel_status {
	LEVEL_METADATA_TRAVEL_OK = 0,
	LEVEL_METADATA_TRAVEL_PARTIAL = 1,
	LEVEL_METADATA_TRAVEL_FAILED = 2
};

typedef struct metadata_target {
	int seg;
	int pos[3];
	int visited;
} metadata_target;

typedef struct metadata_path {
	double distance;
	int first_locked_seg;
	int first_locked_side;
	int first_locked_key;
} metadata_path;

static int component_id[LEVEL_METADATA_MAX_SEGMENTS];
static int queue[LEVEL_METADATA_MAX_SEGMENTS];
static int energy_segments[LEVEL_METADATA_MAX_SEGMENTS];
static int energy_centers[LEVEL_METADATA_MAX_SEGMENTS][3];
static int energy_parent[LEVEL_METADATA_MAX_SEGMENTS];
static unsigned char energy_seen[LEVEL_METADATA_MAX_SEGMENTS];
static int segment_centers[LEVEL_METADATA_MAX_SEGMENTS][3];
static unsigned char segment_center_valid[LEVEL_METADATA_MAX_SEGMENTS];
static double route_distance[LEVEL_METADATA_MAX_SEGMENTS];
static int route_parent_seg[LEVEL_METADATA_MAX_SEGMENTS];
static int route_parent_side[LEVEL_METADATA_MAX_SEGMENTS];
static unsigned char route_closed[LEVEL_METADATA_MAX_SEGMENTS];
static int route_heap[LEVEL_METADATA_MAX_SEGMENTS + 1];
static int route_heap_pos[LEVEL_METADATA_MAX_SEGMENTS];
static metadata_target hostage_targets[LEVEL_METADATA_MAX_TARGETS];
static metadata_target key_targets[3][LEVEL_METADATA_MAX_TARGETS];
static int key_target_count[3];
static metadata_target exit_targets[LEVEL_METADATA_MAX_TARGETS];
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

const char *level_metadata_travel_status_name(int status)
{
	switch (status) {
		case LEVEL_METADATA_TRAVEL_OK:
			return "ok";
		case LEVEL_METADATA_TRAVEL_PARTIAL:
			return "partial";
		case LEVEL_METADATA_TRAVEL_FAILED:
			return "failed";
		default:
			return "unknown";
	}
}

static int valid_segment(const level_metadata_scan_view *view, int seg)
{
	return view && seg >= 0 && seg < view->num_segments;
}

static int valid_wall(const level_metadata_scan_view *view, int wall_num)
{
	return view && wall_num >= 0 && wall_num < view->num_walls;
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

static double point_distance(const int a[3], const int b[3])
{
	double dx = ((double) a[0] - (double) b[0]) / LEVEL_METADATA_FIX_SCALE;
	double dy = ((double) a[1] - (double) b[1]) / LEVEL_METADATA_FIX_SCALE;
	double dz = ((double) a[2] - (double) b[2]) / LEVEL_METADATA_FIX_SCALE;

	return sqrt(dx * dx + dy * dy + dz * dz);
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

static int key_index_for_wall_key(const level_metadata_scan_view *view, int wall_keys)
{
	if ((wall_keys & view->wall_key_blue) != 0)
		return 0;
	if ((wall_keys & view->wall_key_red) != 0)
		return 1;
	if ((wall_keys & view->wall_key_gold) != 0)
		return 2;
	return -1;
}

static int key_bit_for_index(int index)
{
	return index >= 0 && index < 3 ? 1 << index : 0;
}

static int wall_key_allowed(const level_metadata_scan_view *view, int wall_keys, int key_mask)
{
	int required = 0;

	if ((wall_keys & view->wall_key_blue) != 0)
		required |= key_bit_for_index(0);
	if ((wall_keys & view->wall_key_red) != 0)
		required |= key_bit_for_index(1);
	if ((wall_keys & view->wall_key_gold) != 0)
		required |= key_bit_for_index(2);
	return required == 0 || (required & key_mask) != 0;
}

static int powerup_key_index(const level_metadata_scan_view *view, int id)
{
	if (id == view->powerup_key_blue)
		return 0;
	if (id == view->powerup_key_red)
		return 1;
	if (id == view->powerup_key_gold)
		return 2;
	return -1;
}

static int side_has_exit(const level_metadata_scan_view *view, int seg, int side)
{
	int child;

	if (view->side_has_exit_trigger && view->side_has_exit_trigger(view->user, seg, side))
		return 1;
	child = view->segment_child(view->user, seg, side);
	return child == -2;
}

static int side_center(const level_metadata_scan_view *view, int seg, int side, int xyz[3])
{
	int corners[4][3];
	int i;

	if (!view->segment_vertex || side < 0 || side >= LEVEL_METADATA_MAX_SIDES || !xyz)
		return 0;
	for (i = 0; i < 4; ++i)
		if (!view->segment_vertex(view->user, seg, side_to_verts[side][i], corners[i]))
			return 0;
	xyz[0] = (int) (((long long) corners[0][0] + corners[1][0] + corners[2][0] + corners[3][0]) / 4);
	xyz[1] = (int) (((long long) corners[0][1] + corners[1][1] + corners[2][1] + corners[3][1]) / 4);
	xyz[2] = (int) (((long long) corners[0][2] + corners[1][2] + corners[2][2] + corners[3][2]) / 4);
	return 1;
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

static void heap_swap(int a, int b)
{
	int seg_a = route_heap[a];
	int seg_b = route_heap[b];

	route_heap[a] = seg_b;
	route_heap[b] = seg_a;
	route_heap_pos[seg_a] = b;
	route_heap_pos[seg_b] = a;
}

static void heap_sift_up(int index)
{
	while (index > 1) {
		int parent = index / 2;
		if (route_distance[route_heap[parent]] <= route_distance[route_heap[index]])
			break;
		heap_swap(parent, index);
		index = parent;
	}
}

static void heap_sift_down(int index, int size)
{
	for (;;) {
		int left = index * 2;
		int right = left + 1;
		int smallest = index;
		if (left <= size && route_distance[route_heap[left]] < route_distance[route_heap[smallest]])
			smallest = left;
		if (right <= size && route_distance[route_heap[right]] < route_distance[route_heap[smallest]])
			smallest = right;
		if (smallest == index)
			break;
		heap_swap(index, smallest);
		index = smallest;
	}
}

static void heap_push(int *size, int seg)
{
	route_heap[++*size] = seg;
	route_heap_pos[seg] = *size;
	heap_sift_up(*size);
}

static int heap_pop(int *size)
{
	int result = route_heap[1];

	route_heap_pos[result] = 0;
	if (*size == 1) {
		*size = 0;
		return result;
	}
	route_heap[1] = route_heap[*size];
	route_heap_pos[route_heap[1]] = 1;
	--*size;
	heap_sift_down(1, *size);
	return result;
}

static void heap_decrease(int seg)
{
	if (route_heap_pos[seg] > 0)
		heap_sift_up(route_heap_pos[seg]);
}

static int edge_required_missing_key(const level_metadata_scan_view *view, int seg, int side, int key_mask)
{
	int wall_num;
	int wall_keys;
	int key_index;

	if (!view->wall_num || !view->wall_keys)
		return -1;
	wall_num = view->wall_num(view->user, seg, side);
	if (!valid_wall(view, wall_num))
		return -1;
	wall_keys = view->wall_keys(view->user, wall_num);
	if (wall_keys == view->wall_key_none || wall_key_allowed(view, wall_keys, key_mask))
		return -1;
	key_index = key_index_for_wall_key(view, wall_keys);
	if (key_index < 0 || (key_mask & key_bit_for_index(key_index)) != 0)
		return -1;
	return key_index;
}

static int route_edge_passable(const level_metadata_scan_view *view, int seg, int side, int key_mask, int allow_missing_key, int forbidden_missing_key)
{
	int child = view->segment_child(view->user, seg, side);
	int wall_num;
	int wall_type;
	int wall_flags;
	int wall_keys;

	if (!edge_has_valid_reverse(view, seg, side, child))
		return 0;
	if (!view->wall_num || !view->wall_type || !view->wall_flags || !view->wall_keys)
		return 1;
	wall_num = view->wall_num(view->user, seg, side);
	if (!valid_wall(view, wall_num))
		return 1;
	wall_type = view->wall_type(view->user, wall_num);
	wall_flags = view->wall_flags(view->user, wall_num);
	wall_keys = view->wall_keys(view->user, wall_num);
	if (wall_keys == view->wall_key_none)
		return !side_has_exit(view, seg, side);
	if (wall_type == view->wall_type_open || wall_type == view->wall_type_blastable)
		return 1;
	if (wall_type == view->wall_type_illusion)
		return (wall_flags & view->wall_flag_illusion_off) == 0;
	if (wall_type == view->wall_type_door) {
		if (wall_key_allowed(view, wall_keys, key_mask))
			return 1;
		return allow_missing_key && key_index_for_wall_key(view, wall_keys) != forbidden_missing_key;
	}
	return 0;
}

static double edge_distance(const level_metadata_scan_view *view, int seg, int child)
{
	if (!valid_segment(view, seg) || !valid_segment(view, child) ||
	    !segment_center_valid[seg] || !segment_center_valid[child])
		return DBL_MAX;
	return point_distance(segment_centers[seg], segment_centers[child]);
}

static int find_shortest_path(
    const level_metadata_scan_view *view,
    int start_seg,
    const int start_pos[3],
    int goal_seg,
    const int goal_pos[3],
    int key_mask,
    int allow_missing_key,
    int forbidden_missing_key,
    metadata_path *path)
{
	int heap_size = 0;
	int seg;

	if (path) {
		path->distance = DBL_MAX;
		path->first_locked_seg = -1;
		path->first_locked_side = -1;
		path->first_locked_key = -1;
	}
	if (!valid_segment(view, start_seg) || !valid_segment(view, goal_seg) ||
	    !segment_center_valid[start_seg] || !segment_center_valid[goal_seg])
		return 0;
	for (seg = 0; seg < view->num_segments; ++seg) {
		route_distance[seg] = DBL_MAX;
		route_parent_seg[seg] = -1;
		route_parent_side[seg] = -1;
		route_closed[seg] = 0;
		route_heap_pos[seg] = 0;
	}
	route_distance[start_seg] = start_pos ? point_distance(start_pos, segment_centers[start_seg]) : 0.0;
	heap_push(&heap_size, start_seg);
	while (heap_size > 0) {
		int cur = heap_pop(&heap_size);
		int side;
		if (cur == goal_seg)
			break;
		route_closed[cur] = 1;
		for (side = 0; side < LEVEL_METADATA_MAX_SIDES; ++side) {
			int child = view->segment_child(view->user, cur, side);
			double step;
			double next_distance;
			if (!valid_segment(view, child) || route_closed[child])
				continue;
			if (!route_edge_passable(view, cur, side, key_mask, allow_missing_key, forbidden_missing_key))
				continue;
			step = edge_distance(view, cur, child);
			if (step == DBL_MAX)
				continue;
			next_distance = route_distance[cur] + step;
			if (next_distance >= route_distance[child])
				continue;
			route_distance[child] = next_distance;
			route_parent_seg[child] = cur;
			route_parent_side[child] = side;
			if (route_heap_pos[child])
				heap_decrease(child);
			else
				heap_push(&heap_size, child);
		}
	}
	if (route_distance[goal_seg] == DBL_MAX)
		return 0;
	if (path) {
		int reversed[LEVEL_METADATA_MAX_SEGMENTS];
		int count = 0;
		int cur = goal_seg;
		int i;
		path->distance = route_distance[goal_seg] + (goal_pos ? point_distance(segment_centers[goal_seg], goal_pos) : 0.0);
		while (valid_segment(view, cur) && count < LEVEL_METADATA_MAX_SEGMENTS) {
			reversed[count++] = cur;
			if (cur == start_seg)
				break;
			cur = route_parent_seg[cur];
		}
		for (i = count - 1; i > 0; --i) {
			int from = reversed[i];
			int to = reversed[i - 1];
			int side = route_parent_side[to];
			int key_index = edge_required_missing_key(view, from, side, key_mask);
			if (key_index >= 0) {
				path->first_locked_seg = from;
				path->first_locked_side = side;
				path->first_locked_key = key_index;
				break;
			}
		}
	}
	return 1;
}

static int append_target(const level_metadata_scan_view *view, metadata_target *targets, int *count, int max_count, int seg, const int pos[3])
{
	if (!targets || !count || *count >= max_count || !pos || !valid_segment(view, seg))
		return 0;
	targets[*count].seg = seg;
	copy_pos(targets[*count].pos, pos);
	targets[*count].visited = 0;
	++*count;
	return 1;
}

static int collect_route_targets(
    const level_metadata_scan_view *view,
    metadata_target *hostages,
    int *hostage_count,
    metadata_target *reactor,
    metadata_target *exits,
    int *exit_count)
{
	int obj_count;
	int objnum;
	int seg;
	int side;
	int found_reactor = 0;

	*hostage_count = 0;
	*exit_count = 0;
	memset(key_target_count, 0, sizeof(key_target_count));
	if (reactor)
		memset(reactor, 0, sizeof(*reactor));
	if (view->object_count && view->object_segment && view->object_type && view->object_position) {
		obj_count = view->object_count(view->user);
		for (objnum = 0; objnum < obj_count; ++objnum) {
			int type;
			int pos[3];
			int obj_seg;
			if (view->object_flags &&
			    (view->object_flags(view->user, objnum) & view->obj_flag_should_be_dead))
				continue;
			obj_seg = view->object_segment(view->user, objnum);
			if (!valid_segment(view, obj_seg) || !view->object_position(view->user, objnum, pos))
				continue;
			type = view->object_type(view->user, objnum);
			if (type == view->obj_type_hostage) {
				append_target(view, hostages, hostage_count, LEVEL_METADATA_MAX_TARGETS, obj_seg, pos);
			} else if (type == view->obj_type_control_center && reactor && !found_reactor) {
				reactor->seg = obj_seg;
				copy_pos(reactor->pos, pos);
				found_reactor = 1;
			} else if (type == view->obj_type_powerup && view->object_id) {
				int key_index = powerup_key_index(view, view->object_id(view->user, objnum));
				if (key_index >= 0)
					append_target(view, key_targets[key_index], &key_target_count[key_index], LEVEL_METADATA_MAX_TARGETS, obj_seg, pos);
			}
			if (view->object_contains_type && view->object_contains_id && view->object_contains_count &&
			    view->object_contains_count(view->user, objnum) > 0 &&
			    view->object_contains_type(view->user, objnum) == view->obj_type_powerup) {
				int key_index = powerup_key_index(view, view->object_contains_id(view->user, objnum));
				if (key_index >= 0)
					append_target(view, key_targets[key_index], &key_target_count[key_index], LEVEL_METADATA_MAX_TARGETS, obj_seg, pos);
			}
		}
	}
	if (!found_reactor && reactor && view->segment_special) {
		for (seg = 0; seg < view->num_segments; ++seg) {
			if (view->segment_special(view->user, seg) != view->segment_special_control_center ||
			    !segment_center_valid[seg])
				continue;
			reactor->seg = seg;
			copy_pos(reactor->pos, segment_centers[seg]);
			found_reactor = 1;
			break;
		}
	}
	for (seg = 0; seg < view->num_segments; ++seg) {
		for (side = 0; side < LEVEL_METADATA_MAX_SIDES; ++side) {
			int pos[3];
			if (!side_has_exit(view, seg, side))
				continue;
			if (!side_center(view, seg, side, pos) && segment_center_valid[seg])
				copy_pos(pos, segment_centers[seg]);
			append_target(view, exits, exit_count, LEVEL_METADATA_MAX_TARGETS, seg, pos);
		}
	}
	return found_reactor;
}

static int select_nearest_target(
    const level_metadata_scan_view *view,
    const metadata_target *targets,
    int count,
    int current_seg,
    const int current_pos[3],
    int key_mask,
    int allow_locked_fallback,
    int forbidden_missing_key,
    metadata_path *best_path)
{
	int best = -1;
	int pass;

	if (best_path)
		best_path->distance = DBL_MAX;
	for (pass = 0; pass < (allow_locked_fallback ? 2 : 1) && best < 0; ++pass) {
		int allow_missing_key = pass != 0;
		int i;
		for (i = 0; i < count; ++i) {
			metadata_path candidate;
			if (targets[i].visited)
				continue;
			if (!find_shortest_path(view, current_seg, current_pos, targets[i].seg, targets[i].pos, key_mask, allow_missing_key, forbidden_missing_key, &candidate))
				continue;
			if (candidate.distance >= best_path->distance)
				continue;
			best = i;
			*best_path = candidate;
		}
	}
	return best;
}

static const char *key_name(int key_index)
{
	return key_index == 0 ? "blue" : key_index == 1 ? "red"
	                             : key_index == 2   ? "gold"
	                                                : "unknown";
}

static int assume_key_acquired(int wanted_key, int *key_mask, level_metadata_state *state)
{
	*key_mask |= key_bit_for_index(wanted_key);
	state->travel_key_detours++;
	return 1;
}

static int acquire_key(
    const level_metadata_scan_view *view,
    int wanted_key,
    int *current_seg,
    int current_pos[3],
    int *key_mask,
    level_metadata_state *state,
    int in_progress)
{
	int guard;

	if ((*key_mask & key_bit_for_index(wanted_key)) != 0)
		return 1;
	if (wanted_key < 0 || wanted_key >= 3 || (in_progress & key_bit_for_index(wanted_key)) != 0) {
		snprintf(state->travel_problem, sizeof(state->travel_problem), "%s key dependency loop", key_name(wanted_key));
		return 0;
	}
	if (key_target_count[wanted_key] <= 0) {
		snprintf(state->travel_problem, sizeof(state->travel_problem), "%s key missing", key_name(wanted_key));
		return 0;
	}
	in_progress |= key_bit_for_index(wanted_key);
	for (guard = 0; guard < 8; ++guard) {
		metadata_path path;
		metadata_target *keys = key_targets[wanted_key];
		int key_index = select_nearest_target(view, keys, key_target_count[wanted_key], *current_seg, current_pos, *key_mask, 1, wanted_key, &path);
		if (key_index < 0) {
			return assume_key_acquired(wanted_key, key_mask, state);
		}
		if (find_shortest_path(view, *current_seg, current_pos, keys[key_index].seg, keys[key_index].pos, *key_mask, 0, -1, &path)) {
			state->travel_distance += path.distance;
			*current_seg = keys[key_index].seg;
			copy_pos(current_pos, keys[key_index].pos);
			keys[key_index].visited = 1;
			*key_mask |= key_bit_for_index(wanted_key);
			return 1;
		}
		if (!find_shortest_path(view, *current_seg, current_pos, keys[key_index].seg, keys[key_index].pos, *key_mask, 1, wanted_key, &path) ||
		    path.first_locked_key < 0) {
			metadata_path self_key_path;
			if (find_shortest_path(view, *current_seg, current_pos, keys[key_index].seg, keys[key_index].pos, *key_mask, 1, -1, &self_key_path) &&
			    self_key_path.first_locked_key == wanted_key) {
				state->travel_distance += self_key_path.distance;
				*current_seg = keys[key_index].seg;
				copy_pos(current_pos, keys[key_index].pos);
				keys[key_index].visited = 1;
				*key_mask |= key_bit_for_index(wanted_key);
				state->travel_key_detours++;
				return 1;
			}
			return assume_key_acquired(wanted_key, key_mask, state);
		}
		{
			metadata_target door_target;
			metadata_path door_path;
			metadata_path return_path;

			door_target.seg = path.first_locked_seg;
			copy_pos(door_target.pos, segment_centers[path.first_locked_seg]);
			door_target.visited = 0;
			if (!find_shortest_path(view, *current_seg, current_pos, door_target.seg, door_target.pos, *key_mask, 0, -1, &door_path)) {
				snprintf(state->travel_problem, sizeof(state->travel_problem), "%s key door approach unreachable", key_name(wanted_key));
				return 0;
			}
			state->travel_distance += door_path.distance;
			*current_seg = door_target.seg;
			copy_pos(current_pos, door_target.pos);
			if (!acquire_key(view, path.first_locked_key, current_seg, current_pos, key_mask, state, in_progress))
				return 0;
			if (!find_shortest_path(view, *current_seg, current_pos, door_target.seg, door_target.pos, *key_mask, 0, -1, &return_path)) {
				snprintf(state->travel_problem, sizeof(state->travel_problem), "%s", "could not return to locked door after key");
				return 0;
			}
			state->travel_distance += return_path.distance;
			*current_seg = door_target.seg;
			copy_pos(current_pos, door_target.pos);
			state->travel_key_detours++;
		}
	}
	snprintf(state->travel_problem, sizeof(state->travel_problem), "%s key detour limit", key_name(wanted_key));
	return 0;
}

static int route_to_target(
    const level_metadata_scan_view *view,
    int *current_seg,
    int current_pos[3],
    const metadata_target *target,
    int *key_mask,
    level_metadata_state *state)
{
	int guard;

	for (guard = 0; guard < 8; ++guard) {
		metadata_path path;
		if (find_shortest_path(view, *current_seg, current_pos, target->seg, target->pos, *key_mask, 0, -1, &path)) {
			state->travel_distance += path.distance;
			*current_seg = target->seg;
			copy_pos(current_pos, target->pos);
			return 1;
		}
		if (!find_shortest_path(view, *current_seg, current_pos, target->seg, target->pos, *key_mask, 1, -1, &path)) {
			snprintf(state->travel_problem, sizeof(state->travel_problem), "%s", "target unreachable or blocked by unsupported door");
			return 0;
		}
		if (path.first_locked_key < 0) {
			snprintf(state->travel_problem, sizeof(state->travel_problem), "%s", "target unreachable");
			return 0;
		}
		if (key_target_count[path.first_locked_key] <= 0) {
			snprintf(state->travel_problem, sizeof(state->travel_problem), "%s key missing",
			         path.first_locked_key == 0 ? "blue" : path.first_locked_key == 1 ? "red"
			                                                                          : "gold");
			return 0;
		}
		{
			metadata_target door_target;
			metadata_path door_path;
			metadata_path return_path;

			door_target.seg = path.first_locked_seg;
			copy_pos(door_target.pos, segment_centers[path.first_locked_seg]);
			door_target.visited = 0;
			if (!find_shortest_path(view, *current_seg, current_pos, door_target.seg, door_target.pos, *key_mask, 0, -1, &door_path)) {
				snprintf(state->travel_problem, sizeof(state->travel_problem), "%s", "locked door approach unreachable");
				return 0;
			}
			state->travel_distance += door_path.distance;
			*current_seg = door_target.seg;
			copy_pos(current_pos, door_target.pos);
			if (!acquire_key(view, path.first_locked_key, current_seg, current_pos, key_mask, state, 0))
				return 0;
			if (!find_shortest_path(view, *current_seg, current_pos, door_target.seg, door_target.pos, *key_mask, 0, -1, &return_path)) {
				snprintf(state->travel_problem, sizeof(state->travel_problem), "%s", "could not return to locked door after key");
				return 0;
			}
			state->travel_distance += return_path.distance;
			*current_seg = door_target.seg;
			copy_pos(current_pos, door_target.pos);
			state->travel_key_detours++;
		}
	}
	snprintf(state->travel_problem, sizeof(state->travel_problem), "%s", "too many key detours");
	return 0;
}

static void collect_travel_time(const level_metadata_scan_view *view, level_metadata_state *state)
{
	metadata_target reactor;
	int hostage_count = 0;
	int exit_count = 0;
	int found_reactor;
	int current_seg = view->start_segment;
	int current_pos[3];
	int key_mask = 0;
	int i;

	state->travel_status = LEVEL_METADATA_TRAVEL_FAILED;
	if (!valid_segment(view, current_seg) || !view->start_position || !view->start_position(view->user, current_pos)) {
		snprintf(state->travel_problem, sizeof(state->travel_problem), "%s", "missing player start");
		return;
	}
	found_reactor = collect_route_targets(view, hostage_targets, &hostage_count, &reactor, exit_targets, &exit_count);
	state->travel_targets_total = hostage_count + (found_reactor ? 1 : 0) + (exit_count > 0 ? 1 : 0);
	if (!found_reactor) {
		snprintf(state->travel_problem, sizeof(state->travel_problem), "%s", "missing reactor");
		return;
	}
	if (exit_count <= 0) {
		snprintf(state->travel_problem, sizeof(state->travel_problem), "%s", "missing exit");
		return;
	}
	for (i = 0; i < hostage_count; ++i) {
		metadata_path best_path;
		int target_index = select_nearest_target(view, hostage_targets, hostage_count, current_seg, current_pos, key_mask, 1, -1, &best_path);
		if (target_index < 0)
			break;
		if (!route_to_target(view, &current_seg, current_pos, &hostage_targets[target_index], &key_mask, state))
			break;
		hostage_targets[target_index].visited = 1;
		state->travel_targets_reached++;
	}
	if (state->travel_targets_reached < hostage_count) {
		if (!state->travel_problem[0])
			snprintf(state->travel_problem, sizeof(state->travel_problem), "%s", "hostage unreachable");
		state->travel_status = state->travel_targets_reached > 0 || state->travel_distance > 0.0 ? LEVEL_METADATA_TRAVEL_PARTIAL : LEVEL_METADATA_TRAVEL_FAILED;
	} else if (route_to_target(view, &current_seg, current_pos, &reactor, &key_mask, state)) {
		metadata_path exit_path;
		int exit_index;
		state->travel_targets_reached++;
		exit_index = select_nearest_target(view, exit_targets, exit_count, current_seg, current_pos, key_mask, 1, -1, &exit_path);
		if (exit_index >= 0 && route_to_target(view, &current_seg, current_pos, &exit_targets[exit_index], &key_mask, state)) {
			state->travel_targets_reached++;
			state->travel_status = LEVEL_METADATA_TRAVEL_OK;
			state->travel_problem[0] = '\0';
		} else {
			if (!state->travel_problem[0])
				snprintf(state->travel_problem, sizeof(state->travel_problem), "%s", "exit unreachable");
			state->travel_status = LEVEL_METADATA_TRAVEL_PARTIAL;
		}
	} else {
		state->travel_status = state->travel_targets_reached > 0 || state->travel_distance > 0.0 ? LEVEL_METADATA_TRAVEL_PARTIAL : LEVEL_METADATA_TRAVEL_FAILED;
	}
	state->travel_time_seconds = (int) floor(state->travel_distance / LEVEL_METADATA_SHIP_SPEED_UNITS_PER_SECOND + 0.5);
}

int level_metadata_scan_level(const level_metadata_scan_view *view, level_metadata_state *state)
{
	level_metadata_state_clear(state);
	if (!state || !view_is_valid(view))
		return 0;
	collect_segment_centers(view);
	collect_mine_volume(view, state);
	collect_energy_center_stats(view, state);
	state->matcen_raw_count = count_connected_special_components(view, view->segment_special_robotmaker, &state->matcen_segment_count);
	state->matcen_count = state->matcen_segment_count;
	collect_travel_time(view, state);
	return state->energy_center_count;
}
