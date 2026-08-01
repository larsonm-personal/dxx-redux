#include "secret_area_scan.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

typedef struct candidate_secret {
	int component;
	int entry_distance;
	int entry_seg;
	int entry_side;
	int lowest_segment;
	int segment_count;
	int label_pos[3];
	int entrance_count;
	secret_area_entrance entrances[SECRET_AREA_MAX_ENTRANCES];
	int robot_count;
	int robotmaker_count;
	int item_count;
	secret_area_item items[SECRET_AREA_MAX_ITEMS];
	long long label_total[3];
	int label_count;
} candidate_secret;

typedef struct candidate_summary {
	int present;
	int entry_distance;
	int entry_seg;
	int entry_side;
	int hidden_reachable;
	int has_non_marginal_entrance;
	int contains_progress_item;
	int has_item;
} candidate_summary;

typedef struct route_side {
	int seg;
	int side;
} route_side;

static int progression_distance[SECRET_AREA_MAX_SEGMENTS];
static int hidden_distance[SECRET_AREA_MAX_SEGMENTS];
static int component_id[SECRET_AREA_MAX_SEGMENTS];
static int queue[SECRET_AREA_MAX_SEGMENTS];
static int component_count[SECRET_AREA_MAX_SEGMENTS];
static int component_lowest_segment[SECRET_AREA_MAX_SEGMENTS];
static unsigned char required_route_side[SECRET_AREA_MAX_SEGMENTS][SECRET_AREA_MAX_SIDES];
static int route_parent_seg[SECRET_AREA_MAX_SEGMENTS];
static int route_parent_side[SECRET_AREA_MAX_SEGMENTS];
static int route_distance[SECRET_AREA_MAX_SEGMENTS];
static int route_queue[SECRET_AREA_MAX_SEGMENTS];
static int route_key_segments[3][SECRET_AREA_MAX_SEGMENTS];
static route_side route_key_doors[3][SECRET_AREA_MAX_SEGMENTS];
static candidate_summary candidate_summaries[SECRET_AREA_MAX_SEGMENTS];
static int selected_candidate[SECRET_AREA_MAX_SEGMENTS];
static candidate_secret candidates[SECRET_AREA_MAX_GENERATED + 1];

void secret_area_state_clear(secret_area_state *state)
{
	if (state)
		memset(state, 0, sizeof(*state));
}

static int valid_segment(const secret_area_scan_view *view, int seg)
{
	return view && seg >= 0 && seg < view->num_segments;
}

static int valid_wall(const secret_area_scan_view *view, int wall_num)
{
	return view && wall_num >= 0 && wall_num < view->num_walls;
}

static int is_key_powerup(const secret_area_scan_view *view, int id)
{
	return id == view->powerup_key_blue || id == view->powerup_key_red ||
	       id == view->powerup_key_gold;
}

static void copy_item_name(secret_area_item *item, const char *name)
{
	size_t length;

	if (!item)
		return;
	if (!name)
		name = "";
	length = strlen(name);
	if (length >= SECRET_AREA_ITEM_NAME_LEN)
		length = SECRET_AREA_ITEM_NAME_LEN - 1;
	memcpy(item->name, name, length);
	item->name[length] = '\0';
}

static void add_candidate_item(const secret_area_scan_view *view, candidate_secret *candidate, int id, int count, int contained)
{
	int index;
	secret_area_item *item;
	const char *name;

	if (!candidate || id < 0 || count <= 0)
		return;
	for (index = 0; index < candidate->item_count; ++index) {
		item = &candidate->items[index];
		if (item->id != id)
			continue;
		item->count += count;
		if (contained)
			item->contained_count += count;
		else
			item->direct_count += count;
		return;
	}
	if (candidate->item_count >= SECRET_AREA_MAX_ITEMS)
		return;
	item = &candidate->items[candidate->item_count++];
	memset(item, 0, sizeof(*item));
	item->id = id;
	item->count = count;
	if (contained)
		item->contained_count = count;
	else
		item->direct_count = count;
	name = view && view->powerup_name ? view->powerup_name(view->user, id) : NULL;
	copy_item_name(item, name);
}

static int edge_has_valid_reverse(const secret_area_scan_view *view, int seg, int side, int child)
{
	int reverse_side;

	if (!valid_segment(view, child) || !view->reverse_side)
		return 0;
	reverse_side = view->reverse_side(view->user, seg, child);
	if (reverse_side < 0 || reverse_side >= SECRET_AREA_MAX_SIDES)
		return 0;
	return view->segment_child(view->user, child, reverse_side) == seg;
}

static int side_has_exit_trigger(const secret_area_scan_view *view, int seg, int side)
{
	if (!view->side_has_exit_trigger)
		return 0;
	return view->side_has_exit_trigger(view->user, seg, side);
}

static int side_wall_num(const secret_area_scan_view *view, int seg, int side)
{
	if (!view->wall_num)
		return -1;
	return view->wall_num(view->user, seg, side);
}

static int is_ordinary_edge(const secret_area_scan_view *view, int seg, int side, int flags);

static int wall_clip_flags(const secret_area_scan_view *view, int wall_num)
{
	if (!valid_wall(view, wall_num) || !view->wall_clip_flags)
		return 0;
	return view->wall_clip_flags(view->user, wall_num);
}

