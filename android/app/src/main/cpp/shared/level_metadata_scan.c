#include "level_metadata_scan.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

enum level_metadata_route_status {
	LEVEL_METADATA_ROUTE_OK = 0,
	LEVEL_METADATA_ROUTE_PARTIAL = 1,
	LEVEL_METADATA_ROUTE_FAILED = 2
};

enum metadata_route_block_kind {
	METADATA_ROUTE_BLOCK_NONE = 0,
	METADATA_ROUTE_BLOCK_KEY = 1,
	METADATA_ROUTE_BLOCK_TRIGGER = 2,
	METADATA_ROUTE_BLOCK_HIDDEN_DOOR = 3
};

typedef struct metadata_target {
	int seg;
	int pos[3];
	int visited;
} metadata_target;

typedef struct metadata_route_block {
	int kind;
	int key_index;
	int seg;
	int side;
	int wall_num;
	int source_wall;
	int source_seg;
	int source_side;
	int trigger_num;
	int trigger_type;
} metadata_route_block;

typedef struct metadata_route_path {
	double distance;
	int progress_weight;
	metadata_route_block first_block;
	int terminal_seg;
	int terminal_pos[3];
	int terminal_pos_valid;
} metadata_route_path;

typedef struct metadata_route_context {
	int current_seg;
	int current_pos[3];
	int key_mask;
	int key_in_progress;
	int avoided_key_mask;
	int control_center_destroyed;
	int unresolved_block_valid;
	int failed_trigger;
	int failed_key;
	metadata_route_block unresolved_block;
	double pending_distance;
	unsigned char fired_triggers[256];
	unsigned char trigger_in_progress[256];
	unsigned char avoided_triggers[256];
	unsigned char opened_hidden_walls[LEVEL_METADATA_MAX_WALLS];
	unsigned char hidden_door_in_progress[LEVEL_METADATA_MAX_WALLS];
} metadata_route_context;

typedef struct metadata_route_progress_snapshot {
	metadata_route_context route;
	int route_step_count;
	char route_problem[128];
	level_metadata_route_step route_steps[LEVEL_METADATA_MAX_ROUTE_STEPS];
	unsigned char key_target_visited[3][LEVEL_METADATA_MAX_TARGETS];
} metadata_route_progress_snapshot;

static int component_id[LEVEL_METADATA_MAX_SEGMENTS];
static int queue[LEVEL_METADATA_MAX_SEGMENTS];
static int unexplored_component_size[LEVEL_METADATA_MAX_SEGMENTS];
static int unexplored_component_target[LEVEL_METADATA_MAX_SEGMENTS];
static double unexplored_component_distance[LEVEL_METADATA_MAX_SEGMENTS];
static unsigned char unexplored_component_direct[LEVEL_METADATA_MAX_SEGMENTS];
static unsigned char unexplored_component_tried[LEVEL_METADATA_MAX_SEGMENTS];
static int energy_segments[LEVEL_METADATA_MAX_SEGMENTS];
static int energy_centers[LEVEL_METADATA_MAX_SEGMENTS][3];
static int energy_parent[LEVEL_METADATA_MAX_SEGMENTS];
static unsigned char energy_seen[LEVEL_METADATA_MAX_SEGMENTS];
static int segment_centers[LEVEL_METADATA_MAX_SEGMENTS][3];
static unsigned char segment_center_valid[LEVEL_METADATA_MAX_SEGMENTS];
static double route_distance[LEVEL_METADATA_MAX_SEGMENTS];
static int route_progress_weight[LEVEL_METADATA_MAX_SEGMENTS];
static int route_parent_seg[LEVEL_METADATA_MAX_SEGMENTS];
static int route_parent_side[LEVEL_METADATA_MAX_SEGMENTS];
static unsigned char route_closed[LEVEL_METADATA_MAX_SEGMENTS];
static int route_heap[LEVEL_METADATA_MAX_SEGMENTS + 1];
static int route_heap_pos[LEVEL_METADATA_MAX_SEGMENTS];
static int route_heap_uses_progress_cost;
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
		case LEVEL_METADATA_ROUTE_HOSTAGE:
			return "hostage";
		case LEVEL_METADATA_ROUTE_UNEXPLORED:
			return "unexplored";
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

