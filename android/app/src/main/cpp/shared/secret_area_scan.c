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
} candidate_secret;

static int progression_distance[SECRET_AREA_MAX_SEGMENTS];
static int hidden_distance[SECRET_AREA_MAX_SEGMENTS];
static int component_id[SECRET_AREA_MAX_SEGMENTS];
static int queue[SECRET_AREA_MAX_SEGMENTS];
static int component_count[SECRET_AREA_MAX_SEGMENTS];
static int component_lowest_segment[SECRET_AREA_MAX_SEGMENTS];
static candidate_secret candidates[SECRET_AREA_MAX_SEGMENTS];

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

static int find_candidate_by_component(int count, int component)
{
	int i;

	for (i = 0; i < count; ++i)
		if (candidates[i].component == component)
			return i;
	return -1;
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

static void maybe_update_entry(candidate_secret *candidate, int distance, int seg, int side)
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
			int candidate_index;
			int component;
			if (!valid_segment(view, child) || progression_distance[child] >= 0)
				continue;
			if (!is_secret_boundary_edge(view, seg, side, child))
				continue;
			component = component_id[child];
			if (component < 0)
				continue;
			candidate_index = find_candidate_by_component(count, component);
			if (candidate_index < 0) {
				candidate_secret *candidate;
				if (count >= SECRET_AREA_MAX_SEGMENTS)
					break;
				candidate_index = count++;
				candidate = &candidates[candidate_index];
				memset(candidate, 0, sizeof(*candidate));
				candidate->component = component;
				candidate->entry_distance = INT_MAX;
				candidate->entry_seg = INT_MAX;
				candidate->entry_side = INT_MAX;
				candidate->lowest_segment = component_lowest_segment[component];
			}
			maybe_update_entry(&candidates[candidate_index], progression_distance[seg], seg, side);
			append_entrance(&candidates[candidate_index], seg, side, child, side_wall_num(view, seg, side));
		}
	}
	return count;
}

static int component_contains_progress_item(const secret_area_scan_view *view, candidate_secret *candidate)
{
	int obj_count;
	int objnum;
	int seg;

	for (seg = 0; seg < view->num_segments; ++seg) {
		if (component_id[seg] != candidate->component)
			continue;
		if (view->segment_special) {
			int special = view->segment_special(view->user, seg);
			if (special == view->segment_special_control_center)
				return 1;
			if (special == view->segment_special_robotmaker)
				candidate->robotmaker_count++;
		}
	}
	if (!view->object_count)
		return 0;
	obj_count = view->object_count(view->user);
	for (objnum = 0; objnum < obj_count; ++objnum) {
		int type;
		int id;
		int contains_type;
		int contains_id;
		if (view->object_flags &&
		    (view->object_flags(view->user, objnum) & view->obj_flag_should_be_dead))
			continue;
		seg = view->object_segment(view->user, objnum);
		if (!valid_segment(view, seg) || component_id[seg] != candidate->component)
			continue;
		type = view->object_type(view->user, objnum);
		id = view->object_id(view->user, objnum);
		if (type == view->obj_type_none)
			continue;
		if (type == view->obj_type_hostage || type == view->obj_type_control_center)
			return 1;
		if (type == view->obj_type_powerup) {
			if (is_key_powerup(view, id))
				return 1;
			add_candidate_item(view, candidate, id, 1, 0);
		}
		if (type == view->obj_type_robot)
			candidate->robot_count++;
		contains_type = view->object_contains_type ? view->object_contains_type(view->user, objnum) : view->obj_type_none;
		contains_id = view->object_contains_id ? view->object_contains_id(view->user, objnum) : -1;
		if (contains_type == view->obj_type_powerup && is_key_powerup(view, contains_id))
			return 1;
		if (contains_type == view->obj_type_powerup) {
			int contains_count = view->object_contains_count ? view->object_contains_count(view->user, objnum) : 1;
			add_candidate_item(view, candidate, contains_id, contains_count, 1);
		}
	}
	return 0;
}

static int component_hidden_reachable(const secret_area_scan_view *view, int component)
{
	int seg;

	for (seg = 0; seg < view->num_segments; ++seg) {
		if (component_id[seg] != component)
			continue;
		if (hidden_distance[seg] < 0)
			return 0;
	}
	return 1;
}

static void compute_label_pos(const secret_area_scan_view *view, candidate_secret *candidate)
{
	long long total[3] = { 0, 0, 0 };
	int xyz[3];
	int seg;
	int count = 0;

	for (seg = 0; seg < view->num_segments; ++seg) {
		if (component_id[seg] != candidate->component)
			continue;
		if (view->segment_center && view->segment_center(view->user, seg, xyz)) {
			total[0] += xyz[0];
			total[1] += xyz[1];
			total[2] += xyz[2];
			count++;
		}
	}
	if (count > 0) {
		candidate->label_pos[0] = (int) (total[0] / count);
		candidate->label_pos[1] = (int) (total[1] / count);
		candidate->label_pos[2] = (int) (total[2] / count);
		return;
	}
	candidate->label_pos[0] = 0;
	candidate->label_pos[1] = 0;
	candidate->label_pos[2] = 0;
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

static void copy_candidate_to_state(const secret_area_scan_view *view, secret_area_state *state, int index, const candidate_secret *candidate)
{
	secret_area_entry *entry = &state->secrets[index];
	int seg;

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
	for (seg = 0; seg < view->num_segments; ++seg) {
		if (component_id[seg] != candidate->component)
			continue;
		entry->segments[entry->segment_count++] = seg;
		state->segment_to_secret[seg] = entry->display_index;
	}
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
	int i;

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
	bfs_distances(view, hidden_distance, SECRET_AREA_EDGE_ALLOW_HIDDEN | SECRET_AREA_EDGE_ALLOW_PROGRESSION);
	component_total = build_components(view);
	(void) component_total;
	raw_count = collect_raw_candidates(view);
	state->raw_candidate_count = raw_count;
	for (i = 0; i < raw_count; ++i) {
		candidate_secret *candidate = &candidates[i];
		if (!component_hidden_reachable(view, candidate->component))
			continue;
		if (component_contains_progress_item(view, candidate))
			continue;
		if (candidate->item_count == 0)
			continue;
		compute_label_pos(view, candidate);
		sort_candidate_items(candidate);
		candidates[final_count++] = *candidate;
	}
	state->final_candidate_count = final_count;
	if (final_count > max_generated) {
		state->disabled_reason = SECRET_AREA_DISABLED_TOO_MANY_CANDIDATES;
		return 0;
	}
	sort_candidates(candidates, final_count);
	state->enabled = 1;
	for (i = 0; i < final_count; ++i)
		copy_candidate_to_state(view, state, i, &candidates[i]);
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
			int seg = secret->segments[j];
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