static int is_hidden_door_edge(const secret_area_scan_view *view, int seg, int side, int child)
{
	int wall_num;
	int wall_flags;

	if (!edge_has_valid_reverse(view, seg, side, child))
		return 0;
	wall_num = side_wall_num(view, seg, side);
	if (!valid_wall(view, wall_num))
		return 0;
	if (view->wall_type(view->user, wall_num) != view->wall_type_door)
		return 0;
	if (!(wall_clip_flags(view, wall_num) & view->wall_clip_hidden))
		return 0;
	wall_flags = view->wall_flags(view->user, wall_num);
	if (wall_flags & view->wall_flag_door_locked)
		return 0;
	if (view->wall_keys(view->user, wall_num) != view->wall_key_none)
		return 0;
	return !side_has_exit_trigger(view, seg, side);
}

#define SECRET_AREA_EDGE_ALLOW_HIDDEN      1
#define SECRET_AREA_EDGE_ALLOW_PROGRESSION 2

static int side_has_reachable_trigger_opener(const secret_area_scan_view *view, int seg, int side)
{
	int count;
	int i;

	if (!view->triggered_side_opener_count || !view->triggered_side_opener_segment)
		return 0;
	count = view->triggered_side_opener_count(view->user, seg, side);
	for (i = 0; i < count; ++i) {
		int opener_seg = view->triggered_side_opener_segment(view->user, seg, side, i);
		if (valid_segment(view, opener_seg) && progression_distance[opener_seg] >= 0)
			return 1;
	}
	return 0;
}

static int source_wall_is_progression_pass_through(const secret_area_scan_view *view, int seg, int side, int index)
{
	int source_seg;
	int source_side;
	int source_wall;

	if (!view->triggered_side_opener_wall_num || !view->triggered_side_opener_side)
		return 0;
	source_seg = view->triggered_side_opener_segment(view->user, seg, side, index);
	source_side = view->triggered_side_opener_side(view->user, seg, side, index);
	source_wall = view->triggered_side_opener_wall_num(view->user, seg, side, index);
	if (!valid_segment(view, source_seg) ||
	    source_side < 0 || source_side >= SECRET_AREA_MAX_SIDES ||
	    !valid_wall(view, source_wall))
		return 0;
	return required_route_side[source_seg][source_side] != 0;
}

static int source_wall_is_reachable_pass_through(const secret_area_scan_view *view, int seg, int side, int index)
{
	int source_seg;
	int source_side;
	int source_child;
	int source_wall;

	if (!view->triggered_side_opener_wall_num || !view->triggered_side_opener_side)
		return 0;
	source_seg = view->triggered_side_opener_segment(view->user, seg, side, index);
	source_side = view->triggered_side_opener_side(view->user, seg, side, index);
	source_wall = view->triggered_side_opener_wall_num(view->user, seg, side, index);
	if (!valid_segment(view, source_seg) ||
	    source_side < 0 || source_side >= SECRET_AREA_MAX_SIDES ||
	    !valid_wall(view, source_wall) ||
	    view->wall_type(view->user, source_wall) != view->wall_type_open)
		return 0;
	source_child = view->segment_child(view->user, source_seg, source_side);
	return valid_segment(view, source_child) &&
	       progression_distance[source_child] >= 0 &&
	       is_ordinary_edge(view, source_seg, source_side, SECRET_AREA_EDGE_ALLOW_PROGRESSION);
}

static int progression_pass_through_opener_count(const secret_area_scan_view *view, int seg, int side)
{
	int count;
	int i;
	int pass_through_count = 0;

	if (!view->triggered_side_opener_count)
		return 0;
	count = view->triggered_side_opener_count(view->user, seg, side);
	for (i = 0; i < count; ++i) {
		int opener_seg = view->triggered_side_opener_segment(view->user, seg, side, i);
		if (!valid_segment(view, opener_seg) || progression_distance[opener_seg] < 0)
			continue;
		if (source_wall_is_reachable_pass_through(view, seg, side, i))
			pass_through_count++;
	}
	return pass_through_count;
}

static int side_has_only_marginal_reachable_trigger_openers(const secret_area_scan_view *view, int seg, int side)
{
	int count;
	int i;
	int found = 0;
	int pass_through_count;

	if (!view->triggered_side_opener_count || !view->triggered_side_opener_segment)
		return 0;
	count = view->triggered_side_opener_count(view->user, seg, side);
	pass_through_count = progression_pass_through_opener_count(view, seg, side);
	for (i = 0; i < count; ++i) {
		int opener_seg = view->triggered_side_opener_segment(view->user, seg, side, i);
		if (!valid_segment(view, opener_seg) || progression_distance[opener_seg] < 0)
			continue;
		if (!source_wall_is_progression_pass_through(view, seg, side, i) &&
		    !(pass_through_count >= 2 &&
		      source_wall_is_reachable_pass_through(view, seg, side, i)))
			return 0;
		found = 1;
	}
	return found;
}