static void weighted_point(int dest[3], const int a[3], int a_weight, const int b[3], int b_weight)
{
	int total = a_weight + b_weight;

	dest[0] = (int) (((long long) a[0] * a_weight + (long long) b[0] * b_weight) / total);
	dest[1] = (int) (((long long) a[1] * a_weight + (long long) b[1] * b_weight) / total);
	dest[2] = (int) (((long long) a[2] * a_weight + (long long) b[2] * b_weight) / total);
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

static int metadata_wall_flags(const level_metadata_scan_view *view, int wall_num)
{
	if (!valid_wall(view, wall_num) || !view->wall_flags)
		return 0;
	return view->wall_flags(view->user, wall_num);
}

static int metadata_wall_type(const level_metadata_scan_view *view, int wall_num)
{
	if (!valid_wall(view, wall_num) || !view->wall_type)
		return -1;
	return view->wall_type(view->user, wall_num);
}

static int metadata_wall_clip_flags(const level_metadata_scan_view *view, int wall_num)
{
	if (!valid_wall(view, wall_num) || !view->wall_clip_flags)
		return 0;
	return view->wall_clip_flags(view->user, wall_num);
}

static int metadata_wall_is_opened_door(const level_metadata_scan_view *view, int wall_num)
{
	return view->wall_flag_door_opened &&
	       (metadata_wall_flags(view, wall_num) & view->wall_flag_door_opened) != 0;
}

static int metadata_wall_is_hidden_door(const level_metadata_scan_view *view, int wall_num)
{
	if (!valid_wall(view, wall_num) ||
	    !view->wall_type ||
	    !view->wall_keys ||
	    !view->wall_clip_flags ||
	    metadata_wall_type(view, wall_num) != view->wall_type_door)
		return 0;
	if ((metadata_wall_clip_flags(view, wall_num) & view->wall_clip_hidden) == 0)
		return 0;
	if ((metadata_wall_flags(view, wall_num) & view->wall_flag_door_locked) != 0)
		return 0;
	return view->wall_keys(view->user, wall_num) == view->wall_key_none;
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

static int side_has_route_exit(const level_metadata_scan_view *view, int seg, int side)
{
	int wall_num;
	int trigger_num;

	if (view->segment_child(view->user, seg, side) == -2)
		return 1;
	if (!view->side_has_exit_trigger ||
	    !view->side_has_exit_trigger(view->user, seg, side))
		return 0;
	if (!view->wall_num || !view->wall_trigger || !view->trigger_type)
		return 1;
	wall_num = view->wall_num(view->user, seg, side);
	if (!valid_wall(view, wall_num))
		return 1;
	trigger_num = view->wall_trigger(view->user, wall_num);
	return view->trigger_type_secret_exit == 0 ||
	       view->trigger_type(view->user, trigger_num) != view->trigger_type_secret_exit;
}

static int side_center(const level_metadata_scan_view *view, int seg, int side, int xyz[3])
{
	int corners[4][3];
	int i;

	if (side < 0 || side >= LEVEL_METADATA_MAX_SIDES || !xyz)
		return 0;
	if (view->side_center && view->side_center(view->user, seg, side, xyz))
		return 1;
	if (!view->segment_vertex)
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

static int metadata_route_block_progress_weight(const metadata_route_block *block)
{
	if (!block || block->kind == METADATA_ROUTE_BLOCK_NONE)
		return 0;
	return 1;
}

static int route_cost_less(int left_seg, int right_seg)
{
	if (route_heap_uses_progress_cost &&
	    route_progress_weight[left_seg] != route_progress_weight[right_seg])
		return route_progress_weight[left_seg] < route_progress_weight[right_seg];
	return route_distance[left_seg] < route_distance[right_seg];
}

static void heap_sift_up(int index)
{
	while (index > 1) {
		int parent = index / 2;
		if (!route_cost_less(route_heap[index], route_heap[parent]))
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
		if (left <= size && route_cost_less(route_heap[left], route_heap[smallest]))
			smallest = left;
		if (right <= size && route_cost_less(route_heap[right], route_heap[smallest]))
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

static double edge_distance(const level_metadata_scan_view *view, int seg, int child)
{
	if (!valid_segment(view, seg) || !valid_segment(view, child) ||
	    !segment_center_valid[seg] || !segment_center_valid[child])
		return DBL_MAX;
	return point_distance(segment_centers[seg], segment_centers[child]);
}

static int metadata_target_visible_from_pos(
    const level_metadata_scan_view *view,
    int seg,
    const int pos[3],
    const metadata_target *target,
    int wall_num)
{
	if (valid_wall(view, wall_num) && view->wall_visible_from_segment)
		return view->wall_visible_from_segment(view->user, seg, pos, wall_num);
	return view->target_visible_from_segment &&
	       view->target_visible_from_segment(view->user, seg, pos, target->seg, target->pos);
}

static int segment_target_visible(
    const level_metadata_scan_view *view,
    int seg,
    const int *preferred_pos,
    const metadata_target *target,
    int wall_num,
    int visible_pos[3],
    double *extra_distance)
{
	static const int sample_weights[] = { 3, 7, 15 };
	int candidate[3];
	int sample_index;
	int side;
	int vertex_index;

	if ((!view->target_visible_from_segment && !view->wall_visible_from_segment) ||
	    !valid_segment(view, seg) || !target || !visible_pos)
		return 0;
	if (preferred_pos &&
	    metadata_target_visible_from_pos(view, seg, preferred_pos, target, wall_num)) {
		copy_pos(visible_pos, preferred_pos);
		if (extra_distance)
			*extra_distance = 0.0;
		return 1;
	}
	if (!segment_center_valid[seg])
		return 0;
	if (metadata_target_visible_from_pos(view, seg, segment_centers[seg], target, wall_num)) {
		copy_pos(visible_pos, segment_centers[seg]);
		if (extra_distance)
			*extra_distance = 0.0;
		return 1;
	}
	for (side = 0; side < LEVEL_METADATA_MAX_SIDES; ++side) {
		int face_center[3];

		if (!side_center(view, seg, side, face_center))
			continue;
		for (sample_index = 0; sample_index < (int) (sizeof(sample_weights) / sizeof(sample_weights[0])); ++sample_index) {
			weighted_point(candidate, segment_centers[seg], 1, face_center, sample_weights[sample_index]);
			if (!metadata_target_visible_from_pos(view, seg, candidate, target, wall_num))
				continue;
			copy_pos(visible_pos, candidate);
			if (extra_distance)
				*extra_distance = point_distance(segment_centers[seg], candidate);
			return 1;
		}
	}
	for (vertex_index = 0; vertex_index < 8; ++vertex_index) {
		int vertex[3];

		if (!view->segment_vertex || !view->segment_vertex(view->user, seg, vertex_index, vertex))
			continue;
		for (sample_index = 0; sample_index < (int) (sizeof(sample_weights) / sizeof(sample_weights[0])); ++sample_index) {
			weighted_point(candidate, segment_centers[seg], 1, vertex, sample_weights[sample_index]);
			if (!metadata_target_visible_from_pos(view, seg, candidate, target, wall_num))
				continue;
			copy_pos(visible_pos, candidate);
			if (extra_distance)
				*extra_distance = point_distance(segment_centers[seg], candidate);
			return 1;
		}
	}
	for (side = 0; side < LEVEL_METADATA_MAX_SIDES; ++side) {
		int edge;
		for (edge = 0; edge < 4; ++edge) {
			int edge_midpoint[3];
			int first[3];
			int second[3];

			if (!view->segment_vertex ||
			    !view->segment_vertex(view->user, seg, side_to_verts[side][edge], first) ||
			    !view->segment_vertex(view->user, seg, side_to_verts[side][(edge + 1) % 4], second))
				continue;
			edge_midpoint[0] = (first[0] + second[0]) / 2;
			edge_midpoint[1] = (first[1] + second[1]) / 2;
			edge_midpoint[2] = (first[2] + second[2]) / 2;
			for (sample_index = 0; sample_index < (int) (sizeof(sample_weights) / sizeof(sample_weights[0])); ++sample_index) {
				weighted_point(candidate, segment_centers[seg], 1, edge_midpoint, sample_weights[sample_index]);
				if (!metadata_target_visible_from_pos(view, seg, candidate, target, wall_num))
					continue;
				copy_pos(visible_pos, candidate);
				if (extra_distance)
					*extra_distance = point_distance(segment_centers[seg], candidate);
				return 1;
			}
		}
	}
	return 0;
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

static void collect_route_key_targets(const level_metadata_scan_view *view)
{
	int obj_count;
	int objnum;

	memset(key_target_count, 0, sizeof(key_target_count));
	if (!view->object_count || !view->object_segment || !view->object_type || !view->object_position)
		return;
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
		if (type == view->obj_type_powerup && view->object_id) {
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

static int collect_route_targets(
    const level_metadata_scan_view *view,
    metadata_target *reactor,
    metadata_target *exits,
    int *exit_count)
{
	int obj_count;
	int objnum;
	int seg;
	int side;
	int found_reactor = 0;

	*exit_count = 0;
	if (reactor)
		memset(reactor, 0, sizeof(*reactor));
	if (view->object_count && view->object_segment && view->object_type && view->object_position) {
		obj_count = view->object_count(view->user);
		for (objnum = 0; objnum < obj_count; ++objnum) {
			int pos[3];
			int obj_seg;
			if (view->object_flags &&
			    (view->object_flags(view->user, objnum) & view->obj_flag_should_be_dead))
				continue;
			obj_seg = view->object_segment(view->user, objnum);
			if (!valid_segment(view, obj_seg) || !view->object_position(view->user, objnum, pos))
				continue;
			if (view->object_type(view->user, objnum) == view->obj_type_control_center && reactor && !found_reactor) {
				reactor->seg = obj_seg;
				copy_pos(reactor->pos, pos);
				found_reactor = 1;
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
			if (!side_has_route_exit(view, seg, side))
				continue;
			if (!side_center(view, seg, side, pos) && segment_center_valid[seg])
				copy_pos(pos, segment_centers[seg]);
			append_target(view, exits, exit_count, LEVEL_METADATA_MAX_TARGETS, seg, pos);
		}
	}
	return found_reactor;
}

static const char *key_name(int key_index)
{
	return key_index == 0 ? "blue" : key_index == 1 ? "red"
	                             : key_index == 2   ? "gold"
	                                                : "unknown";
}

static void metadata_route_clear_block(metadata_route_block *block)
{
	if (!block)
		return;
	memset(block, 0, sizeof(*block));
	block->kind = METADATA_ROUTE_BLOCK_NONE;
	block->key_index = -1;
	block->seg = -1;
	block->side = -1;
	block->wall_num = -1;
	block->source_wall = -1;
	block->source_seg = -1;
	block->source_side = -1;
	block->trigger_num = -1;
	block->trigger_type = -1;
}

static void metadata_route_clear_path(metadata_route_path *path)
{
	if (!path)
		return;
	path->distance = DBL_MAX;
	path->progress_weight = INT_MAX;
	metadata_route_clear_block(&path->first_block);
	path->terminal_seg = -1;
	path->terminal_pos_valid = 0;
}

static int metadata_route_trigger_valid(int trigger_num)
{
	return trigger_num >= 0 && trigger_num < (int) sizeof(((metadata_route_context *) 0)->fired_triggers);
}

static int metadata_route_trigger_is_progress(const level_metadata_scan_view *view, int trigger_type)
{
	return trigger_type == view->trigger_type_open_door ||
	       trigger_type == view->trigger_type_open_wall ||
	       trigger_type == view->trigger_type_illusory_wall ||
	       trigger_type == view->trigger_type_illusion_off ||
	       trigger_type == view->trigger_type_unlock_door;
}

static const char *metadata_route_trigger_type_name(const level_metadata_scan_view *view, int trigger_type)
{
	if (trigger_type == view->trigger_type_open_door)
		return "open_door";
	if (trigger_type == view->trigger_type_exit)
		return "exit";
	if (trigger_type == view->trigger_type_secret_exit)
		return "secret_exit";
	if (trigger_type == view->trigger_type_illusion_off)
		return "illusion_off";
	if (trigger_type == view->trigger_type_unlock_door)
		return "unlock_door";
	if (trigger_type == view->trigger_type_open_wall)
		return "open_wall";
	if (trigger_type == view->trigger_type_illusory_wall)
		return "illusory_wall";
	return "unknown";
}

static int metadata_route_source_wall_block(
    const level_metadata_scan_view *view,
    const metadata_route_context *route,
    int seg,
    int side,
    int include_fired,
    int source_wall,
    metadata_route_block *block)
{
	int trigger_num;
	int trigger_type;

	if (!view->wall_trigger ||
	    !view->wall_segment ||
	    !view->wall_side ||
	    !view->trigger_type ||
	    !valid_wall(view, source_wall))
		return 0;
	trigger_num = view->wall_trigger(view->user, source_wall);
	if (!metadata_route_trigger_valid(trigger_num))
		return 0;
	if (view->trigger_flag_disabled &&
	    view->trigger_flags &&
	    (view->trigger_flags(view->user, trigger_num) & view->trigger_flag_disabled))
		return 0;
	trigger_type = view->trigger_type(view->user, trigger_num);
	if (!metadata_route_trigger_is_progress(view, trigger_type))
		return 0;
	if (!include_fired && route->fired_triggers[trigger_num])
		return 0;
	if (block) {
		metadata_route_clear_block(block);
		block->kind = METADATA_ROUTE_BLOCK_TRIGGER;
		block->seg = seg;
		block->side = side;
		block->wall_num = view->wall_num ? view->wall_num(view->user, seg, side) : -1;
		block->source_wall = source_wall;
		block->source_seg = view->wall_segment(view->user, source_wall);
		block->source_side = view->wall_side(view->user, source_wall);
		block->trigger_num = trigger_num;
		block->trigger_type = trigger_type;
	}
	return 1;
}

static int metadata_route_side_trigger_source(
    const level_metadata_scan_view *view,
    const metadata_route_context *route,
    int seg,
    int side,
    int include_fired,
    metadata_route_block *block)
{
	int count;
	int index;

	if (!view->triggered_side_opener_count ||
	    !view->triggered_side_opener_wall_num ||
	    !view->wall_trigger ||
	    !view->wall_segment ||
	    !view->wall_side ||
	    !view->trigger_type ||
	    !valid_segment(view, seg) ||
	    side < 0 ||
	    side >= LEVEL_METADATA_MAX_SIDES)
		return 0;
	count = view->triggered_side_opener_count(view->user, seg, side);
	for (index = 0; index < count; ++index) {
		int source_wall = view->triggered_side_opener_wall_num(view->user, seg, side, index);
		if (metadata_route_source_wall_block(view, route, seg, side, include_fired, source_wall, block))
			return 1;
	}
	return 0;
}

static int metadata_route_edge_has_fired_trigger(
    const level_metadata_scan_view *view,
    const metadata_route_context *route,
    int seg,
    int side)
{
	metadata_route_block block;
	int count;
	int index;

	if (!route ||
	    !view->triggered_side_opener_count ||
	    !view->triggered_side_opener_wall_num ||
	    !valid_segment(view, seg) ||
	    side < 0 ||
	    side >= LEVEL_METADATA_MAX_SIDES)
		return 0;
	count = view->triggered_side_opener_count(view->user, seg, side);
	for (index = 0; index < count; ++index) {
		int source_wall = view->triggered_side_opener_wall_num(view->user, seg, side, index);
		if (metadata_route_source_wall_block(view, route, seg, side, 1, source_wall, &block) &&
		    metadata_route_trigger_valid(block.trigger_num) &&
		    route->fired_triggers[block.trigger_num])
			return 1;
	}
	return 0;
}

static int metadata_route_edge_pair_has_fired_trigger(
    const level_metadata_scan_view *view,
    const metadata_route_context *route,
    int seg,
    int side,
    int child)
{
	int reverse_side;

	if (metadata_route_edge_has_fired_trigger(view, route, seg, side))
		return 1;
	reverse_side = view->reverse_side ? view->reverse_side(view->user, seg, child) : -1;
	return reverse_side >= 0 &&
	       reverse_side < LEVEL_METADATA_MAX_SIDES &&
	       metadata_route_edge_has_fired_trigger(view, route, child, reverse_side);
}

static int metadata_route_edge_trigger_blocker(
    const level_metadata_scan_view *view,
    const metadata_route_context *route,
    int seg,
    int side,
    int child,
    metadata_route_block *block)
{
	int reverse_side;

	if (metadata_route_side_trigger_source(view, route, seg, side, 0, block))
		return 1;
	reverse_side = view->reverse_side ? view->reverse_side(view->user, seg, child) : -1;
	return reverse_side >= 0 &&
	       reverse_side < LEVEL_METADATA_MAX_SIDES &&
	       metadata_route_side_trigger_source(view, route, child, reverse_side, 0, block);
}

static int metadata_route_side_is_flyable(
    const level_metadata_scan_view *view,
    int seg,
    int side)
{
	if (!view->side_is_flyable)
		return 0;
	return view->side_is_flyable(view->user, seg, side) != 0;
}

static int metadata_route_edge_pair_is_control_center_link(
    const level_metadata_scan_view *view,
    int seg,
    int side,
    int child)
{
	int reverse_side;

	if (!view->side_is_control_center_link)
		return 0;
	if (view->side_is_control_center_link(view->user, seg, side))
		return 1;
	reverse_side = view->reverse_side ? view->reverse_side(view->user, seg, child) : -1;
	return reverse_side >= 0 &&
	       reverse_side < LEVEL_METADATA_MAX_SIDES &&
	       view->side_is_control_center_link(view->user, child, reverse_side);
}

static int metadata_route_edge_passable(
    const level_metadata_scan_view *view,
    const metadata_route_context *route,
    int seg,
    int side,
    int optimistic,
    int forbidden_missing_key,
    metadata_route_block *block)
{
	int child = view->segment_child(view->user, seg, side);
	int wall_num;
	int wall_type;
	int wall_keys;
	metadata_route_block trigger_block;

	if (block)
		metadata_route_clear_block(block);
	if (!edge_has_valid_reverse(view, seg, side, child))
		return 0;
	if (side_has_exit(view, seg, side))
		return 0;
	if (metadata_route_side_is_flyable(view, seg, side))
		return 1;
	if (route->control_center_destroyed &&
	    metadata_route_edge_pair_is_control_center_link(view, seg, side, child))
		return 1;
	if (metadata_route_edge_pair_has_fired_trigger(view, route, seg, side, child))
		return 1;
	if (!view->wall_num || !view->wall_type || !view->wall_keys)
		return 1;
	wall_num = view->wall_num(view->user, seg, side);
	if (!valid_wall(view, wall_num))
		return 1;
	wall_type = view->wall_type(view->user, wall_num);
	wall_keys = view->wall_keys(view->user, wall_num);
	if (metadata_wall_is_opened_door(view, wall_num))
		return 1;
	if (metadata_wall_is_hidden_door(view, wall_num)) {
		if (route->opened_hidden_walls[wall_num])
			return 1;
		if (block) {
			metadata_route_clear_block(block);
			block->kind = METADATA_ROUTE_BLOCK_HIDDEN_DOOR;
			block->seg = seg;
			block->side = side;
			block->wall_num = wall_num;
		}
		return optimistic;
	}
	if (view->side_is_hard_blocked &&
	    view->side_is_hard_blocked(view->user, seg, side)) {
		if (metadata_route_edge_trigger_blocker(view, route, seg, side, child, &trigger_block)) {
			if (metadata_route_trigger_valid(trigger_block.trigger_num) &&
			    route->avoided_triggers[trigger_block.trigger_num])
				return 0;
			if (block)
				*block = trigger_block;
			return optimistic;
		}
		return 0;
	}
	if (wall_type == view->wall_type_open ||
	    wall_type == view->wall_type_blastable ||
	    wall_type == view->wall_type_illusion)
		return 1;
	if (wall_type == view->wall_type_door &&
	    wall_key_allowed(view, wall_keys, route->key_mask) &&
	    (!view->wall_flag_door_locked ||
	     (metadata_wall_flags(view, wall_num) & view->wall_flag_door_locked) == 0))
		return 1;
	if (metadata_route_edge_trigger_blocker(view, route, seg, side, child, &trigger_block)) {
		if (metadata_route_trigger_valid(trigger_block.trigger_num) &&
		    route->avoided_triggers[trigger_block.trigger_num])
			return 0;
		if (block)
			*block = trigger_block;
		return optimistic;
	}
	if (wall_type == view->wall_type_door &&
	    view->wall_flag_door_locked &&
	    (metadata_wall_flags(view, wall_num) & view->wall_flag_door_locked) != 0)
		return 0;
	if (wall_type == view->wall_type_door && wall_key_allowed(view, wall_keys, route->key_mask))
		return 1;
	if (wall_type == view->wall_type_door && !wall_key_allowed(view, wall_keys, route->key_mask)) {
		int key_index = key_index_for_wall_key(view, wall_keys);
		if (key_index >= 0 &&
		    ((route->avoided_key_mask | route->key_in_progress) & key_bit_for_index(key_index)) != 0)
			return 0;
		if (key_index >= 0 && block) {
			metadata_route_clear_block(block);
			block->kind = METADATA_ROUTE_BLOCK_KEY;
			block->key_index = key_index;
			block->seg = seg;
			block->side = side;
			block->wall_num = wall_num;
		}
		return optimistic && key_index >= 0 && key_index != forbidden_missing_key;
	}
	return 0;
}

int level_metadata_scan_route_edge_cost(
    const level_metadata_scan_view *view,
    int seg,
    int side)
{
	metadata_route_context route;

	if (!view ||
	    !view->segment_child ||
	    !view->reverse_side ||
	    !valid_segment(view, seg) ||
	    side < 0 ||
	    side >= LEVEL_METADATA_MAX_SIDES)
		return LEVEL_METADATA_ROUTE_EDGE_BLOCKED;
	memset(&route, 0, sizeof(route));
	route.key_mask = view->initial_key_mask &
	                 (LEVEL_METADATA_KEY_MASK_BLUE |
	                  LEVEL_METADATA_KEY_MASK_RED |
	                  LEVEL_METADATA_KEY_MASK_GOLD);
	route.control_center_destroyed = view->initial_control_center_destroyed != 0;
	if (metadata_route_edge_passable(view, &route, seg, side, 0, -1, NULL))
		return LEVEL_METADATA_ROUTE_EDGE_PASSABLE;
	if (metadata_route_edge_passable(view, &route, seg, side, 1, -1, NULL))
		return LEVEL_METADATA_ROUTE_EDGE_PROGRESS;
	return LEVEL_METADATA_ROUTE_EDGE_BLOCKED;
}

static int metadata_route_compute_paths(
    const level_metadata_scan_view *view,
    const metadata_route_context *route,
    int goal_seg,
    int optimistic,
    int forbidden_missing_key)
{
	int heap_size = 0;
	int seg;

	if (!valid_segment(view, route->current_seg) ||
	    !segment_center_valid[route->current_seg] ||
	    (goal_seg >= 0 && (!valid_segment(view, goal_seg) || !segment_center_valid[goal_seg])))
		return 0;
	route_heap_uses_progress_cost = forbidden_missing_key < 0;
	for (seg = 0; seg < view->num_segments; ++seg) {
		route_distance[seg] = DBL_MAX;
		route_progress_weight[seg] = INT_MAX;
		route_parent_seg[seg] = -1;
		route_parent_side[seg] = -1;
		route_closed[seg] = 0;
		route_heap_pos[seg] = 0;
	}
	route_distance[route->current_seg] = point_distance(route->current_pos, segment_centers[route->current_seg]);
	route_progress_weight[route->current_seg] = 0;
	heap_push(&heap_size, route->current_seg);
	while (heap_size > 0) {
		int cur = heap_pop(&heap_size);
		int side;
		if (goal_seg >= 0 && cur == goal_seg)
			break;
		route_closed[cur] = 1;
		for (side = 0; side < LEVEL_METADATA_MAX_SIDES; ++side) {
			metadata_route_block block;
			int child = view->segment_child(view->user, cur, side);
			int next_progress_weight;
			double step;
			double next_distance;
			if (!valid_segment(view, child) || route_closed[child])
				continue;
			if (!metadata_route_edge_passable(view, route, cur, side, optimistic, forbidden_missing_key, &block))
				continue;
			step = edge_distance(view, cur, child);
			if (step == DBL_MAX)
				continue;
			next_distance = route_distance[cur] + step;
			next_progress_weight = route_progress_weight[cur] + metadata_route_block_progress_weight(&block);
			if (route_heap_uses_progress_cost &&
			    next_progress_weight > route_progress_weight[child])
				continue;
			if ((!route_heap_uses_progress_cost ||
			     next_progress_weight == route_progress_weight[child]) &&
			    next_distance >= route_distance[child])
				continue;
			route_distance[child] = next_distance;
			route_progress_weight[child] = next_progress_weight;
			route_parent_seg[child] = cur;
			route_parent_side[child] = side;
			if (route_heap_pos[child])
				heap_decrease(child);
			else
				heap_push(&heap_size, child);
		}
	}
	return goal_seg < 0 || route_distance[goal_seg] != DBL_MAX;
}

static int metadata_route_find_path_forbidden_key(
    const level_metadata_scan_view *view,
    const metadata_route_context *route,
    int goal_seg,
    const int goal_pos[3],
    int optimistic,
    int forbidden_missing_key,
    metadata_route_path *path)
{
	metadata_route_clear_path(path);
	if (!metadata_route_compute_paths(view, route, goal_seg, optimistic, forbidden_missing_key))
		return 0;
	if (path) {
		int reversed[LEVEL_METADATA_MAX_SEGMENTS];
		int count = 0;
		int cur = goal_seg;
		int i;
		path->distance = route_distance[goal_seg] + (goal_pos ? point_distance(segment_centers[goal_seg], goal_pos) : 0.0);
		path->progress_weight = route_progress_weight[goal_seg];
		while (valid_segment(view, cur) && count < LEVEL_METADATA_MAX_SEGMENTS) {
			reversed[count++] = cur;
			if (cur == route->current_seg)
				break;
			cur = route_parent_seg[cur];
		}
		for (i = count - 1; i > 0; --i) {
			int from = reversed[i];
			int to = reversed[i - 1];
			int side = route_parent_side[to];
			metadata_route_block block;
			if (!metadata_route_edge_passable(view, route, from, side, 1, forbidden_missing_key, &block))
				continue;
			if (block.kind != METADATA_ROUTE_BLOCK_NONE) {
				path->first_block = block;
				break;
			}
		}
		path->terminal_seg = goal_seg;
		if (goal_pos)
			copy_pos(path->terminal_pos, goal_pos);
		else
			copy_pos(path->terminal_pos, segment_centers[goal_seg]);
		path->terminal_pos_valid = 1;
	}
	return 1;
}

int level_metadata_scan_route_search_shadow(
    const level_metadata_scan_view *view,
    int optimistic,
    level_metadata_route_search_node *nodes,
    int capacity)
{
	level_metadata_route_progress_shadow progress;

	if (!view || !nodes || capacity < view->num_segments ||
	    !valid_segment(view, view->start_segment) || !view->start_position)
		return 0;
	memset(&progress, 0, sizeof(progress));
	progress.current_seg = view->start_segment;
	progress.key_mask = view->initial_key_mask &
	                    (LEVEL_METADATA_KEY_MASK_BLUE |
	                     LEVEL_METADATA_KEY_MASK_RED |
	                     LEVEL_METADATA_KEY_MASK_GOLD);
	progress.control_center_destroyed =
	    view->initial_control_center_destroyed != 0;
	if (!view->start_position(view->user, progress.current_pos))
		return 0;
	return level_metadata_scan_route_search_state_shadow(
	    view, &progress, optimistic, nodes, capacity);
}

static void metadata_route_context_from_progress_shadow(
    metadata_route_context *route,
    const level_metadata_route_progress_shadow *progress)
{
	memset(route, 0, sizeof(*route));
	route->current_seg = progress->current_seg;
	copy_pos(route->current_pos, progress->current_pos);
	route->key_mask = progress->key_mask;
	route->key_in_progress = progress->key_in_progress;
	route->avoided_key_mask = progress->avoided_key_mask;
	route->control_center_destroyed = progress->control_center_destroyed != 0;
	memcpy(route->fired_triggers, progress->fired_triggers,
	       sizeof(progress->fired_triggers));
	memcpy(route->trigger_in_progress, progress->trigger_in_progress,
	       sizeof(progress->trigger_in_progress));
	memcpy(route->avoided_triggers, progress->avoided_triggers,
	       sizeof(progress->avoided_triggers));
	memcpy(route->opened_hidden_walls, progress->opened_hidden_walls,
	       sizeof(progress->opened_hidden_walls));
}

static void metadata_route_context_to_progress_shadow(
    const metadata_route_context *route,
    level_metadata_route_progress_shadow *progress)
{
	memset(progress, 0, sizeof(*progress));
	progress->current_seg = route->current_seg;
	copy_pos(progress->current_pos, route->current_pos);
	progress->key_mask = route->key_mask;
	progress->key_in_progress = route->key_in_progress;
	progress->avoided_key_mask = route->avoided_key_mask;
	progress->control_center_destroyed = route->control_center_destroyed;
	memcpy(progress->fired_triggers, route->fired_triggers,
	       sizeof(progress->fired_triggers));
	memcpy(progress->trigger_in_progress, route->trigger_in_progress,
	       sizeof(progress->trigger_in_progress));
	memcpy(progress->avoided_triggers, route->avoided_triggers,
	       sizeof(progress->avoided_triggers));
	memcpy(progress->opened_hidden_walls, route->opened_hidden_walls,
	       sizeof(progress->opened_hidden_walls));
}

int level_metadata_scan_route_search_state_shadow(
    const level_metadata_scan_view *view,
    const level_metadata_route_progress_shadow *progress,
    int optimistic,
    level_metadata_route_search_node *nodes,
    int capacity)
{
	metadata_route_context route;
	int seg;

	if (!view || !progress || !nodes || capacity < view->num_segments ||
	    !valid_segment(view, progress->current_seg))
		return 0;
	collect_segment_centers(view);
	metadata_route_context_from_progress_shadow(&route, progress);
	if (!metadata_route_compute_paths(view, &route, -1, optimistic != 0, -1))
		return 0;
	for (seg = 0; seg < view->num_segments; ++seg) {
		nodes[seg].reachable = route_distance[seg] != DBL_MAX;
		nodes[seg].distance = route_distance[seg];
		nodes[seg].progress_weight = route_progress_weight[seg];
		nodes[seg].parent_seg = route_parent_seg[seg];
		nodes[seg].parent_side = route_parent_side[seg];
	}
	return view->num_segments;
}

static int metadata_route_find_path(
    const level_metadata_scan_view *view,
    const metadata_route_context *route,
    int goal_seg,
    const int goal_pos[3],
    int optimistic,
    metadata_route_path *path)
{
	return metadata_route_find_path_forbidden_key(view, route, goal_seg, goal_pos, optimistic, -1, path);
}

static int metadata_route_find_visible_path(
    const level_metadata_scan_view *view,
    const metadata_route_context *route,
    const metadata_target *target,
    int wall_num,
    metadata_route_path *path)
{
	int heap_size = 0;
	int seg;

	metadata_route_clear_path(path);
	if ((!view->target_visible_from_segment && !view->wall_visible_from_segment) ||
	    !route ||
	    !valid_segment(view, route->current_seg) ||
	    !target ||
	    !valid_segment(view, target->seg) ||
	    !segment_center_valid[route->current_seg])
		return 0;
	for (seg = 0; seg < view->num_segments; ++seg) {
		route_distance[seg] = DBL_MAX;
		route_progress_weight[seg] = INT_MAX;
		route_parent_seg[seg] = -1;
		route_parent_side[seg] = -1;
		route_closed[seg] = 0;
		route_heap_pos[seg] = 0;
	}
	route_distance[route->current_seg] = point_distance(route->current_pos, segment_centers[route->current_seg]);
	route_progress_weight[route->current_seg] = 0;
	heap_push(&heap_size, route->current_seg);
	while (heap_size > 0) {
		int cur = heap_pop(&heap_size);
		int side;
		const int *preferred_pos = cur == route->current_seg ? route->current_pos : NULL;
		int visible_pos[3];
		double visible_extra_distance = 0.0;

		if (segment_target_visible(view, cur, preferred_pos, target, wall_num, visible_pos, &visible_extra_distance)) {
			if (path) {
				path->distance = route_distance[cur] + visible_extra_distance;
				path->progress_weight = 0;
				path->terminal_seg = cur;
				copy_pos(path->terminal_pos, visible_pos);
				path->terminal_pos_valid = 1;
			}
			return 1;
		}
		route_closed[cur] = 1;
		for (side = 0; side < LEVEL_METADATA_MAX_SIDES; ++side) {
			int child = view->segment_child(view->user, cur, side);
			double step;
			double next_distance;
			if (!valid_segment(view, child) || route_closed[child])
				continue;
			if (!metadata_route_edge_passable(view, route, cur, side, 0, -1, NULL))
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
	return 0;
}

static int metadata_route_trigger_source_pos(
    const level_metadata_scan_view *view,
    const metadata_route_block *block,
    int pos[3])
{
	if (!block || !valid_segment(view, block->source_seg))
		return 0;
	if (side_center(view, block->source_seg, block->source_side, pos))
		return 1;
	if (segment_center_valid[block->source_seg]) {
		copy_pos(pos, segment_centers[block->source_seg]);
		return 1;
	}
	return 0;
}

int level_metadata_scan_route_trigger_sources_shadow(
    const level_metadata_scan_view *view,
    const level_metadata_route_progress_shadow *progress,
    int seg,
    int side,
    level_metadata_route_trigger_source_shadow *sources,
    int capacity,
    int *count)
{
	metadata_route_context route;
	metadata_route_block first;
	int child;
	int opener_count;
	int index;

	if (!view || !progress || !count || capacity < 0 ||
	    (capacity > 0 && !sources))
		return 0;
	*count = 0;
	if (!view->segment_child || !valid_segment(view, seg) ||
	    side < 0 || side >= LEVEL_METADATA_MAX_SIDES)
		return 1;
	child = view->segment_child(view->user, seg, side);
	if (!valid_segment(view, child))
		return 1;
	collect_segment_centers(view);
	metadata_route_context_from_progress_shadow(&route, progress);
	if (!metadata_route_edge_trigger_blocker(
	        view, &route, seg, side, child, &first))
		return 1;
	opener_count = view->triggered_side_opener_count(
	    view->user, first.seg, first.side);
	for (index = 0; index < opener_count; ++index) {
		metadata_route_block candidate;
		level_metadata_route_trigger_source_shadow *source;
		int source_wall = view->triggered_side_opener_wall_num(
		    view->user, first.seg, first.side, index);
		int pos[3];
		if (!metadata_route_source_wall_block(
		        view, &route, first.seg, first.side, 0, source_wall,
		        &candidate) ||
		    route.trigger_in_progress[candidate.trigger_num] ||
		    !metadata_route_trigger_source_pos(view, &candidate, pos))
			continue;
		if (*count >= capacity)
			return 0;
		source = &sources[(*count)++];
		source->target_seg = candidate.seg;
		source->target_side = candidate.side;
		source->target_wall = candidate.wall_num;
		source->source_wall = candidate.source_wall;
		source->source_seg = candidate.source_seg;
		source->source_side = candidate.source_side;
		source->trigger_num = candidate.trigger_num;
		source->trigger_type = candidate.trigger_type;
		copy_pos(source->source_pos, pos);
	}
	return 1;
}

static int metadata_route_try_trigger_firing_path(
    const level_metadata_scan_view *view,
    const metadata_route_context *route,
    const metadata_route_block *candidate,
    metadata_route_path *path)
{
	metadata_target target;
	int pos[3];

	if (!candidate ||
	    !metadata_route_trigger_valid(candidate->trigger_num) ||
	    route->fired_triggers[candidate->trigger_num] ||
	    route->trigger_in_progress[candidate->trigger_num] ||
	    !metadata_route_trigger_source_pos(view, candidate, pos))
		return 0;
	target.seg = candidate->source_seg;
	copy_pos(target.pos, pos);
	target.visited = 0;
	if (metadata_route_find_path(view, route, candidate->source_seg, pos, 0, path))
		return 1;
	return metadata_route_find_visible_path(view, route, &target, candidate->source_wall, path);
}

static int metadata_route_consider_trigger_firing_path(
    const level_metadata_scan_view *view,
    const metadata_route_context *route,
    const metadata_route_block *candidate,
    metadata_route_block *best_block,
    metadata_route_path *best_path)
{
	metadata_route_path candidate_path;

	if (!metadata_route_try_trigger_firing_path(view, route, candidate, &candidate_path))
		return 0;
	if (best_path->terminal_seg >= 0 && candidate_path.distance >= best_path->distance)
		return 0;
	*best_block = *candidate;
	*best_path = candidate_path;
	return 1;
}

static int metadata_route_find_trigger_firing_path(
    const level_metadata_scan_view *view,
    const metadata_route_context *route,
    const metadata_route_block *block,
    metadata_route_block *best_block,
    metadata_route_path *best_path)
{
	int count;
	int index;
	int found = 0;

	if (!block || !best_block || !best_path)
		return 0;
	metadata_route_clear_path(best_path);
	found |= metadata_route_consider_trigger_firing_path(view, route, block, best_block, best_path);
	if (!view->triggered_side_opener_count ||
	    !view->triggered_side_opener_wall_num ||
	    !valid_segment(view, block->seg) ||
	    block->side < 0 ||
	    block->side >= LEVEL_METADATA_MAX_SIDES)
		return found;
	count = view->triggered_side_opener_count(view->user, block->seg, block->side);
	for (index = 0; index < count; ++index) {
		metadata_route_block candidate;
		int source_wall = view->triggered_side_opener_wall_num(view->user, block->seg, block->side, index);
		if (!metadata_route_source_wall_block(view, route, block->seg, block->side, 0, source_wall, &candidate))
			continue;
		if (metadata_route_consider_trigger_firing_path(view, route, &candidate, best_block, best_path))
			found = 1;
	}
	return found;
}

int level_metadata_scan_route_trigger_firing_path_shadow(
    const level_metadata_scan_view *view,
    const level_metadata_route_progress_shadow *progress,
    int seg,
    int side,
    level_metadata_route_trigger_firing_path_shadow *result)
{
	metadata_route_context route;
	metadata_route_block block;
	metadata_route_block selected;
	metadata_route_path path;
	int child;
	int source_pos[3];

	if (!view || !progress || !result)
		return 0;
	memset(result, 0, sizeof(*result));
	result->source.target_seg = -1;
	result->source.target_side = -1;
	result->source.target_wall = -1;
	result->source.source_wall = -1;
	result->source.source_seg = -1;
	result->source.source_side = -1;
	result->source.trigger_num = -1;
	result->source.trigger_type = -1;
	result->terminal_seg = -1;
	if (!view->segment_child || !valid_segment(view, seg) ||
	    side < 0 || side >= LEVEL_METADATA_MAX_SIDES)
		return 1;
	child = view->segment_child(view->user, seg, side);
	if (!valid_segment(view, child))
		return 1;
	collect_segment_centers(view);
	metadata_route_context_from_progress_shadow(&route, progress);
	if (!metadata_route_edge_trigger_blocker(
	        view, &route, seg, side, child, &block) ||
	    !metadata_route_find_trigger_firing_path(
	        view, &route, &block, &selected, &path))
		return 1;
	if (!metadata_route_trigger_source_pos(view, &selected, source_pos))
		return 0;
	result->found = 1;
	result->source.target_seg = selected.seg;
	result->source.target_side = selected.side;
	result->source.target_wall = selected.wall_num;
	result->source.source_wall = selected.source_wall;
	result->source.source_seg = selected.source_seg;
	result->source.source_side = selected.source_side;
	result->source.trigger_num = selected.trigger_num;
	result->source.trigger_type = selected.trigger_type;
	copy_pos(result->source.source_pos, source_pos);
	result->distance = path.distance;
	result->progress_weight = path.progress_weight;
	result->terminal_seg = path.terminal_seg;
	result->terminal_pos_valid = path.terminal_pos_valid;
	if (path.terminal_pos_valid)
		copy_pos(result->terminal_pos, path.terminal_pos);
	return 1;
}

static void metadata_route_set_problem(level_metadata_state *state, const char *problem)
{
	if (!state || state->route_problem[0] || !problem)
		return;
	snprintf(state->route_problem, sizeof(state->route_problem), "%s", problem);
}

static int metadata_route_problem_is_avoidable_trigger_dependency(const char *problem)
{
	return problem &&
	       (!strncmp(problem, "trigger route dependency loop", 29) ||
	        !strcmp(problem, "trigger source missing"));
}

static level_metadata_route_step *metadata_route_append_step(
    const level_metadata_scan_view *view,
    level_metadata_state *state,
    metadata_route_context *route,
    int kind,
    const char *label,
    int seg,
    int side)
{
	level_metadata_route_step *step;
	int i;

	if (state->route_step_count >= LEVEL_METADATA_MAX_ROUTE_STEPS) {
		metadata_route_set_problem(state, "route step limit");
		return NULL;
	}
	step = &state->route_steps[state->route_step_count++];
	memset(step, 0, sizeof(*step));
	step->kind = kind;
	step->seg = seg;
	step->side = side;
	step->wall_num = -1;
	step->trigger_num = -1;
	step->trigger_type = -1;
	step->key_index = -1;
	step->activation_kind = LEVEL_METADATA_ROUTE_ACTIVATION_NONE;
	if (route && valid_segment(view, route->current_seg)) {
		step->activation_pos_valid = 1;
		copy_pos(step->activation_pos, route->current_pos);
	}
	step->distance_from_previous = route ? route->pending_distance : 0.0;
	snprintf(step->label, sizeof(step->label), "%s", label ? label : "");
	for (i = 0; i < LEVEL_METADATA_MAX_ROUTE_LINKS; ++i) {
		step->opened_link_seg[i] = -1;
		step->opened_link_side[i] = -1;
		step->opened_link_wall[i] = -1;
	}
	if (view && view->wall_num && valid_segment(view, seg) && side >= 0 && side < LEVEL_METADATA_MAX_SIDES)
		step->wall_num = view->wall_num(view->user, seg, side);
	if (kind != LEVEL_METADATA_ROUTE_START && valid_segment(view, seg)) {
		if (side_center(view, seg, side, step->label_pos))
			step->label_pos_valid = 1;
		else if (segment_center_valid[seg]) {
			copy_pos(step->label_pos, segment_centers[seg]);
			step->label_pos_valid = 1;
		}
	}
	if (route)
		route->pending_distance = 0.0;
	return step;
}

static void metadata_route_step_set_aim(
    level_metadata_route_step *step, const int pos[3])
{
	if (!step || !pos)
		return;
	step->aim_pos_valid = 1;
	copy_pos(step->aim_pos, pos);
	step->label_pos_valid = 1;
	copy_pos(step->label_pos, pos);
}

static int metadata_route_wall_target_pos(
    const level_metadata_scan_view *view,
    int wall_num,
    int fallback_seg,
    int fallback_side,
    int pos[3])
{
	int seg = fallback_seg;
	int side = fallback_side;

	if (!valid_wall(view, wall_num))
		return 0;
	if (view->wall_segment && view->wall_side) {
		seg = view->wall_segment(view->user, wall_num);
		side = view->wall_side(view->user, wall_num);
	}
	if (!valid_segment(view, seg))
		return 0;
	if (side_center(view, seg, side, pos))
		return 1;
	if (segment_center_valid[seg]) {
		copy_pos(pos, segment_centers[seg]);
		return 1;
	}
	return 0;
}

static int metadata_route_trigger_activation_kind(
    const level_metadata_scan_view *view,
    const metadata_route_block *block)
{
	int wall_type;

	if (!view || !block)
		return LEVEL_METADATA_ROUTE_ACTIVATION_PASS_THROUGH_TRIGGER;
	if (view->wall_is_shootable_trigger &&
	    valid_wall(view, block->source_wall) &&
	    view->wall_is_shootable_trigger(view->user, block->source_wall))
		return LEVEL_METADATA_ROUTE_ACTIVATION_SHOOT_SWITCH;
	wall_type = metadata_wall_type(view, block->source_wall);
	if (wall_type == view->wall_type_open)
		return LEVEL_METADATA_ROUTE_ACTIVATION_FLY_THROUGH_TRIGGER;
	return LEVEL_METADATA_ROUTE_ACTIVATION_PASS_THROUGH_TRIGGER;
}

static const char *metadata_route_trigger_action_name(int activation_kind)
{
	switch (activation_kind) {
		case LEVEL_METADATA_ROUTE_ACTIVATION_SHOOT_SWITCH:
			return "Shoot switch";
		case LEVEL_METADATA_ROUTE_ACTIVATION_FLY_THROUGH_TRIGGER:
			return "Fly-through";
		case LEVEL_METADATA_ROUTE_ACTIVATION_PASS_THROUGH_TRIGGER:
			return "Pass through";
		default:
			return "Trigger";
	}
}

static int metadata_route_step_for_trigger(
    const level_metadata_scan_view *view,
    level_metadata_state *state,
    metadata_route_context *route,
    const metadata_route_block *block,
    int mark_fired)
{
	level_metadata_route_step *step;
	char label[LEVEL_METADATA_ROUTE_LABEL_LEN];
	const char *type_name;
	int link_count;
	int i;
	int activation_kind;

	if (!block || !metadata_route_trigger_valid(block->trigger_num))
		return 0;
	type_name = metadata_route_trigger_type_name(view, block->trigger_type);
	activation_kind = metadata_route_trigger_activation_kind(view, block);
	snprintf(label, sizeof(label), "%s trigger %d",
	         metadata_route_trigger_action_name(activation_kind),
	         block->trigger_num);
	step = metadata_route_append_step(view, state, route, LEVEL_METADATA_ROUTE_TRIGGER, label, block->source_seg, block->source_side);
	if (!step)
		return 0;
	step->wall_num = block->source_wall;
	step->trigger_num = block->trigger_num;
	step->trigger_type = block->trigger_type;
	step->activation_kind = activation_kind;
	{
		int aim_pos[3];
		if (metadata_route_wall_target_pos(
		        view, block->source_wall, block->source_seg,
		        block->source_side, aim_pos))
			metadata_route_step_set_aim(step, aim_pos);
	}
	snprintf(step->trigger_type_name, sizeof(step->trigger_type_name), "%s", type_name);
	if (view->trigger_link_count && view->trigger_link_segment && view->trigger_link_side) {
		link_count = view->trigger_link_count(view->user, block->trigger_num);
		for (i = 0; i < link_count && step->opened_link_count < LEVEL_METADATA_MAX_ROUTE_LINKS; ++i) {
			int link_seg = view->trigger_link_segment(view->user, block->trigger_num, i);
			int link_side = view->trigger_link_side(view->user, block->trigger_num, i);
			int out = step->opened_link_count++;
			step->opened_link_seg[out] = link_seg;
			step->opened_link_side[out] = link_side;
			step->opened_link_wall[out] = view->wall_num && valid_segment(view, link_seg) ? view->wall_num(view->user, link_seg, link_side) : -1;
		}
	}
	if (mark_fired)
		route->fired_triggers[block->trigger_num] = 1;
	return 1;
}

static int metadata_route_move_to_target(
    const level_metadata_scan_view *view,
    level_metadata_state *state,
    metadata_route_context *route,
    int goal_seg,
    const int goal_pos[3],
    int depth);

static int metadata_route_reverse_wall_num(const level_metadata_scan_view *view, int seg, int side)
{
	int child;
	int reverse_side;

	if (!view->segment_child || !view->reverse_side || !view->wall_num)
		return -1;
	child = view->segment_child(view->user, seg, side);
	if (!valid_segment(view, child))
		return -1;
	reverse_side = view->reverse_side(view->user, seg, child);
	if (reverse_side < 0 || reverse_side >= LEVEL_METADATA_MAX_SIDES)
		return -1;
	return view->wall_num(view->user, child, reverse_side);
}

static void metadata_route_set_hidden_door_state(
    const level_metadata_scan_view *view,
    const metadata_route_block *block,
    unsigned char state[LEVEL_METADATA_MAX_WALLS],
    unsigned char value)
{
	int reverse_wall;

	if (!block || !state)
		return;
	if (valid_wall(view, block->wall_num))
		state[block->wall_num] = value;
	reverse_wall = metadata_route_reverse_wall_num(view, block->seg, block->side);
	if (valid_wall(view, reverse_wall))
		state[reverse_wall] = value;
}

static int metadata_route_hidden_door_in_progress(
    const level_metadata_scan_view *view,
    const metadata_route_context *route,
    const metadata_route_block *block)
{
	int reverse_wall;

	if (!route || !block)
		return 0;
	if (valid_wall(view, block->wall_num) && route->hidden_door_in_progress[block->wall_num])
		return 1;
	reverse_wall = metadata_route_reverse_wall_num(view, block->seg, block->side);
	return valid_wall(view, reverse_wall) && route->hidden_door_in_progress[reverse_wall];
}

static void metadata_route_add_hidden_door_link(
    level_metadata_route_step *step,
    int seg,
    int side,
    int wall_num)
{
	int out;

	if (!step ||
	    step->opened_link_count >= LEVEL_METADATA_MAX_ROUTE_LINKS)
		return;
	out = step->opened_link_count++;
	step->opened_link_seg[out] = seg;
	step->opened_link_side[out] = side;
	step->opened_link_wall[out] = wall_num;
}

static int metadata_route_append_hidden_door_step(
    const level_metadata_scan_view *view,
    level_metadata_state *state,
    metadata_route_context *route,
    const metadata_route_block *block)
{
	level_metadata_route_step *step;
	int reverse_wall;
	int child;
	int reverse_side;

	if (!block || block->kind != METADATA_ROUTE_BLOCK_HIDDEN_DOOR || !valid_wall(view, block->wall_num))
		return 0;
	step = metadata_route_append_step(view, state, route, LEVEL_METADATA_ROUTE_HIDDEN_DOOR, "Open hidden door", block->seg, block->side);
	if (!step)
		return 0;
	step->wall_num = block->wall_num;
	step->activation_kind = LEVEL_METADATA_ROUTE_ACTIVATION_OPEN_HIDDEN_DOOR;
	{
		int aim_pos[3];
		if (metadata_route_wall_target_pos(
		        view, block->wall_num, block->seg, block->side, aim_pos))
			metadata_route_step_set_aim(step, aim_pos);
	}
	metadata_route_add_hidden_door_link(step, block->seg, block->side, block->wall_num);
	child = view->segment_child ? view->segment_child(view->user, block->seg, block->side) : -1;
	reverse_side = valid_segment(view, child) && view->reverse_side ? view->reverse_side(view->user, block->seg, child) : -1;
	reverse_wall = metadata_route_reverse_wall_num(view, block->seg, block->side);
	if (valid_wall(view, reverse_wall))
		metadata_route_add_hidden_door_link(step, child, reverse_side, reverse_wall);
	return 1;
}

static int metadata_route_open_hidden_door(
    const level_metadata_scan_view *view,
    level_metadata_state *state,
    metadata_route_context *route,
    const metadata_route_block *block,
    int depth)
{
	int pos[3];

	if (!block || block->kind != METADATA_ROUTE_BLOCK_HIDDEN_DOOR || !valid_wall(view, block->wall_num)) {
		metadata_route_set_problem(state, "unknown hidden door route dependency");
		return 0;
	}
	if (route->opened_hidden_walls[block->wall_num])
		return 1;
	if (metadata_route_hidden_door_in_progress(view, route, block)) {
		metadata_route_set_problem(state, "hidden door route dependency loop");
		return 0;
	}
	if (!side_center(view, block->seg, block->side, pos)) {
		if (!valid_segment(view, block->seg) || !segment_center_valid[block->seg]) {
			metadata_route_set_problem(state, "hidden door source missing");
			return 0;
		}
		copy_pos(pos, segment_centers[block->seg]);
	}
	metadata_route_set_hidden_door_state(view, block, route->hidden_door_in_progress, 1);
	if (!metadata_route_move_to_target(view, state, route, block->seg, pos, depth + 1)) {
		metadata_route_set_hidden_door_state(view, block, route->hidden_door_in_progress, 0);
		return 0;
	}
	route->current_seg = block->seg;
	copy_pos(route->current_pos, pos);
	if (!metadata_route_append_hidden_door_step(view, state, route, block)) {
		metadata_route_set_hidden_door_state(view, block, route->hidden_door_in_progress, 0);
		return 0;
	}
	metadata_route_set_hidden_door_state(view, block, route->opened_hidden_walls, 1);
	metadata_route_set_hidden_door_state(view, block, route->hidden_door_in_progress, 0);
	return 1;
}

static int metadata_route_acquire_key(
    const level_metadata_scan_view *view,
    level_metadata_state *state,
    metadata_route_context *route,
    int key_index,
    int depth)
{
	metadata_route_path best_path;
	int best = -1;
	int i;

	if (key_index < 0 || key_index >= 3) {
		metadata_route_set_problem(state, "unknown key route dependency");
		return 0;
	}
	if ((route->key_mask & key_bit_for_index(key_index)) != 0)
		return 1;
	if ((route->key_in_progress & key_bit_for_index(key_index)) != 0) {
		char problem[64];
		snprintf(problem, sizeof(problem), "%s key route dependency loop", key_name(key_index));
		metadata_route_set_problem(state, problem);
		route->failed_key = key_index;
		return 0;
	}
	if (key_target_count[key_index] <= 0) {
		char problem[64];
		snprintf(problem, sizeof(problem), "%s key missing", key_name(key_index));
		metadata_route_set_problem(state, problem);
		return 0;
	}
	best_path.distance = DBL_MAX;
	metadata_route_clear_block(&best_path.first_block);
	for (i = 0; i < key_target_count[key_index]; ++i) {
		metadata_route_path candidate;
		if (key_targets[key_index][i].visited)
			continue;
		if (!metadata_route_find_path_forbidden_key(
		        view, route, key_targets[key_index][i].seg, key_targets[key_index][i].pos, 1, key_index, &candidate))
			continue;
		if (candidate.distance < best_path.distance) {
			best = i;
			best_path = candidate;
		}
	}
	if (best < 0) {
		char problem[64];
		snprintf(problem, sizeof(problem), "%s key unreachable", key_name(key_index));
		metadata_route_set_problem(state, problem);
		route->failed_key = key_index;
		return 0;
	}
	route->key_in_progress |= key_bit_for_index(key_index);
	if (!metadata_route_move_to_target(view, state, route, key_targets[key_index][best].seg, key_targets[key_index][best].pos, depth + 1)) {
		route->key_in_progress &= ~key_bit_for_index(key_index);
		return 0;
	}
	{
		char label[LEVEL_METADATA_ROUTE_LABEL_LEN];
		level_metadata_route_step *step;
		snprintf(label, sizeof(label), "%s key", key_name(key_index));
		step = metadata_route_append_step(view, state, route, LEVEL_METADATA_ROUTE_KEY, label, key_targets[key_index][best].seg, -1);
		if (step) {
			step->key_index = key_index;
			step->activation_kind = LEVEL_METADATA_ROUTE_ACTIVATION_PICKUP_KEY;
			metadata_route_step_set_aim(
			    step, key_targets[key_index][best].pos);
		}
	}
	key_targets[key_index][best].visited = 1;
	route->key_mask |= key_bit_for_index(key_index);
	route->key_in_progress &= ~key_bit_for_index(key_index);
	return 1;
}

static void metadata_route_save_progress(
    const level_metadata_state *state,
    const metadata_route_context *route,
    metadata_route_progress_snapshot *snapshot)
{
	int key;
	int i;

	if (!state || !route || !snapshot)
		return;
	snapshot->route = *route;
	snapshot->route_step_count = state->route_step_count;
	snprintf(snapshot->route_problem, sizeof(snapshot->route_problem), "%s", state->route_problem);
	memcpy(snapshot->route_steps, state->route_steps, sizeof(snapshot->route_steps));
	memset(snapshot->key_target_visited, 0, sizeof(snapshot->key_target_visited));
	for (key = 0; key < 3; ++key)
		for (i = 0; i < key_target_count[key]; ++i)
			snapshot->key_target_visited[key][i] = (unsigned char) key_targets[key][i].visited;
}

static void metadata_route_restore_progress(
    level_metadata_state *state,
    metadata_route_context *route,
    const metadata_route_progress_snapshot *snapshot)
{
	int key;
	int i;

	if (!state || !route || !snapshot)
		return;
	*route = snapshot->route;
	state->route_step_count = snapshot->route_step_count;
	snprintf(state->route_problem, sizeof(state->route_problem), "%s", snapshot->route_problem);
	memcpy(state->route_steps, snapshot->route_steps, sizeof(snapshot->route_steps));
	for (key = 0; key < 3; ++key)
		for (i = 0; i < key_target_count[key]; ++i)
			key_targets[key][i].visited = snapshot->key_target_visited[key][i];
}

static int metadata_route_acquire_recovery_key(
    const level_metadata_scan_view *view,
    level_metadata_state *state,
    metadata_route_context *route,
    int depth)
{
	static const int recovery_order[3] = { 0, 2, 1 };
	char last_problem[sizeof(state->route_problem)] = "";
	int key;

	for (key = 0; key < 3; ++key) {
		metadata_route_progress_snapshot snapshot;
		int candidate_key = recovery_order[key];

		if ((route->key_mask & key_bit_for_index(candidate_key)) != 0 ||
		    (route->key_in_progress & key_bit_for_index(candidate_key)) != 0 ||
		    (route->avoided_key_mask & key_bit_for_index(candidate_key)) != 0 ||
		    key_target_count[candidate_key] <= 0)
			continue;
		metadata_route_save_progress(state, route, &snapshot);
		state->route_problem[0] = '\0';
		if (metadata_route_acquire_key(view, state, route, candidate_key, depth + 1))
			return 1;
		if (!last_problem[0] && state->route_problem[0])
			snprintf(last_problem, sizeof(last_problem), "%s", state->route_problem);
		metadata_route_restore_progress(state, route, &snapshot);
	}
	if (last_problem[0])
		snprintf(state->route_problem, sizeof(state->route_problem), "%s", last_problem);
	return 0;
}

static int metadata_route_fire_trigger(
    const level_metadata_scan_view *view,
    level_metadata_state *state,
    metadata_route_context *route,
    const metadata_route_block *block,
    int depth)
{
	metadata_route_block step_block;
	metadata_route_path firing_path;
	int has_firing_path;
	int selected_source_seg;
	int pos[3];

	if (!block || !metadata_route_trigger_valid(block->trigger_num)) {
		metadata_route_set_problem(state, "unknown trigger route dependency");
		return 0;
	}
	if (!valid_segment(view, block->source_seg)) {
		metadata_route_set_problem(state, "trigger source missing");
		return 0;
	}
	step_block = *block;
	has_firing_path = metadata_route_find_trigger_firing_path(view, route, block, &step_block, &firing_path);
	if (route->fired_triggers[step_block.trigger_num])
		return 1;
	if (route->trigger_in_progress[step_block.trigger_num]) {
		char problem[128];
		snprintf(problem, sizeof(problem), "trigger route dependency loop: trigger %d source %d:%d wall %d target %d:%d",
		         step_block.trigger_num,
		         step_block.source_seg,
		         step_block.source_side,
		         step_block.source_wall,
		         step_block.seg,
		         step_block.side);
		metadata_route_set_problem(state, problem);
		route->failed_trigger = step_block.trigger_num;
		return 0;
	}
	if (!metadata_route_trigger_source_pos(view, &step_block, pos)) {
		metadata_route_set_problem(state, "trigger source missing");
		route->failed_trigger = step_block.trigger_num;
		return 0;
	}
	selected_source_seg = step_block.source_seg;
	route->trigger_in_progress[step_block.trigger_num] = 1;
	if (has_firing_path) {
		route->pending_distance += firing_path.distance;
		route->current_seg = firing_path.terminal_seg;
		if (firing_path.terminal_pos_valid)
			copy_pos(route->current_pos, firing_path.terminal_pos);
		else
			copy_pos(route->current_pos, pos);
		step_block.source_seg = route->current_seg;
		if (route->current_seg != selected_source_seg)
			step_block.source_side = -1;
	} else {
		if (!metadata_route_move_to_target(view, state, route, step_block.source_seg, pos, depth + 1)) {
			route->trigger_in_progress[step_block.trigger_num] = 0;
			return 0;
		}
		route->current_seg = step_block.source_seg;
		copy_pos(route->current_pos, pos);
	}
	if (!metadata_route_step_for_trigger(view, state, route, &step_block, 1)) {
		route->trigger_in_progress[step_block.trigger_num] = 0;
		return 0;
	}
	route->trigger_in_progress[step_block.trigger_num] = 0;
	return 1;
}

int level_metadata_scan_route_trigger_dependency_shadow(
    const level_metadata_scan_view *view,
    const level_metadata_route_progress_shadow *progress,
    int seg,
    int side,
    level_metadata_route_trigger_dependency_shadow *result)
{
	metadata_route_context route;
	metadata_route_block block;
	level_metadata_state state;
	int child;

	if (!view || !progress || !result)
		return 0;
	memset(result, 0, sizeof(*result));
	result->failed_trigger = -1;
	result->failed_key = -1;
	metadata_route_context_from_progress_shadow(&route, progress);
	route.failed_trigger = -1;
	route.failed_key = -1;
	metadata_route_context_to_progress_shadow(&route, &result->progress);
	if (!view->segment_child || !valid_segment(view, seg) ||
	    side < 0 || side >= LEVEL_METADATA_MAX_SIDES)
		return 1;
	child = view->segment_child(view->user, seg, side);
	if (!valid_segment(view, child))
		return 1;
	collect_segment_centers(view);
	collect_route_key_targets(view);
	if (!metadata_route_edge_trigger_blocker(
	        view, &route, seg, side, child, &block))
		return 1;
	result->attempted = 1;
	level_metadata_state_clear(&state);
	result->resolved = metadata_route_fire_trigger(
	    view, &state, &route, &block, 0);
	result->failed_trigger = route.failed_trigger;
	result->failed_key = route.failed_key;
	result->pending_distance = route.pending_distance;
	snprintf(result->problem, sizeof(result->problem), "%s",
	         state.route_problem);
	metadata_route_context_to_progress_shadow(&route, &result->progress);
	result->route_step_count = state.route_step_count;
	memcpy(result->route_steps, state.route_steps,
	       sizeof(result->route_steps));
	return 1;
}

static void metadata_route_note_unresolved_block(
    metadata_route_context *route,
    const metadata_route_block *block)
{
	if (!route || !block || route->unresolved_block_valid)
		return;
	if (block->kind != METADATA_ROUTE_BLOCK_TRIGGER &&
	    block->kind != METADATA_ROUTE_BLOCK_HIDDEN_DOOR)
		return;
	route->unresolved_block = *block;
	route->unresolved_block_valid = 1;
}

static int metadata_route_append_unresolved_block(
    const level_metadata_scan_view *view,
    level_metadata_state *state,
    metadata_route_context *route)
{
	if (!route || !route->unresolved_block_valid)
		return 0;
	if (route->unresolved_block.kind == METADATA_ROUTE_BLOCK_TRIGGER)
		return metadata_route_step_for_trigger(view, state, route, &route->unresolved_block, 0);
	if (route->unresolved_block.kind == METADATA_ROUTE_BLOCK_HIDDEN_DOOR)
		return metadata_route_append_hidden_door_step(view, state, route, &route->unresolved_block);
	return 0;
}

static int metadata_route_move_to_target(
    const level_metadata_scan_view *view,
    level_metadata_state *state,
    metadata_route_context *route,
    int goal_seg,
    const int goal_pos[3],
    int depth)
{
	int guard;
	int saved_avoided_key_mask = route->avoided_key_mask;
	unsigned char saved_avoided_triggers[sizeof(route->avoided_triggers)];
	char last_dependency_problem[sizeof(state->route_problem)] = "";

	if (depth > LEVEL_METADATA_MAX_ROUTE_STEPS) {
		metadata_route_set_problem(state, "route dependency depth limit");
		return 0;
	}
	memcpy(saved_avoided_triggers, route->avoided_triggers, sizeof(saved_avoided_triggers));
	for (guard = 0; guard < LEVEL_METADATA_MAX_ROUTE_STEPS; ++guard) {
		metadata_route_path path;
		if (metadata_route_find_path(view, route, goal_seg, goal_pos, 0, &path)) {
			route->pending_distance += path.distance;
			route->current_seg = goal_seg;
			copy_pos(route->current_pos, goal_pos);
			route->avoided_key_mask = saved_avoided_key_mask;
			route->failed_key = -1;
			route->failed_trigger = -1;
			memcpy(route->avoided_triggers, saved_avoided_triggers, sizeof(saved_avoided_triggers));
			return 1;
		}
		if (!metadata_route_find_path(view, route, goal_seg, goal_pos, 1, &path) ||
		    path.first_block.kind == METADATA_ROUTE_BLOCK_NONE) {
			state->route_problem[0] = '\0';
			if (metadata_route_acquire_recovery_key(view, state, route, depth + 1)) {
				route->avoided_key_mask = saved_avoided_key_mask;
				memcpy(route->avoided_triggers, saved_avoided_triggers, sizeof(saved_avoided_triggers));
				route->failed_trigger = -1;
				route->failed_key = -1;
				continue;
			}
			metadata_route_set_problem(state, last_dependency_problem[0] ? last_dependency_problem : "route target unreachable");
			route->avoided_key_mask = saved_avoided_key_mask;
			memcpy(route->avoided_triggers, saved_avoided_triggers, sizeof(saved_avoided_triggers));
			return 0;
		}
		if (path.first_block.kind == METADATA_ROUTE_BLOCK_KEY) {
			metadata_route_progress_snapshot snapshot;
			char key_problem[sizeof(state->route_problem)];
			int failed_key;

			metadata_route_save_progress(state, route, &snapshot);
			route->failed_key = -1;
			if (!metadata_route_acquire_key(view, state, route, path.first_block.key_index, depth + 1)) {
				snprintf(key_problem, sizeof(key_problem), "%s", state->route_problem);
				failed_key = route->failed_key >= 0 && route->failed_key < 3 ? route->failed_key : path.first_block.key_index;
				metadata_route_restore_progress(state, route, &snapshot);
				route->failed_key = failed_key;
				snprintf(last_dependency_problem, sizeof(last_dependency_problem), "%s", key_problem);
				state->route_problem[0] = '\0';
				route->avoided_key_mask |= key_bit_for_index(failed_key);
				continue;
			}
		} else if (path.first_block.kind == METADATA_ROUTE_BLOCK_TRIGGER) {
			metadata_route_progress_snapshot snapshot;
			int failed_trigger;

			metadata_route_save_progress(state, route, &snapshot);
			route->failed_trigger = -1;
			if (!metadata_route_fire_trigger(view, state, route, &path.first_block, depth + 1)) {
				if (metadata_route_trigger_valid(path.first_block.trigger_num) &&
				    metadata_route_problem_is_avoidable_trigger_dependency(state->route_problem)) {
					snprintf(last_dependency_problem, sizeof(last_dependency_problem), "%s", state->route_problem);
					failed_trigger = metadata_route_trigger_valid(route->failed_trigger) ? route->failed_trigger : path.first_block.trigger_num;
					metadata_route_restore_progress(state, route, &snapshot);
					route->failed_trigger = failed_trigger;
					route->avoided_triggers[failed_trigger] = 1;
					continue;
				}
				metadata_route_note_unresolved_block(route, &path.first_block);
				route->avoided_key_mask = saved_avoided_key_mask;
				memcpy(route->avoided_triggers, saved_avoided_triggers, sizeof(saved_avoided_triggers));
				return 0;
			}
		} else if (path.first_block.kind == METADATA_ROUTE_BLOCK_HIDDEN_DOOR) {
			if (!metadata_route_open_hidden_door(view, state, route, &path.first_block, depth + 1)) {
				metadata_route_note_unresolved_block(route, &path.first_block);
				route->avoided_key_mask = saved_avoided_key_mask;
				memcpy(route->avoided_triggers, saved_avoided_triggers, sizeof(saved_avoided_triggers));
				return 0;
			}
		} else {
			metadata_route_set_problem(state, "unsupported route dependency");
			route->avoided_key_mask = saved_avoided_key_mask;
			memcpy(route->avoided_triggers, saved_avoided_triggers, sizeof(saved_avoided_triggers));
			return 0;
		}
	}
	metadata_route_set_problem(state, last_dependency_problem[0] ? last_dependency_problem : "route dependency iteration limit");
	route->avoided_key_mask = saved_avoided_key_mask;
	memcpy(route->avoided_triggers, saved_avoided_triggers, sizeof(saved_avoided_triggers));
	return 0;
}

static int metadata_route_move_to_target_or_visible(
    const level_metadata_scan_view *view,
    level_metadata_state *state,
    metadata_route_context *route,
    const metadata_target *target,
    int depth)
{
	char saved_problem[sizeof(state->route_problem)];
	metadata_route_path path;

	if (!target)
		return 0;
	if (metadata_route_move_to_target(view, state, route, target->seg, target->pos, depth))
		return 1;
	snprintf(saved_problem, sizeof(saved_problem), "%s", state->route_problem);
	state->route_problem[0] = '\0';
	if (!metadata_route_find_visible_path(view, route, target, -1, &path)) {
		metadata_route_set_problem(state, saved_problem[0] ? saved_problem : "route target unreachable");
		return 0;
	}
	route->pending_distance += path.distance;
	route->current_seg = path.terminal_seg;
	if (path.terminal_pos_valid)
		copy_pos(route->current_pos, path.terminal_pos);
	else
		copy_pos(route->current_pos, segment_centers[path.terminal_seg]);
	return 1;
}

static int metadata_route_move_primary_with_key_recovery(
    const level_metadata_scan_view *view,
    level_metadata_state *state,
    metadata_route_context *route,
    const metadata_target *target)
{
	metadata_route_progress_snapshot initial_snapshot;
	char last_problem[sizeof(state->route_problem)] = "";
	int attempt;

	metadata_route_save_progress(state, route, &initial_snapshot);
	if (metadata_route_move_to_target_or_visible(view, state, route, target, 0))
		return 1;
	if (state->route_problem[0])
		snprintf(last_problem, sizeof(last_problem), "%s", state->route_problem);
	metadata_route_restore_progress(state, route, &initial_snapshot);
	for (attempt = 0; attempt < 3; ++attempt) {
		metadata_route_progress_snapshot key_snapshot;

		state->route_problem[0] = '\0';
		if (!metadata_route_acquire_recovery_key(view, state, route, 0))
			break;
		metadata_route_save_progress(state, route, &key_snapshot);
		state->route_problem[0] = '\0';
		if (metadata_route_move_to_target_or_visible(view, state, route, target, 0))
			return 1;
		if (state->route_problem[0])
			snprintf(last_problem, sizeof(last_problem), "%s", state->route_problem);
		metadata_route_restore_progress(state, route, &key_snapshot);
	}
	if (!state->route_problem[0] && last_problem[0])
		snprintf(state->route_problem, sizeof(state->route_problem), "%s", last_problem);
	return 0;
}

static int metadata_route_select_target(
    const level_metadata_scan_view *view,
    const metadata_route_context *route,
    const metadata_target *targets,
    int count)
{
	double best_distance = DBL_MAX;
	int best_progress_weight = INT_MAX;
	int best = -1;
	int i;

	for (i = 0; i < count; ++i) {
		metadata_route_path path;
		if (!metadata_route_find_path(view, route, targets[i].seg, targets[i].pos, 1, &path))
			continue;
		if (path.progress_weight < best_progress_weight ||
		    (path.progress_weight == best_progress_weight && path.distance < best_distance)) {
			best_progress_weight = path.progress_weight;
			best_distance = path.distance;
			best = i;
		}
	}
	return best;
}

int level_metadata_scan_route_select_targets_shadow(
    const level_metadata_scan_view *view,
    const level_metadata_route_progress_shadow *progress,
    const level_metadata_route_target_shadow *targets,
    int count,
    level_metadata_route_target_selection_shadow *selection)
{
	metadata_route_context route;
	metadata_target candidates[LEVEL_METADATA_MAX_TARGETS];
	metadata_route_path path;
	int index;

	if (!view || !progress || !selection || count < 0 ||
	    count > LEVEL_METADATA_MAX_TARGETS || (count > 0 && !targets) ||
	    !valid_segment(view, progress->current_seg))
		return 0;
	selection->selected_index = -1;
	selection->distance = DBL_MAX;
	selection->progress_weight = INT_MAX;
	collect_segment_centers(view);
	metadata_route_context_from_progress_shadow(&route, progress);
	for (index = 0; index < count; ++index) {
		candidates[index].seg = targets[index].seg;
		copy_pos(candidates[index].pos, targets[index].pos);
		candidates[index].visited = 0;
	}
	selection->selected_index = metadata_route_select_target(
	    view, &route, candidates, count);
	if (selection->selected_index < 0)
		return 1;
	if (!metadata_route_find_path(
	        view, &route, candidates[selection->selected_index].seg,
	        candidates[selection->selected_index].pos, 1, &path))
		return 0;
	selection->distance = path.distance;
	selection->progress_weight = path.progress_weight;
	return 1;
}

static int metadata_route_find_boss_target(const level_metadata_scan_view *view, metadata_target *boss)
{
	int obj_count;
	int objnum;

	if (!boss)
		return 0;
	memset(boss, 0, sizeof(*boss));
	if (!view->object_count || !view->object_segment || !view->object_type || !view->object_position || !view->object_is_boss)
		return 0;
	obj_count = view->object_count(view->user);
	for (objnum = 0; objnum < obj_count; ++objnum) {
		int pos[3];
		int obj_seg;
		if (view->object_flags &&
		    (view->object_flags(view->user, objnum) & view->obj_flag_should_be_dead))
			continue;
		if (view->object_type(view->user, objnum) != view->obj_type_robot ||
		    !view->object_is_boss(view->user, objnum))
			continue;
		obj_seg = view->object_segment(view->user, objnum);
		if (!valid_segment(view, obj_seg) || !view->object_position(view->user, objnum, pos))
			continue;
		boss->seg = obj_seg;
		copy_pos(boss->pos, pos);
		return 1;
	}
	return 0;
}

static void metadata_route_copy_target_shadow(
    level_metadata_route_target_shadow *out,
    const metadata_target *target)
{
	out->seg = target->seg;
	copy_pos(out->pos, target->pos);
}

int level_metadata_scan_route_targets_shadow(
    const level_metadata_scan_view *view,
    level_metadata_route_target_inventory_shadow *inventory)
{
	metadata_target reactor;
	metadata_target boss;
	int key;
	int index;

	if (!view || !inventory)
		return 0;
	memset(inventory, 0, sizeof(*inventory));
	collect_segment_centers(view);
	collect_route_key_targets(view);
	inventory->reactor_found = collect_route_targets(
	    view, &reactor, exit_targets, &inventory->exit_count);
	inventory->boss_found = metadata_route_find_boss_target(view, &boss);
	for (key = 0; key < 3; ++key) {
		inventory->key_count[key] = key_target_count[key];
		for (index = 0; index < key_target_count[key]; ++index)
			metadata_route_copy_target_shadow(
			    &inventory->keys[key][index], &key_targets[key][index]);
	}
	if (inventory->reactor_found)
		metadata_route_copy_target_shadow(&inventory->reactor, &reactor);
	if (inventory->boss_found)
		metadata_route_copy_target_shadow(&inventory->boss, &boss);
	for (index = 0; index < inventory->exit_count; ++index)
		metadata_route_copy_target_shadow(
		    &inventory->exits[index], &exit_targets[index]);
	return 1;
}

int level_metadata_scan_route_select_key_shadow(
    const level_metadata_scan_view *view,
    const level_metadata_route_progress_shadow *progress,
    int key_index,
    level_metadata_route_target_selection_shadow *selection)
{
	metadata_route_context route;
	metadata_route_path best_path;
	int index;

	if (!view || !progress || !selection || key_index < 0 || key_index >= 3 ||
	    !valid_segment(view, progress->current_seg))
		return 0;
	selection->selected_index = -1;
	selection->distance = DBL_MAX;
	selection->progress_weight = INT_MAX;
	if (((progress->key_mask | progress->key_in_progress) &
	     key_bit_for_index(key_index)) != 0)
		return 1;
	collect_segment_centers(view);
	collect_route_key_targets(view);
	metadata_route_context_from_progress_shadow(&route, progress);
	metadata_route_clear_path(&best_path);
	for (index = 0; index < key_target_count[key_index]; ++index) {
		metadata_route_path candidate;
		if (!metadata_route_find_path_forbidden_key(
		        view, &route, key_targets[key_index][index].seg,
		        key_targets[key_index][index].pos, 1, key_index, &candidate))
			continue;
		if (candidate.distance >= best_path.distance)
			continue;
		selection->selected_index = index;
		best_path = candidate;
	}
	if (selection->selected_index >= 0) {
		selection->distance = best_path.distance;
		selection->progress_weight = best_path.progress_weight;
	}
	return 1;
}

static void collect_guidebot_info(const level_metadata_scan_view *view, level_metadata_state *state)
{
	metadata_route_context route;
	int obj_count;
	int objnum;
	int start_valid = 0;

	if (!view->object_is_companion || !view->object_count || !view->object_segment || !view->object_position)
		return;
	memset(&route, 0, sizeof(route));
	route.current_seg = view->start_segment;
	route.key_mask = view->initial_key_mask &
	                 (LEVEL_METADATA_KEY_MASK_BLUE |
	                  LEVEL_METADATA_KEY_MASK_RED |
	                  LEVEL_METADATA_KEY_MASK_GOLD);
	route.control_center_destroyed = view->initial_control_center_destroyed != 0;
	if (valid_segment(view, route.current_seg) && view->start_position)
		start_valid = view->start_position(view->user, route.current_pos);
	obj_count = view->object_count(view->user);
	for (objnum = 0; objnum < obj_count; ++objnum) {
		int pos[3];
		int obj_seg;
		if (view->object_flags &&
		    (view->object_flags(view->user, objnum) & view->obj_flag_should_be_dead))
			continue;
		if (!view->object_is_companion(view->user, objnum))
			continue;
		state->guidebot_count++;
		if (state->guidebot_accessible || !start_valid)
			continue;
		obj_seg = view->object_segment(view->user, objnum);
		if (!valid_segment(view, obj_seg) || !view->object_position(view->user, objnum, pos))
			continue;
		if (obj_seg == route.current_seg || metadata_route_find_path(view, &route, obj_seg, pos, 0, NULL))
			state->guidebot_accessible = 1;
	}
	if (state->guidebot_count <= 0) {
		snprintf(
		    state->guidebot_placement_note,
		    sizeof(state->guidebot_placement_note),
		    "%s",
		    "no guidebot or guidebot start cage placed in this level");
	} else if (!state->guidebot_accessible) {
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

static int metadata_route_append_target_step(
    const level_metadata_scan_view *view,
    level_metadata_state *state,
    metadata_route_context *route,
    int kind,
    const metadata_target *target,
    const char *label)
{
	int side = -1;
	int wall_num = -1;
	level_metadata_route_step *step;

	if (kind == LEVEL_METADATA_ROUTE_EXIT && view->side_has_exit_trigger) {
		int s;
		for (s = 0; s < LEVEL_METADATA_MAX_SIDES; ++s) {
			if (!side_has_route_exit(view, target->seg, s))
				continue;
			side = s;
			wall_num = view->wall_num ? view->wall_num(view->user, target->seg, s) : -1;
			break;
		}
	}
	step = metadata_route_append_step(view, state, route, kind, label, target->seg, side);
	if (!step)
		return 0;
	step->wall_num = wall_num;
	metadata_route_step_set_aim(step, target->pos);
	switch (kind) {
		case LEVEL_METADATA_ROUTE_REACTOR:
			step->activation_kind = LEVEL_METADATA_ROUTE_ACTIVATION_DESTROY_REACTOR;
			break;
		case LEVEL_METADATA_ROUTE_BOSS:
			step->activation_kind = LEVEL_METADATA_ROUTE_ACTIVATION_DESTROY_BOSS;
			break;
		case LEVEL_METADATA_ROUTE_EXIT:
			step->activation_kind = LEVEL_METADATA_ROUTE_ACTIVATION_ENTER_EXIT;
			break;
	}
	if (kind == LEVEL_METADATA_ROUTE_EXIT && side >= 0 && view->wall_trigger && valid_wall(view, wall_num)) {
		step->trigger_num = view->wall_trigger(view->user, wall_num);
		if (view->trigger_type && metadata_route_trigger_valid(step->trigger_num)) {
			step->trigger_type = view->trigger_type(view->user, step->trigger_num);
			snprintf(step->trigger_type_name, sizeof(step->trigger_type_name), "%s",
			         metadata_route_trigger_type_name(view, step->trigger_type));
		}
	}
	return 1;
}

static int metadata_route_initialize(
    const level_metadata_scan_view *view,
    level_metadata_state *state,
    metadata_route_context *route)
{
	state->route_status = LEVEL_METADATA_ROUTE_FAILED;
	memset(route, 0, sizeof(*route));
	route->failed_trigger = -1;
	route->failed_key = -1;
	if (!valid_segment(view, view->start_segment) || !view->start_position) {
		metadata_route_set_problem(state, "missing player start");
		return 0;
	}
	route->current_seg = view->start_segment;
	route->key_mask = view->initial_key_mask &
	                  (LEVEL_METADATA_KEY_MASK_BLUE |
	                   LEVEL_METADATA_KEY_MASK_RED |
	                   LEVEL_METADATA_KEY_MASK_GOLD);
	route->control_center_destroyed = view->initial_control_center_destroyed != 0;
	if (!view->start_position(view->user, route->current_pos)) {
		metadata_route_set_problem(state, "missing player start");
		return 0;
	}
	collect_route_key_targets(view);
	return metadata_route_append_step(
	           view, state, route, LEVEL_METADATA_ROUTE_START, "Start", route->current_seg, -1) != NULL;
}

static int metadata_route_progress_primary(
    const level_metadata_scan_view *view,
    level_metadata_state *state,
    metadata_route_context *route,
    int *progressed)
{
	metadata_target reactor;
	metadata_target boss;
	int exit_count = 0;
	int found_reactor = collect_route_targets(view, &reactor, exit_targets, &exit_count);
	int found_boss = metadata_route_find_boss_target(view, &boss);

	if (progressed)
		*progressed = 0;
	if (found_boss) {
		if (!metadata_route_move_primary_with_key_recovery(view, state, route, &boss))
			return 0;
		if (!metadata_route_append_target_step(view, state, route, LEVEL_METADATA_ROUTE_BOSS, &boss, "Boss robot"))
			return 0;
		route->control_center_destroyed = 1;
		if (progressed)
			*progressed = 1;
	} else if (found_reactor) {
		if (!metadata_route_move_primary_with_key_recovery(view, state, route, &reactor))
			return 0;
		if (!metadata_route_append_target_step(view, state, route, LEVEL_METADATA_ROUTE_REACTOR, &reactor, "Reactor"))
			return 0;
		route->control_center_destroyed = 1;
		if (progressed)
			*progressed = 1;
	}
	return 1;
}

static int metadata_route_begin_progression(
    const level_metadata_scan_view *view,
    level_metadata_state *state,
    metadata_route_context *route,
    int require_exit,
    int *exit_count_out)
{
	metadata_target reactor;
	int exit_count = 0;
	int found_reactor;

	if (exit_count_out)
		*exit_count_out = 0;
	if (!metadata_route_initialize(view, state, route))
		return 0;
	found_reactor = collect_route_targets(view, &reactor, exit_targets, &exit_count);
	if (!found_reactor)
		snprintf(state->route_note, sizeof(state->route_note), "%s",
		         exit_count > 0 ? "no reactor, exit exists" : "missing reactor");
	if (exit_count_out)
		*exit_count_out = exit_count;
	if (require_exit && exit_count <= 0) {
		metadata_route_set_problem(state, "missing exit");
		return 0;
	}
	return metadata_route_progress_primary(view, state, route, NULL);
}

static int metadata_route_move_to_endpoint(
    const level_metadata_scan_view *view,
    level_metadata_state *state,
    metadata_route_context *route,
    const metadata_target *target)
{
	metadata_route_progress_snapshot initial;
	char direct_problem[sizeof(state->route_problem)];
	int progressed = 0;

	metadata_route_save_progress(state, route, &initial);
	if (metadata_route_move_to_target(view, state, route, target->seg, target->pos, 0))
		return 1;
	snprintf(direct_problem, sizeof(direct_problem), "%s", state->route_problem);
	metadata_route_restore_progress(state, route, &initial);
	state->route_problem[0] = '\0';
	if (metadata_route_progress_primary(view, state, route, &progressed) && progressed &&
	    metadata_route_move_to_target(view, state, route, target->seg, target->pos, 0))
		return 1;
	if (!progressed) {
		metadata_route_restore_progress(state, route, &initial);
		if (direct_problem[0])
			snprintf(state->route_problem, sizeof(state->route_problem), "%s", direct_problem);
	}
	return 0;
}

static void metadata_route_finish_partial(
    const level_metadata_scan_view *view,
    level_metadata_state *state,
    metadata_route_context *route,
    const char *problem)
{
	metadata_route_append_unresolved_block(view, state, route);
	state->route_status = state->route_step_count > 1 ? LEVEL_METADATA_ROUTE_PARTIAL : LEVEL_METADATA_ROUTE_FAILED;
	if (!state->route_problem[0])
		metadata_route_set_problem(state, problem);
}

static void collect_route_chain(const level_metadata_scan_view *view, level_metadata_state *state)
{
	metadata_route_context route;
	int exit_count = 0;
	int exit_index;

	if (!metadata_route_begin_progression(view, state, &route, 1, &exit_count))
		goto route_partial;
	exit_index = metadata_route_select_target(view, &route, exit_targets, exit_count);
	if (exit_index < 0) {
		metadata_route_set_problem(state, "exit unreachable");
		goto route_partial;
	}
	if (!metadata_route_move_to_target(view, state, &route, exit_targets[exit_index].seg, exit_targets[exit_index].pos, 0))
		goto route_partial;
	if (!metadata_route_append_target_step(view, state, &route, LEVEL_METADATA_ROUTE_EXIT, &exit_targets[exit_index], "Exit"))
		goto route_partial;
	state->route_status = LEVEL_METADATA_ROUTE_OK;
	return;

route_partial:
	metadata_route_finish_partial(view, state, &route, "route incomplete");
}

static void collect_route_travel_metrics(level_metadata_state *state)
{
	int i;

	if (!state)
		return;
	state->travel_distance = 0.0;
	for (i = 0; i < state->route_step_count; ++i)
		if (isfinite(state->route_steps[i].distance_from_previous) &&
		    state->route_steps[i].distance_from_previous > 0.0)
			state->travel_distance += state->route_steps[i].distance_from_previous;
	state->travel_time_seconds = (int) floor(state->travel_distance / LEVEL_METADATA_SHIP_SPEED_UNITS_PER_SECOND + 0.5);
}

static void level_metadata_route_result_clear(level_metadata_state *state)
{
	if (!state)
		return;
	state->route_status = LEVEL_METADATA_ROUTE_FAILED;
	state->route_problem[0] = '\0';
	state->route_step_count = 0;
	memset(state->route_steps, 0, sizeof(state->route_steps));
}

int level_metadata_scan_end_route(
    const level_metadata_scan_view *view,
    level_metadata_state *state)
{
	if (!state || !view_is_valid(view))
		return 0;
	level_metadata_route_result_clear(state);
	collect_segment_centers(view);
	collect_route_chain(view, state);
	return state->route_status == LEVEL_METADATA_ROUTE_OK;
}

int level_metadata_scan_route_to_segment(
    const level_metadata_scan_view *view,
    int target_seg,
    level_metadata_state *state)
{
	metadata_route_context route;
	metadata_target target;

	level_metadata_state_clear(state);
	if (!state || !view_is_valid(view) || !valid_segment(view, target_seg))
		return 0;
	collect_segment_centers(view);
	if (!metadata_route_initialize(view, state, &route))
		goto route_partial;
	if (!segment_center_valid[target_seg]) {
		metadata_route_set_problem(state, "unexplored target missing center");
		goto route_partial;
	}
	target.seg = target_seg;
	copy_pos(target.pos, segment_centers[target_seg]);
	target.visited = 0;
	if (!metadata_route_move_to_endpoint(view, state, &route, &target))
		goto route_partial;
	if (!metadata_route_append_target_step(view, state, &route, LEVEL_METADATA_ROUTE_UNEXPLORED, &target, "Unexplored"))
		goto route_partial;
	state->route_status = LEVEL_METADATA_ROUTE_OK;
	return 1;

route_partial:
	metadata_route_finish_partial(view, state, &route, "unexplored route incomplete");
	return 0;
}

static void metadata_unexplored_route_clear(level_metadata_unexplored_route *result)
{
	if (!result)
		return;
	memset(result, 0, sizeof(*result));
	result->target_seg = -1;
	result->waypoint_seg = -1;
}

static int metadata_route_collect_unexplored_components(
    const level_metadata_scan_view *view,
    const metadata_route_context *route)
{
	metadata_route_context optimistic_route = *route;
	int component_count = 0;
	int seg;

	optimistic_route.control_center_destroyed = 1;
	for (seg = 0; seg < view->num_segments; ++seg)
		component_id[seg] = -1;
	for (seg = 0; seg < view->num_segments; ++seg) {
		int qhead;
		int qtail;

		if (component_id[seg] >= 0 || view->segment_is_explored(view->user, seg))
			continue;
		qhead = 0;
		qtail = 0;
		queue[qtail++] = seg;
		component_id[seg] = component_count;
		unexplored_component_size[component_count] = 0;
		while (qhead < qtail) {
			int current = queue[qhead++];
			int side;

			unexplored_component_size[component_count]++;
			for (side = 0; side < LEVEL_METADATA_MAX_SIDES; ++side) {
				int child = view->segment_child(view->user, current, side);

				if (!valid_segment(view, child) ||
				    component_id[child] >= 0 ||
				    view->segment_is_explored(view->user, child) ||
				    !metadata_route_edge_passable(view, &optimistic_route, current, side, 1, -1, NULL))
					continue;
				component_id[child] = component_count;
				queue[qtail++] = child;
			}
		}
		component_count++;
	}
	return component_count;
}

static void metadata_route_update_unexplored_candidate(int component, int seg, double distance, int direct)
{
	if (component < 0 ||
	    (unexplored_component_target[component] >= 0 &&
	     (distance > unexplored_component_distance[component] ||
	      (distance == unexplored_component_distance[component] &&
	       seg >= unexplored_component_target[component]))))
		return;
	unexplored_component_target[component] = seg;
	unexplored_component_distance[component] = distance;
	unexplored_component_direct[component] = (unsigned char) direct;
}

static void metadata_route_find_unexplored_candidates(
    const level_metadata_scan_view *view,
    const metadata_route_context *route,
    int component_count)
{
	metadata_route_context optimistic_route = *route;
	int component;
	int seg;

	for (component = 0; component < component_count; ++component) {
		unexplored_component_target[component] = -1;
		unexplored_component_distance[component] = DBL_MAX;
		unexplored_component_direct[component] = 0;
		unexplored_component_tried[component] = 0;
	}
	metadata_route_compute_paths(view, route, -1, 0, -1);
	for (seg = 0; seg < view->num_segments; ++seg) {
		component = component_id[seg];
		if (component >= 0 && route_distance[seg] != DBL_MAX)
			metadata_route_update_unexplored_candidate(component, seg, route_distance[seg], 1);
	}
	optimistic_route.control_center_destroyed = 1;
	metadata_route_compute_paths(view, &optimistic_route, -1, 1, -1);
	for (seg = 0; seg < view->num_segments; ++seg) {
		component = component_id[seg];
		if (component >= 0 &&
		    unexplored_component_target[component] < 0 &&
		    route_distance[seg] != DBL_MAX)
			metadata_route_update_unexplored_candidate(component, seg, route_distance[seg], 0);
	}
}

static int metadata_route_unexplored_candidate_is_better(int candidate, int best)
{
	if (best < 0)
		return 1;
	if (unexplored_component_size[candidate] != unexplored_component_size[best])
		return unexplored_component_size[candidate] > unexplored_component_size[best];
	if (unexplored_component_direct[candidate] != unexplored_component_direct[best])
		return unexplored_component_direct[candidate] > unexplored_component_direct[best];
	if (unexplored_component_distance[candidate] != unexplored_component_distance[best])
		return unexplored_component_distance[candidate] < unexplored_component_distance[best];
	return unexplored_component_target[candidate] < unexplored_component_target[best];
}

static int metadata_route_select_unexplored_candidate(int component_count)
{
	int best = -1;
	int component;

	for (component = 0; component < component_count; ++component) {
		if (unexplored_component_tried[component] ||
		    unexplored_component_target[component] < 0)
			continue;
		if (metadata_route_unexplored_candidate_is_better(component, best))
			best = component;
	}
	return best;
}

int level_metadata_scan_unexplored_route(
    const level_metadata_scan_view *view,
    level_metadata_state *state,
    level_metadata_unexplored_route *result)
{
	metadata_route_context route;
	metadata_route_progress_snapshot prefix;
	int component_count;

	level_metadata_state_clear(state);
	metadata_unexplored_route_clear(result);
	if (!state || !view_is_valid(view))
		return 0;
	if (!view->segment_is_explored) {
		metadata_route_set_problem(state, "missing explored segment state");
		return 0;
	}
	collect_segment_centers(view);
	if (!metadata_route_initialize(view, state, &route))
		goto route_partial;
	component_count = metadata_route_collect_unexplored_components(view, &route);
	if (component_count <= 0) {
		metadata_route_set_problem(state, "no unexplored area");
		goto route_partial;
	}
	metadata_route_find_unexplored_candidates(view, &route, component_count);
	metadata_route_save_progress(state, &route, &prefix);
	for (;;) {
		metadata_target target;
		int component = metadata_route_select_unexplored_candidate(component_count);

		if (component < 0)
			break;
		unexplored_component_tried[component] = 1;
		metadata_route_restore_progress(state, &route, &prefix);
		target.seg = unexplored_component_target[component];
		copy_pos(target.pos, segment_centers[target.seg]);
		target.visited = 0;
		if (!metadata_route_move_to_endpoint(view, state, &route, &target))
			continue;
		if (!metadata_route_append_target_step(
		        view, state, &route, LEVEL_METADATA_ROUTE_UNEXPLORED, &target, "Unexplored"))
			continue;
		state->route_status = LEVEL_METADATA_ROUTE_OK;
		if (result) {
			result->component_size = unexplored_component_size[component];
			result->target_seg = target.seg;
			result->waypoint_seg = state->route_step_count > 1 ? state->route_steps[1].seg : target.seg;
			result->direct_reachable = unexplored_component_direct[component] && prefix.route_step_count == 1;
		}
		return 1;
	}
	metadata_route_restore_progress(state, &route, &prefix);
	metadata_route_set_problem(state, "no reachable unexplored area");

route_partial:
	metadata_route_finish_partial(view, state, &route, "unexplored route incomplete");
	return 0;
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
	collect_guidebot_info(view, state);
	collect_route_chain(view, state);
	collect_route_travel_metrics(state);
	return state->energy_center_count;
}