static int is_triggered_secret_edge(const secret_area_scan_view *view, int seg, int side, int child)
{
	int wall_num;
	int reverse_side;

	if (!edge_has_valid_reverse(view, seg, side, child))
		return 0;
	wall_num = side_wall_num(view, seg, side);
	if (valid_wall(view, wall_num) &&
	    view->wall_keys(view->user, wall_num) == view->wall_key_none &&
	    side_has_reachable_trigger_opener(view, seg, side))
		return 1;
	reverse_side = view->reverse_side(view->user, seg, child);
	wall_num = side_wall_num(view, child, reverse_side);
	return valid_wall(view, wall_num) &&
	       view->wall_keys(view->user, wall_num) == view->wall_key_none &&
	       side_has_reachable_trigger_opener(view, child, reverse_side);
}

static int is_only_marginal_trigger_edge(const secret_area_scan_view *view, int seg, int side, int child)
{
	int wall_num;
	int reverse_side;

	if (!edge_has_valid_reverse(view, seg, side, child))
		return 0;
	wall_num = side_wall_num(view, seg, side);
	if (valid_wall(view, wall_num) &&
	    view->wall_keys(view->user, wall_num) == view->wall_key_none &&
	    side_has_only_marginal_reachable_trigger_openers(view, seg, side))
		return 1;
	reverse_side = view->reverse_side(view->user, seg, child);
	wall_num = side_wall_num(view, child, reverse_side);
	return valid_wall(view, wall_num) &&
	       view->wall_keys(view->user, wall_num) == view->wall_key_none &&
	       side_has_only_marginal_reachable_trigger_openers(view, child, reverse_side);
}

static int is_secret_boundary_edge(const secret_area_scan_view *view, int seg, int side, int child)
{
	return is_hidden_door_edge(view, seg, side, child) ||
	       is_triggered_secret_edge(view, seg, side, child);
}

static int is_ordinary_edge(const secret_area_scan_view *view, int seg, int side, int flags)
{
	int child = view->segment_child(view->user, seg, side);
	int wall_num;
	int wall_type;
	int wall_flags;
	int wall_keys;

	if (!edge_has_valid_reverse(view, seg, side, child))
		return 0;
	if (is_secret_boundary_edge(view, seg, side, child))
		return (flags & SECRET_AREA_EDGE_ALLOW_HIDDEN) != 0;
	wall_num = side_wall_num(view, seg, side);
	if (!valid_wall(view, wall_num))
		return !side_has_exit_trigger(view, seg, side);
	wall_type = view->wall_type(view->user, wall_num);
	wall_flags = view->wall_flags(view->user, wall_num);
	wall_keys = view->wall_keys(view->user, wall_num);
	if (wall_type == view->wall_type_open || wall_type == view->wall_type_blastable)
		return !side_has_exit_trigger(view, seg, side);
	if (wall_type == view->wall_type_illusion)
		return !(wall_flags & view->wall_flag_illusion_off) && !side_has_exit_trigger(view, seg, side);
	if (wall_type == view->wall_type_door) {
		if (flags & SECRET_AREA_EDGE_ALLOW_PROGRESSION)
			return !side_has_exit_trigger(view, seg, side);
		return wall_keys == view->wall_key_none &&
		       !(wall_flags & view->wall_flag_door_locked) &&
		       !side_has_exit_trigger(view, seg, side);
	}
	return 0;
}

static int route_key_mask_for_index(const secret_area_scan_view *view, int index)
{
	switch (index) {
		case 0:
			return view->wall_key_blue;
		case 1:
			return view->wall_key_red;
		case 2:
			return view->wall_key_gold;
		default:
			return 0;
	}
}

static int powerup_key_index(const secret_area_scan_view *view, int id)
{
	if (id == view->powerup_key_blue)
		return 0;
	if (id == view->powerup_key_red)
		return 1;
	if (id == view->powerup_key_gold)
		return 2;
	return -1;
}

static int route_wall_has_key(const secret_area_scan_view *view, int wall_keys, int index)
{
	int key_mask = route_key_mask_for_index(view, index);

	return key_mask != 0 && (wall_keys & key_mask) != 0;
}

static void append_route_segment(int *segments, int *count, int seg)
{
	int i;

	if (*count >= SECRET_AREA_MAX_SEGMENTS)
		return;
	for (i = 0; i < *count; ++i)
		if (segments[i] == seg)
			return;
	segments[(*count)++] = seg;
}

static void append_route_door(route_side *doors, int *count, int seg, int side)
{
	int i;

	if (*count >= SECRET_AREA_MAX_SEGMENTS)
		return;
	for (i = 0; i < *count; ++i)
		if (doors[i].seg == seg && doors[i].side == side)
			return;
	doors[*count].seg = seg;
	doors[*count].side = side;
	(*count)++;
}

static int is_required_route_edge(const secret_area_scan_view *view, int seg, int side, int allowed_key_mask)
{
	int child = view->segment_child(view->user, seg, side);
	int wall_num;
	int wall_type;
	int wall_flags;
	int wall_keys;

	if (!edge_has_valid_reverse(view, seg, side, child))
		return 0;
	if (is_secret_boundary_edge(view, seg, side, child))
		return 0;
	if (side_has_exit_trigger(view, seg, side))
		return 0;
	wall_num = side_wall_num(view, seg, side);
	if (!valid_wall(view, wall_num))
		return 1;
	wall_type = view->wall_type(view->user, wall_num);
	wall_flags = view->wall_flags(view->user, wall_num);
	wall_keys = view->wall_keys(view->user, wall_num);
	if (wall_type == view->wall_type_open || wall_type == view->wall_type_blastable)
		return 1;
	if (wall_type == view->wall_type_illusion)
		return (wall_flags & view->wall_flag_illusion_off) == 0;
	if (wall_type == view->wall_type_door) {
		if (wall_keys == view->wall_key_none)
			return (wall_flags & view->wall_flag_door_locked) == 0;
		return (wall_keys & allowed_key_mask) != 0;
	}
	return 0;
}

static void mark_required_route_side(const secret_area_scan_view *view, int seg, int side)
{
	int child;
	int reverse_side;

	if (!valid_segment(view, seg) || side < 0 || side >= SECRET_AREA_MAX_SIDES)
		return;
	required_route_side[seg][side] = 1;
	child = view->segment_child(view->user, seg, side);
	if (!valid_segment(view, child))
		return;
	reverse_side = view->reverse_side(view->user, seg, child);
	if (reverse_side >= 0 && reverse_side < SECRET_AREA_MAX_SIDES)
		required_route_side[child][reverse_side] = 1;
}

static int find_required_route_shortest(const secret_area_scan_view *view, int start_seg, int goal_seg, int allowed_key_mask, int mark)
{
	int head = 0;
	int tail = 0;
	int i;

	if (!valid_segment(view, start_seg) || !valid_segment(view, goal_seg))
		return -1;
	for (i = 0; i < view->num_segments; ++i) {
		route_distance[i] = -1;
		route_parent_seg[i] = -1;
		route_parent_side[i] = -1;
	}
	route_distance[start_seg] = 0;
	route_queue[tail++] = start_seg;
	while (head < tail && route_distance[goal_seg] < 0) {
		int seg = route_queue[head++];
		int side;
		for (side = 0; side < SECRET_AREA_MAX_SIDES; ++side) {
			int child = view->segment_child(view->user, seg, side);
			if (!valid_segment(view, child) || route_distance[child] >= 0)
				continue;
			if (!is_required_route_edge(view, seg, side, allowed_key_mask))
				continue;
			route_distance[child] = route_distance[seg] + 1;
			route_parent_seg[child] = seg;
			route_parent_side[child] = side;
			route_queue[tail++] = child;
			if (child == goal_seg)
				break;
		}
	}
	if (route_distance[goal_seg] < 0)
		return -1;
	if (mark) {
		int cur = goal_seg;
		while (cur != start_seg) {
			int parent = route_parent_seg[cur];
			int parent_side = route_parent_side[cur];
			if (!valid_segment(view, parent) || parent_side < 0)
				break;
			mark_required_route_side(view, parent, parent_side);
			cur = parent;
		}
	}
	return route_distance[goal_seg];
}

static int find_reactor_segment(const secret_area_scan_view *view)
{
	int obj_count;
	int objnum;
	int seg;

	if (view->object_count) {
		obj_count = view->object_count(view->user);
		for (objnum = 0; objnum < obj_count; ++objnum) {
			if (view->object_flags &&
			    (view->object_flags(view->user, objnum) & view->obj_flag_should_be_dead))
				continue;
			if (view->object_type(view->user, objnum) != view->obj_type_control_center)
				continue;
			seg = view->object_segment(view->user, objnum);
			if (valid_segment(view, seg))
				return seg;
		}
	}
	if (!view->segment_special)
		return -1;
	for (seg = 0; seg < view->num_segments; ++seg)
		if (view->segment_special(view->user, seg) == view->segment_special_control_center)
			return seg;
	return -1;
}

static void collect_required_route_targets(const secret_area_scan_view *view, int key_counts[3], int door_counts[3], int *reactor_seg)
{
	int obj_count;
	int objnum;
	int seg;

	memset(key_counts, 0, sizeof(int) * 3);
	memset(door_counts, 0, sizeof(int) * 3);
	*reactor_seg = find_reactor_segment(view);
	if (view->object_count) {
		obj_count = view->object_count(view->user);
		for (objnum = 0; objnum < obj_count; ++objnum) {
			int key_index;
			if (view->object_flags &&
			    (view->object_flags(view->user, objnum) & view->obj_flag_should_be_dead))
				continue;
			if (view->object_type(view->user, objnum) != view->obj_type_powerup)
				continue;
			key_index = powerup_key_index(view, view->object_id(view->user, objnum));
			if (key_index < 0)
				continue;
			seg = view->object_segment(view->user, objnum);
			if (valid_segment(view, seg))
				append_route_segment(route_key_segments[key_index], &key_counts[key_index], seg);
		}
	}
	for (seg = 0; seg < view->num_segments; ++seg) {
		int side;
		for (side = 0; side < SECRET_AREA_MAX_SIDES; ++side) {
			int wall_num = side_wall_num(view, seg, side);
			int wall_keys;
			int key_index;
			if (!valid_wall(view, wall_num))
				continue;
			wall_keys = view->wall_keys(view->user, wall_num);
			for (key_index = 0; key_index < 3; ++key_index)
				if (route_wall_has_key(view, wall_keys, key_index))
					append_route_door(route_key_doors[key_index], &door_counts[key_index], seg, side);
		}
	}
}

static void mark_shortest_key_route_to_door(const secret_area_scan_view *view, int key_index, const route_side *door)
{
	int key_mask = route_key_mask_for_index(view, key_index);
	int best_distance = INT_MAX;
	int best_key_seg = -1;
	int i;

	for (i = 0; i < SECRET_AREA_MAX_SEGMENTS && route_key_segments[key_index][i] >= 0; ++i) {
		int key_seg = route_key_segments[key_index][i];
		int distance = find_required_route_shortest(view, key_seg, door->seg, key_mask, 0);
		if (distance >= 0 && distance < best_distance) {
			best_distance = distance;
			best_key_seg = key_seg;
		}
	}
	if (best_key_seg < 0)
		return;
	find_required_route_shortest(view, best_key_seg, door->seg, key_mask, 1);
	mark_required_route_side(view, door->seg, door->side);
}

static void mark_required_routes(const secret_area_scan_view *view)
{
	int key_counts[3];
	int door_counts[3];
	int reactor_seg;
	int key_index;
	int all_key_mask;

	memset(required_route_side, 0, sizeof(required_route_side));
	memset(route_key_segments, -1, sizeof(route_key_segments));
	collect_required_route_targets(view, key_counts, door_counts, &reactor_seg);
	for (key_index = 0; key_index < 3; ++key_index) {
		int key_mask = route_key_mask_for_index(view, key_index);
		int i;
		if (key_counts[key_index] <= 0 || door_counts[key_index] <= 0 || key_mask == 0)
			continue;
		for (i = 0; i < key_counts[key_index]; ++i)
			find_required_route_shortest(view, view->start_segment, route_key_segments[key_index][i], 0, 1);
		for (i = 0; i < door_counts[key_index]; ++i)
			mark_shortest_key_route_to_door(view, key_index, &route_key_doors[key_index][i]);
	}
	all_key_mask = view->wall_key_blue | view->wall_key_red | view->wall_key_gold;
	if (reactor_seg >= 0)
		find_required_route_shortest(view, view->start_segment, reactor_seg, all_key_mask, 1);
}

static void bfs_distances(const secret_area_scan_view *view, int *distances, int flags)
{
	int head = 0;
	int tail = 0;
	int i;

	for (i = 0; i < view->num_segments; ++i)
		distances[i] = -1;
	if (!valid_segment(view, view->start_segment))
		return;
	distances[view->start_segment] = 0;
	queue[tail++] = view->start_segment;
	while (head < tail) {
		int seg = queue[head++];
		int side;
		for (side = 0; side < SECRET_AREA_MAX_SIDES; ++side) {
			int child = view->segment_child(view->user, seg, side);
			if (!valid_segment(view, child) || distances[child] >= 0)
				continue;
			if (!is_ordinary_edge(view, seg, side, flags))
				continue;
			distances[child] = distances[seg] + 1;
			queue[tail++] = child;
		}
	}
}

static int build_components(const secret_area_scan_view *view)
{
	int seg;
	int component = 0;

	for (seg = 0; seg < view->num_segments; ++seg) {
		component_id[seg] = -1;
		component_count[seg] = 0;
		component_lowest_segment[seg] = INT_MAX;
	}
	for (seg = 0; seg < view->num_segments; ++seg) {
		int head;
		int tail;
		if (component_id[seg] >= 0)
			continue;
		head = 0;
		tail = 0;
		component_id[seg] = component;
		queue[tail++] = seg;
		while (head < tail) {
			int cur = queue[head++];
			int side;
			component_count[component]++;
			if (cur < component_lowest_segment[component])
				component_lowest_segment[component] = cur;
			for (side = 0; side < SECRET_AREA_MAX_SIDES; ++side) {
				int child = view->segment_child(view->user, cur, side);
				if (!valid_segment(view, child) || component_id[child] >= 0)
					continue;
				if (!is_ordinary_edge(view, cur, side, 0))
					continue;
				component_id[child] = component;
				queue[tail++] = child;
			}
		}
		component++;
	}
	return component;
}

static void initialize_candidate_summaries(int component_total)
{
	int component;

	memset(candidate_summaries, 0,
	       (size_t) component_total * sizeof(candidate_summaries[0]));
	for (component = 0; component < component_total; ++component) {
		candidate_summaries[component].entry_distance = INT_MAX;
		candidate_summaries[component].entry_seg = INT_MAX;
		candidate_summaries[component].entry_side = INT_MAX;
		candidate_summaries[component].hidden_reachable = 1;
		selected_candidate[component] = -1;
	}
}

static void append_entrance(candidate_secret *candidate, int seg, int side, int secret_seg, int wall_num)
{
	secret_area_entrance *entrance;

	if (candidate->entrance_count >= SECRET_AREA_MAX_ENTRANCES)
		return;
	entrance = &candidate->entrances[candidate->entrance_count++];
	entrance->seg = seg;
	entrance->side = side;
	entrance->secret_seg = secret_seg;
	entrance->wall_num = wall_num;
}

static void maybe_update_summary_entry(candidate_summary *candidate, int distance, int seg, int side)
{
	if (distance < candidate->entry_distance ||
	    (distance == candidate->entry_distance &&
	     (seg < candidate->entry_seg ||
	      (seg == candidate->entry_seg && side < candidate->entry_side)))) {
		candidate->entry_distance = distance;
		candidate->entry_seg = seg;
		candidate->entry_side = side;
	}
}

static int collect_raw_candidates(const secret_area_scan_view *view)
{
	int seg;
	int count = 0;

	for (seg = 0; seg < view->num_segments; ++seg) {
		int side;
		if (progression_distance[seg] < 0)
			continue;
		for (side = 0; side < SECRET_AREA_MAX_SIDES; ++side) {
			int child = view->segment_child(view->user, seg, side);
			int component;
			candidate_summary *candidate;
			if (!valid_segment(view, child) || progression_distance[child] >= 0)
				continue;
			if (!is_secret_boundary_edge(view, seg, side, child))
				continue;
			component = component_id[child];
			if (component < 0)
				continue;
			candidate = &candidate_summaries[component];
			if (!candidate->present) {
				candidate->present = 1;
				count++;
			}
			maybe_update_summary_entry(candidate, progression_distance[seg], seg, side);
			if (!is_only_marginal_trigger_edge(view, seg, side, child))
				candidate->has_non_marginal_entrance = 1;
		}
	}
	return count;
}

static void classify_component_contents(const secret_area_scan_view *view)
{
	int obj_count;
	int objnum;
	int seg;

	for (seg = 0; seg < view->num_segments; ++seg) {
		candidate_summary *summary = &candidate_summaries[component_id[seg]];
		if (hidden_distance[seg] < 0)
			summary->hidden_reachable = 0;
		if (view->segment_special &&
		    view->segment_special(view->user, seg) ==
		        view->segment_special_control_center)
			summary->contains_progress_item = 1;
	}
	if (!view->object_count)
		return;
	obj_count = view->object_count(view->user);
	for (objnum = 0; objnum < obj_count; ++objnum) {
		candidate_summary *summary;
		int type;
		int id;
		int contains_type;
		int contains_id;
		if (view->object_flags &&
		    (view->object_flags(view->user, objnum) & view->obj_flag_should_be_dead))
			continue;
		seg = view->object_segment(view->user, objnum);
		if (!valid_segment(view, seg))
			continue;
		summary = &candidate_summaries[component_id[seg]];
		type = view->object_type(view->user, objnum);
		id = view->object_id(view->user, objnum);
		if (type == view->obj_type_none)
			continue;
		if (type == view->obj_type_hostage || type == view->obj_type_control_center)
			summary->contains_progress_item = 1;
		if (type == view->obj_type_powerup) {
			if (is_key_powerup(view, id))
				summary->contains_progress_item = 1;
			else if (id >= 0)
				summary->has_item = 1;
		}
		contains_type = view->object_contains_type ? view->object_contains_type(view->user, objnum) : view->obj_type_none;
		contains_id = view->object_contains_id ? view->object_contains_id(view->user, objnum) : -1;
		if (contains_type == view->obj_type_powerup && is_key_powerup(view, contains_id))
			summary->contains_progress_item = 1;
		else if (contains_type == view->obj_type_powerup && contains_id >= 0) {
			int contains_count = view->object_contains_count ? view->object_contains_count(view->user, objnum) : 1;
			if (contains_count > 0)
				summary->has_item = 1;
		}
	}
}

static void collect_selected_candidate_details(const secret_area_scan_view *view)
{
	int obj_count;
	int objnum;
	int seg;

	for (seg = 0; seg < view->num_segments; ++seg) {
		int index = selected_candidate[component_id[seg]];
		candidate_secret *candidate;
		int xyz[3];
		if (index < 0)
			continue;
		candidate = &candidates[index];
		candidate->segment_count++;
		if (view->segment_special &&
		    view->segment_special(view->user, seg) ==
		        view->segment_special_robotmaker)
			candidate->robotmaker_count++;
		if (view->segment_center &&
		    view->segment_center(view->user, seg, xyz)) {
			candidate->label_total[0] += xyz[0];
			candidate->label_total[1] += xyz[1];
			candidate->label_total[2] += xyz[2];
			candidate->label_count++;
		}
	}
	if (!view->object_count)
		return;
	obj_count = view->object_count(view->user);
	for (objnum = 0; objnum < obj_count; ++objnum) {
		candidate_secret *candidate;
		int index;
		int type;
		int id;
		int contains_type;
		int contains_id;
		if (view->object_flags &&
		    (view->object_flags(view->user, objnum) &
		     view->obj_flag_should_be_dead))
			continue;
		seg = view->object_segment(view->user, objnum);
		if (!valid_segment(view, seg))
			continue;
		index = selected_candidate[component_id[seg]];
		if (index < 0)
			continue;
		candidate = &candidates[index];
		type = view->object_type(view->user, objnum);
		id = view->object_id(view->user, objnum);
		if (type == view->obj_type_powerup && !is_key_powerup(view, id))
			add_candidate_item(view, candidate, id, 1, 0);
		if (type == view->obj_type_robot)
			candidate->robot_count++;
		contains_type = view->object_contains_type ? view->object_contains_type(view->user, objnum) : view->obj_type_none;
		contains_id = view->object_contains_id ? view->object_contains_id(view->user, objnum) : -1;
		if (contains_type == view->obj_type_powerup &&
		    !is_key_powerup(view, contains_id)) {
			int contains_count = view->object_contains_count ? view->object_contains_count(view->user, objnum) : 1;
			add_candidate_item(view, candidate, contains_id, contains_count, 1);
		}
	}
}

static void collect_selected_candidate_entrances(const secret_area_scan_view *view)
{
	int seg;

	for (seg = 0; seg < view->num_segments; ++seg) {
		int side;
		if (progression_distance[seg] < 0)
			continue;
		for (side = 0; side < SECRET_AREA_MAX_SIDES; ++side) {
			int child = view->segment_child(view->user, seg, side);
			int index;
			if (!valid_segment(view, child) || progression_distance[child] >= 0 ||
			    !is_secret_boundary_edge(view, seg, side, child))
				continue;
			index = selected_candidate[component_id[child]];
			if (index >= 0)
				append_entrance(
				    &candidates[index], seg, side, child,
				    side_wall_num(view, seg, side));
		}
	}
}

static void sort_candidate_items(candidate_secret *candidate);

static void finalize_candidate_details(candidate_secret *candidate)
{
	if (candidate->label_count > 0) {
		candidate->label_pos[0] =
		    (int) (candidate->label_total[0] / candidate->label_count);
		candidate->label_pos[1] =
		    (int) (candidate->label_total[1] / candidate->label_count);
		candidate->label_pos[2] =
		    (int) (candidate->label_total[2] / candidate->label_count);
	}
	sort_candidate_items(candidate);
}

static int compare_candidates(const void *a, const void *b)
{
	const candidate_secret *ca = (const candidate_secret *) a;
	const candidate_secret *cb = (const candidate_secret *) b;

	if (ca->entry_distance != cb->entry_distance)
		return ca->entry_distance < cb->entry_distance ? -1 : 1;
	if (ca->entry_seg != cb->entry_seg)
		return ca->entry_seg < cb->entry_seg ? -1 : 1;
	if (ca->entry_side != cb->entry_side)
		return ca->entry_side < cb->entry_side ? -1 : 1;
	if (ca->lowest_segment != cb->lowest_segment)
		return ca->lowest_segment < cb->lowest_segment ? -1 : 1;
	return 0;
}

static void sort_candidates(candidate_secret *list, int count)
{
	int i;
	int j;

	for (i = 1; i < count; ++i) {
		candidate_secret tmp = list[i];
		j = i - 1;
		while (j >= 0 && compare_candidates(&list[j], &tmp) > 0) {
			list[j + 1] = list[j];
			j--;
		}
		list[j + 1] = tmp;
	}
}

static void sort_candidate_items(candidate_secret *candidate)
{
	int i;
	int j;

	if (!candidate)
		return;
	for (i = 1; i < candidate->item_count; ++i) {
		secret_area_item tmp = candidate->items[i];
		j = i - 1;
		while (j >= 0 && candidate->items[j].id > tmp.id) {
			candidate->items[j + 1] = candidate->items[j];
			j--;
		}
		candidate->items[j + 1] = tmp;
	}
}

static void copy_candidate_to_state(secret_area_state *state, int index, const candidate_secret *candidate)
{
	secret_area_entry *entry = &state->secrets[index];

	memset(entry, 0, sizeof(*entry));
	entry->display_index = index + 1;
	entry->entry_distance = candidate->entry_distance;
	entry->entry_seg = candidate->entry_seg;
	entry->entry_side = candidate->entry_side;
	entry->lowest_segment = candidate->lowest_segment;
	entry->label_pos[0] = candidate->label_pos[0];
	entry->label_pos[1] = candidate->label_pos[1];
	entry->label_pos[2] = candidate->label_pos[2];
	entry->entrance_count = candidate->entrance_count;
	memcpy(entry->entrances, candidate->entrances, sizeof(candidate->entrances));
	entry->robot_count = candidate->robot_count;
	entry->robotmaker_count = candidate->robotmaker_count;
	entry->item_count = candidate->item_count;
	memcpy(entry->items, candidate->items, sizeof(candidate->items));
	entry->segment_count = candidate->segment_count;
}

static int view_is_valid(const secret_area_scan_view *view)
{
	return view &&
	       view->num_segments > 0 &&
	       view->num_segments <= SECRET_AREA_MAX_SEGMENTS &&
	       view->start_segment >= 0 &&
	       view->start_segment < view->num_segments &&
	       view->segment_child &&
	       view->reverse_side &&
	       view->wall_num &&
	       view->wall_type &&
	       view->wall_flags &&
	       view->wall_keys &&
	       view->wall_clip_flags &&
	       view->object_segment &&
	       view->object_type &&
	       view->object_id;
}

int secret_area_scan_level(const secret_area_scan_view *view, secret_area_state *state)
{
	int component_total;
	int raw_count;
	int final_count = 0;
	int max_generated;
	int component;
	int i;
	int segment_offset = 0;
	int segment_write_index[SECRET_AREA_MAX_GENERATED] = { 0 };

	secret_area_state_clear(state);
	if (!state || !view_is_valid(view)) {
		if (state)
			state->disabled_reason = SECRET_AREA_DISABLED_INVALID_VIEW;
		return 0;
	}
	max_generated = view->max_generated > 0 ? view->max_generated : SECRET_AREA_MAX_GENERATED;
	if (max_generated > SECRET_AREA_MAX_GENERATED)
		max_generated = SECRET_AREA_MAX_GENERATED;
	bfs_distances(view, progression_distance, SECRET_AREA_EDGE_ALLOW_PROGRESSION);
	mark_required_routes(view);
	bfs_distances(view, hidden_distance, SECRET_AREA_EDGE_ALLOW_HIDDEN | SECRET_AREA_EDGE_ALLOW_PROGRESSION);
	component_total = build_components(view);
	initialize_candidate_summaries(component_total);
	classify_component_contents(view);
	raw_count = collect_raw_candidates(view);
	state->raw_candidate_count = raw_count;
	for (component = 0; component < component_total; ++component) {
		const candidate_summary *summary = &candidate_summaries[component];
		candidate_secret *candidate;
		if (!summary->present || !summary->hidden_reachable ||
		    !summary->has_non_marginal_entrance ||
		    summary->contains_progress_item || !summary->has_item)
			continue;
		if (final_count > max_generated)
			break;
		candidate = &candidates[final_count];
		memset(candidate, 0, sizeof(*candidate));
		candidate->component = component;
		candidate->entry_distance = summary->entry_distance;
		candidate->entry_seg = summary->entry_seg;
		candidate->entry_side = summary->entry_side;
		candidate->lowest_segment = component_lowest_segment[component];
		selected_candidate[component] = final_count;
		final_count++;
	}
	state->final_candidate_count = final_count;
	if (final_count > max_generated) {
		state->disabled_reason = SECRET_AREA_DISABLED_TOO_MANY_CANDIDATES;
		return 0;
	}
	collect_selected_candidate_details(view);
	collect_selected_candidate_entrances(view);
	for (i = 0; i < final_count; ++i)
		finalize_candidate_details(&candidates[i]);
	sort_candidates(candidates, final_count);
	state->enabled = 1;
	for (component = 0; component < component_total; ++component)
		selected_candidate[component] = -1;
	for (i = 0; i < final_count; ++i) {
		copy_candidate_to_state(state, i, &candidates[i]);
		state->secrets[i].segment_offset = segment_offset;
		segment_write_index[i] = segment_offset;
		segment_offset += state->secrets[i].segment_count;
		selected_candidate[candidates[i].component] = i;
	}
	for (i = 0; i < view->num_segments; ++i) {
		int index = selected_candidate[component_id[i]];
		secret_area_entry *entry;
		if (index < 0)
			continue;
		entry = &state->secrets[index];
		state->segments[segment_write_index[index]++] = i;
		state->segment_to_secret[i] = entry->display_index;
	}
	return final_count;
}

int secret_area_mark_segment_entered(secret_area_state *state, int seg)
{
	int display_index;

	if (!state || !state->enabled || seg < 0 || seg >= SECRET_AREA_MAX_SEGMENTS)
		return 0;
	display_index = state->segment_to_secret[seg];
	if (display_index <= 0 || display_index > SECRET_AREA_MAX_GENERATED)
		return 0;
	if (state->found[display_index - 1])
		return 0;
	state->found[display_index - 1] = 1;
	state->found_count++;
	return display_index;
}

static void clear_found_bits(secret_area_state *state)
{
	if (!state)
		return;
	memset(state->found, 0, sizeof(state->found));
	state->found_count = 0;
}

void secret_area_restore_found(secret_area_state *state, int saved_total, const unsigned char *found, int found_capacity)
{
	int i;
	int total = secret_area_total(state);

	clear_found_bits(state);
	if (!state || !state->enabled || !found || found_capacity <= 0 || saved_total != total)
		return;
	if (found_capacity > total)
		found_capacity = total;
	for (i = 0; i < found_capacity; ++i) {
		if (!found[i])
			continue;
		state->found[i] = 1;
		state->found_count++;
	}
}

void secret_area_restore_found_from_visited(secret_area_state *state, const unsigned char *visited, int visited_count)
{
	int i;
	int total = secret_area_total(state);

	clear_found_bits(state);
	if (!state || !state->enabled || !visited || visited_count <= 0)
		return;
	for (i = 0; i < total; ++i) {
		const secret_area_entry *secret = &state->secrets[i];
		int j;
		for (j = 0; j < secret->segment_count; ++j) {
			int seg = state->segments[secret->segment_offset + j];
			if (seg >= 0 && seg < visited_count && visited[seg]) {
				state->found[i] = 1;
				state->found_count++;
				break;
			}
		}
	}
}

int secret_area_total(const secret_area_state *state)
{
	if (!state || !state->enabled)
		return 0;
	return state->final_candidate_count;
}

int secret_area_found_count(const secret_area_state *state)
{
	if (!state || !state->enabled)
		return 0;
	return state->found_count;
}

const char *secret_area_disabled_reason_name(int reason)
{
	switch (reason) {
		case SECRET_AREA_DISABLED_NONE:
			return "none";
		case SECRET_AREA_DISABLED_INVALID_VIEW:
			return "invalid_view";
		case SECRET_AREA_DISABLED_TOO_MANY_CANDIDATES:
			return "too_many_candidates";
		default:
			return "unknown";
	}
}
