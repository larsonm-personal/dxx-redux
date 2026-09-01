#include "guidebot_route_certifier.h"

#include <stdio.h>
#include <string.h>

static int guidebot_certifier_budget_exhausted(
    const guidebot_route_certifier_budget *budget,
    unsigned int work)
{
	return budget &&
	       ((budget->work_limit && work >= budget->work_limit) ||
	        (budget->clock_us &&
	         budget->clock_us(budget->clock_user) >= budget->deadline_us));
}

void guidebot_route_certifier_reset_job(
    guidebot_route_certifier_workspace *workspace)
{
	if (!workspace)
		return;
	workspace->job_active = 0;
	workspace->reach_complete = 0;
	workspace->firing_search_active = 0;
	workspace->unexplored_active = 0;
}

static int guidebot_valid_segment(
    const level_metadata_scan_view *view,
    int segment)
{
	return view && segment >= 0 && segment < view->num_segments &&
	       segment < LEVEL_METADATA_MAX_SEGMENTS;
}

static int guidebot_key_allowed(
    const level_metadata_scan_view *view,
    int key)
{
	if (key == view->wall_key_none)
		return 1;
	if (key == view->wall_key_blue)
		return (view->initial_key_mask & LEVEL_METADATA_KEY_MASK_BLUE) != 0;
	if (key == view->wall_key_red)
		return (view->initial_key_mask & LEVEL_METADATA_KEY_MASK_RED) != 0;
	if (key == view->wall_key_gold)
		return (view->initial_key_mask & LEVEL_METADATA_KEY_MASK_GOLD) != 0;
	return 0;
}

static int guidebot_route_side_passable(
    const level_metadata_scan_view *view,
    int segment,
    int side,
    int allow_player_keyed_door,
    int allow_control_center_link,
    int allow_triggered_link)
{
	int child;
	int clearance;
	int hard_blocked;
	int local_hard_blocked;
	int key;
	int wall;
	int reverse_hard_blocked;
	int reverse_wall;
	int type;
	int flags;
	int reverse;

	child = view->segment_child(view->user, segment, side);
	if (!guidebot_valid_segment(view, child))
		return 0;
	if (view->reverse_side) {
		reverse = view->reverse_side(view->user, segment, child);
		if (reverse < 0 || reverse >= LEVEL_METADATA_MAX_SIDES ||
		    view->segment_child(view->user, child, reverse) != segment)
			return 0;
	} else
		reverse = -1;
	if (view->side_clearance_radius) {
		clearance = view->side_clearance_radius(view->user, segment, side);
		if (view->navigator_radius > 0 && clearance > 0 &&
		    clearance < view->navigator_radius)
			return 0;
	}
	if (view->side_has_exit_trigger &&
	    view->side_has_exit_trigger(view->user, segment, side))
		return 0;
	if (view->side_is_flyable &&
	    view->side_is_flyable(view->user, segment, side))
		return 1;
	if (view->side_is_control_center_link &&
	    (view->side_is_control_center_link(view->user, segment, side) ||
	     (reverse >= 0 && view->side_is_control_center_link(
	                          view->user, child, reverse))))
		return view->initial_control_center_destroyed ||
		       allow_control_center_link;
	local_hard_blocked =
	    view->side_is_hard_blocked &&
	    view->side_is_hard_blocked(view->user, segment, side);
	reverse_hard_blocked =
	    view->side_is_hard_blocked && reverse >= 0 &&
	    view->side_is_hard_blocked(view->user, child, reverse);
	hard_blocked = local_hard_blocked || reverse_hard_blocked;
	if (hard_blocked && !allow_player_keyed_door)
		return 0;
	wall = view->wall_num ? view->wall_num(view->user, segment, side) : -1;
	reverse_wall = view->wall_num && reverse >= 0 ? view->wall_num(view->user, child, reverse) : -1;
	/* A portal may carry the Buddy-proof keyed door only on its reverse side.
	 * Once that side supplied the hard block, use the same side for the key and
	 * door properties instead of incorrectly inspecting an unrelated forward
	 * wall. */
	if (reverse_hard_blocked && !local_hard_blocked)
		wall = reverse_wall;
	else if ((wall < 0 || wall >= view->num_walls) &&
	         reverse_wall >= 0 && reverse_wall < view->num_walls)
		wall = reverse_wall;
	if (wall < 0 || wall >= view->num_walls)
		return !hard_blocked;
	if (hard_blocked && !view->wall_keys)
		return 0;
	key = view->wall_keys ? view->wall_keys(view->user, wall)
	                      : view->wall_key_none;
	if (hard_blocked &&
	    (key == view->wall_key_none || !guidebot_key_allowed(view, key)))
		return 0;
	type = view->wall_type ? view->wall_type(view->user, wall) : -1;
	flags = view->wall_flags ? view->wall_flags(view->user, wall) : 0;
	if (type == view->wall_type_open ||
	    (flags & view->wall_flag_door_opened) != 0 ||
	    (view->wall_is_opening && view->wall_is_opening(view->user, wall)))
		return 1;
	if (type != view->wall_type_door)
		return 0;
	if ((flags & view->wall_flag_door_locked) != 0)
		return 0;
	if (view->wall_clip_flags &&
	    (view->wall_clip_flags(view->user, wall) & view->wall_clip_hidden) != 0)
		return 0;
	/* While pursuing an objective, classic Guide-Bot can plan through keyed
	 * doors but does not open them on collision.  Keep those doors out of the
	 * physical route; the strategic route may still cross them when the player
	 * owns the key. */
	if (key != view->wall_key_none && !allow_player_keyed_door)
		return 0;
	if (view->triggered_side_opener_count &&
	    (view->triggered_side_opener_count(view->user, segment, side) > 0 ||
	     (reverse >= 0 && view->triggered_side_opener_count(
	                          view->user, child, reverse) > 0)) &&
	    key == view->wall_key_none && !allow_triggered_link)
		return 0;
	return guidebot_key_allowed(view, key);
}

int guidebot_route_side_passable_current(
    const level_metadata_scan_view *view,
    int segment,
    int side)
{
	/* Match the classic companion's physical door handling: a visible,
	 * unlocked door remains usable even when a trigger also controls it.
	 * Hidden and locked trigger doors are still rejected below. */
	return guidebot_route_side_passable(view, segment, side, 0, 0, 1);
}

int guidebot_route_side_progress_reachable_current(
    const level_metadata_scan_view *view,
    int segment,
    int side)
{
	return guidebot_route_side_passable(view, segment, side, 1, 0, 0);
}

static int guidebot_wall_is_player_openable_keyed_door(
    const level_metadata_scan_view *view,
    int wall)
{
	int flags;
	int key;

	if (!view || wall < 0 || wall >= view->num_walls || !view->wall_keys ||
	    !view->wall_type)
		return 0;
	key = view->wall_keys(view->user, wall);
	if (key == view->wall_key_none || !guidebot_key_allowed(view, key) ||
	    view->wall_type(view->user, wall) != view->wall_type_door)
		return 0;
	flags = view->wall_flags ? view->wall_flags(view->user, wall) : 0;
	if ((flags & (view->wall_flag_door_opened |
	              view->wall_flag_door_locked)) != 0 ||
	    (view->wall_is_opening && view->wall_is_opening(view->user, wall)) ||
	    (view->wall_clip_flags &&
	     (view->wall_clip_flags(view->user, wall) & view->wall_clip_hidden) != 0))
		return 0;
	return 1;
}

int guidebot_route_segment_has_player_openable_keyed_door(
    const level_metadata_scan_view *view,
    int segment)
{
	int side;

	if (!guidebot_valid_segment(view, segment) || !view->segment_child ||
	    !view->wall_num)
		return 0;
	for (side = 0; side < LEVEL_METADATA_MAX_SIDES; ++side) {
		const int child = view->segment_child(view->user, segment, side);
		const int wall = view->wall_num(view->user, segment, side);

		if (guidebot_wall_is_player_openable_keyed_door(view, wall))
			return 1;
		if (guidebot_valid_segment(view, child) && view->reverse_side) {
			const int reverse =
			    view->reverse_side(view->user, segment, child);
			const int reverse_wall =
			    reverse >= 0 && reverse < LEVEL_METADATA_MAX_SIDES ? view->wall_num(view->user, child, reverse) : -1;

			if (guidebot_wall_is_player_openable_keyed_door(
			        view, reverse_wall))
				return 1;
		}
	}
	return 0;
}

static int guidebot_route_best_physical_frontier_internal(
    const level_metadata_scan_view *view,
    int start_segment,
    int goal_segment,
    int max_depth,
    int avoid_from,
    int avoid_to,
    int avoid_from2,
    int avoid_to2,
    int allow_control_center_link,
    guidebot_route_certifier_workspace *workspace)
{
	int best_remaining;
	int best_segment;
	int head;
	int segment;
	int side;
	int tail;

	if (!workspace || !view || view->num_segments > LEVEL_METADATA_MAX_SEGMENTS ||
	    !view->segment_child || !view->reverse_side ||
	    !guidebot_valid_segment(view, start_segment) ||
	    !guidebot_valid_segment(view, goal_segment))
		return -1;
	for (segment = 0; segment < view->num_segments; ++segment) {
		workspace->strategic_distance[segment] = -1;
		workspace->physical_distance[segment] = -1;
	}
	head = 0;
	tail = 0;
	workspace->strategic_distance[goal_segment] = 0;
	workspace->queue[tail++] = goal_segment;
	while (head < tail) {
		int child;
		int reverse;

		segment = workspace->queue[head++];
		for (side = 0; side < LEVEL_METADATA_MAX_SIDES; ++side) {
			child = view->segment_child(view->user, segment, side);
			if (!guidebot_valid_segment(view, child) ||
			    workspace->strategic_distance[child] >= 0)
				continue;
			reverse = view->reverse_side(view->user, segment, child);
			if (reverse < 0 || reverse >= LEVEL_METADATA_MAX_SIDES ||
			    !guidebot_route_side_passable(
			        view, child, reverse, 1,
			        allow_control_center_link, 1))
				continue;
			workspace->strategic_distance[child] =
			    workspace->strategic_distance[segment] + 1;
			workspace->queue[tail++] = child;
		}
	}
	if (workspace->strategic_distance[start_segment] < 0)
		return -1;
	head = 0;
	tail = 0;
	best_segment = start_segment;
	best_remaining = workspace->strategic_distance[start_segment];
	workspace->physical_distance[start_segment] = 0;
	workspace->queue[tail++] = start_segment;
	while (head < tail) {
		int child;

		segment = workspace->queue[head++];
		if (workspace->strategic_distance[segment] >= 0 &&
		    workspace->strategic_distance[segment] < best_remaining) {
			best_segment = segment;
			best_remaining = workspace->strategic_distance[segment];
		}
		if (best_remaining == 0)
			break;
		if (max_depth > 0 &&
		    workspace->physical_distance[segment] + 1 >= max_depth)
			continue;
		for (side = 0; side < LEVEL_METADATA_MAX_SIDES; ++side) {
			child = view->segment_child(view->user, segment, side);
			if (!guidebot_valid_segment(view, child) ||
			    workspace->physical_distance[child] >= 0 ||
			    ((segment == avoid_from && child == avoid_to) ||
			     (segment == avoid_to && child == avoid_from) ||
			     (segment == avoid_from2 && child == avoid_to2) ||
			     (segment == avoid_to2 && child == avoid_from2)) ||
			    !guidebot_route_side_passable_current(view, segment, side))
				continue;
			workspace->physical_distance[child] =
			    workspace->physical_distance[segment] + 1;
			workspace->queue[tail++] = child;
		}
	}
	return best_segment;
}

int guidebot_route_best_physical_frontier(
    const level_metadata_scan_view *view,
    int start_segment,
    int goal_segment,
    int max_depth,
    int avoid_from,
    int avoid_to,
    int avoid_from2,
    int avoid_to2,
    guidebot_route_certifier_workspace *workspace)
{
	return guidebot_route_best_physical_frontier_internal(
	    view, start_segment, goal_segment, max_depth, avoid_from, avoid_to,
	    avoid_from2, avoid_to2, 0, workspace);
}

int guidebot_route_best_deferred_countdown_frontier(
    const level_metadata_scan_view *view,
    int start_segment,
    int goal_segment,
    int max_depth,
    int avoid_from,
    int avoid_to,
    int avoid_from2,
    int avoid_to2,
    guidebot_route_certifier_workspace *workspace)
{
	return guidebot_route_best_physical_frontier_internal(
	    view, start_segment, goal_segment, max_depth, avoid_from, avoid_to,
	    avoid_from2, avoid_to2, 1, workspace);
}

static int guidebot_build_reachability(
    const level_metadata_scan_view *view,
    guidebot_route_certifier_workspace *workspace,
    guidebot_route_certifier_summary *summary,
    const guidebot_route_certifier_budget *budget,
    unsigned int *tick_work)
{
	if (!guidebot_valid_segment(view, view->start_segment) ||
	    !view->segment_child)
		return GUIDEBOT_ROUTE_CERTIFIER_INVALID;
	if (!workspace->job_active ||
	    workspace->job_num_segments != view->num_segments) {
		int segment;

		workspace->job_active = 1;
		workspace->job_start_segment = view->start_segment;
		workspace->job_num_segments = view->num_segments;
		workspace->reach_head = 0;
		workspace->reach_tail = 1;
		workspace->reach_side = 0;
		workspace->reach_complete = 0;
		workspace->job_visited_segments = 0;
		workspace->job_evaluated_edges = 0;
		workspace->job_evaluated_firing_positions = 0;
		workspace->firing_search_active = 0;
		memset(workspace->reachable, 0, sizeof(workspace->reachable));
		for (segment = 0; segment < view->num_segments; ++segment)
			workspace->physical_distance[segment] = -1;
		workspace->reachable[view->start_segment] = 1;
		workspace->physical_distance[view->start_segment] = 0;
		workspace->queue[0] = view->start_segment;
	}
	while (!workspace->reach_complete) {
		int segment;

		if (workspace->reach_head >= workspace->reach_tail) {
			workspace->reach_complete = 1;
			break;
		}
		if (guidebot_certifier_budget_exhausted(budget, *tick_work))
			return GUIDEBOT_ROUTE_CERTIFIER_PENDING;
		segment = workspace->queue[workspace->reach_head];
		while (workspace->reach_side < LEVEL_METADATA_MAX_SIDES) {
			int child;
			int side;

			if (guidebot_certifier_budget_exhausted(
			        budget, *tick_work))
				return GUIDEBOT_ROUTE_CERTIFIER_PENDING;
			side = workspace->reach_side++;

			workspace->job_evaluated_edges++;
			(*tick_work)++;
			child = view->segment_child(view->user, segment, side);
			if (!guidebot_valid_segment(view, child) ||
			    workspace->reachable[child] ||
			    !guidebot_route_side_progress_reachable_current(
			        view, segment, side))
				continue;
			workspace->reachable[child] = 1;
			workspace->physical_distance[child] =
			    workspace->physical_distance[segment] + 1;
			workspace->queue[workspace->reach_tail++] = child;
			if (guidebot_certifier_budget_exhausted(
			        budget, *tick_work))
				return GUIDEBOT_ROUTE_CERTIFIER_PENDING;
		}
		workspace->reach_side = 0;
		workspace->reach_head++;
		workspace->job_visited_segments++;
	}
	summary->visited_segments = workspace->job_visited_segments;
	summary->evaluated_edges = workspace->job_evaluated_edges;
	return GUIDEBOT_ROUTE_CERTIFIER_VALID;
}

int guidebot_route_find_unexplored_budgeted(
    const level_metadata_scan_view *view,
    guidebot_route_certifier_workspace *workspace,
    level_metadata_unexplored_route *result,
    const guidebot_route_certifier_budget *budget)
{
	guidebot_route_certifier_summary summary;
	unsigned int tick_work = 0;
	int reachability;

	if (!view || !workspace || !result || !view->segment_is_explored ||
	    !view->segment_child)
		return GUIDEBOT_ROUTE_CERTIFIER_INVALID;
	reachability = guidebot_build_reachability(
	    view, workspace, &summary, budget, &tick_work);
	if (reachability != GUIDEBOT_ROUTE_CERTIFIER_VALID)
		return reachability;
	if (!workspace->unexplored_active) {
		workspace->unexplored_active = 1;
		workspace->unexplored_init_segment = 0;
		workspace->unexplored_scan_segment = 0;
		workspace->unexplored_component_head = 0;
		workspace->unexplored_component_tail = 0;
		workspace->unexplored_component_side = 0;
		workspace->unexplored_component_size = 0;
		workspace->unexplored_component_target = -1;
		workspace->unexplored_component_distance = -1;
		workspace->unexplored_best_size = 0;
		workspace->unexplored_best_target = -1;
		workspace->unexplored_best_distance = -1;
	}
	while (workspace->unexplored_init_segment < view->num_segments) {
		if (guidebot_certifier_budget_exhausted(budget, tick_work))
			return GUIDEBOT_ROUTE_CERTIFIER_PENDING;
		workspace->strategic_distance
		    [workspace->unexplored_init_segment++] = -1;
		tick_work++;
	}
	for (;;) {
		if (workspace->unexplored_component_head >=
		    workspace->unexplored_component_tail) {
			if (workspace->unexplored_component_size > 0 &&
			    (workspace->unexplored_component_size >
			         workspace->unexplored_best_size ||
			     (workspace->unexplored_component_size ==
			          workspace->unexplored_best_size &&
			      ((workspace->unexplored_component_distance >= 0) >
			           (workspace->unexplored_best_distance >= 0) ||
			       ((workspace->unexplored_component_distance >= 0) ==
			            (workspace->unexplored_best_distance >= 0) &&
			        ((workspace->unexplored_component_distance >= 0 &&
			          (workspace->unexplored_component_distance <
			               workspace->unexplored_best_distance ||
			           (workspace->unexplored_component_distance ==
			                workspace->unexplored_best_distance &&
			            workspace->unexplored_component_target <
			                workspace->unexplored_best_target))) ||
			         (workspace->unexplored_component_distance < 0 &&
			          workspace->unexplored_component_target <
			              workspace->unexplored_best_target))))))) {
				workspace->unexplored_best_size =
				    workspace->unexplored_component_size;
				workspace->unexplored_best_target =
				    workspace->unexplored_component_target;
				workspace->unexplored_best_distance =
				    workspace->unexplored_component_distance;
			}
			while (workspace->unexplored_scan_segment < view->num_segments &&
			       (workspace->strategic_distance
			                [workspace->unexplored_scan_segment] >= 0 ||
			        view->segment_is_explored(
			            view->user,
			            workspace->unexplored_scan_segment))) {
				if (guidebot_certifier_budget_exhausted(
				        budget, tick_work))
					return GUIDEBOT_ROUTE_CERTIFIER_PENDING;
				workspace->unexplored_scan_segment++;
				tick_work++;
			}
			if (workspace->unexplored_scan_segment >= view->num_segments)
				break;
			workspace->unexplored_component_head = 0;
			workspace->unexplored_component_tail = 1;
			workspace->unexplored_component_side = 0;
			workspace->unexplored_component_size = 1;
			workspace->unexplored_component_target =
			    workspace->unexplored_scan_segment;
			workspace->unexplored_component_distance = -1;
			workspace->queue[0] = workspace->unexplored_scan_segment;
			workspace->strategic_distance
			    [workspace->unexplored_scan_segment] =
			    workspace->unexplored_scan_segment;
			workspace->unexplored_scan_segment++;
		}
		while (workspace->unexplored_component_head <
		       workspace->unexplored_component_tail) {
			const int segment = workspace->queue
			                        [workspace->unexplored_component_head];

			if (workspace->unexplored_component_side == 0) {
				if (workspace->reachable[segment] &&
				    (workspace->unexplored_component_distance < 0 ||
				     workspace->physical_distance[segment] <
				         workspace->unexplored_component_distance ||
				     (workspace->physical_distance[segment] ==
				          workspace->unexplored_component_distance &&
				      segment <
				          workspace->unexplored_component_target))) {
					workspace->unexplored_component_target = segment;
					workspace->unexplored_component_distance =
					    workspace->physical_distance[segment];
				}
			}
			while (workspace->unexplored_component_side <
			       LEVEL_METADATA_MAX_SIDES) {
				int child;
				int side;

				if (guidebot_certifier_budget_exhausted(
				        budget, tick_work))
					return GUIDEBOT_ROUTE_CERTIFIER_PENDING;
				side = workspace->unexplored_component_side++;
				child = view->segment_child(view->user, segment, side);
				tick_work++;
				if (!guidebot_valid_segment(view, child) ||
				    workspace->strategic_distance[child] >= 0 ||
				    view->segment_is_explored(view->user, child))
					continue;
				workspace->strategic_distance[child] = child;
				workspace->queue
				    [workspace->unexplored_component_tail++] = child;
				workspace->unexplored_component_size++;
			}
			workspace->unexplored_component_side = 0;
			workspace->unexplored_component_head++;
		}
	}
	if (workspace->unexplored_best_target < 0) {
		guidebot_route_certifier_reset_job(workspace);
		return GUIDEBOT_ROUTE_CERTIFIER_INVALID;
	}
	result->component_size = workspace->unexplored_best_size;
	result->target_seg = workspace->unexplored_best_target;
	result->waypoint_seg = workspace->unexplored_best_target;
	result->direct_reachable =
	    workspace->unexplored_best_distance >= 0;
	guidebot_route_certifier_reset_job(workspace);
	return GUIDEBOT_ROUTE_CERTIFIER_VALID;
}

static int guidebot_object_alive(
    const level_metadata_scan_view *view,
    int object)
{
	return object >= 0 && view->object_count &&
	       object < view->object_count(view->user) && view->object_flags &&
	       (view->object_flags(view->user, object) &
	        view->obj_flag_should_be_dead) == 0;
}

static int guidebot_key_powerup(const level_metadata_scan_view *view, int key)
{
	switch (key) {
		case 0: return view->powerup_key_blue;
		case 1: return view->powerup_key_red;
		case 2: return view->powerup_key_gold;
		default: return -1;
	}
}

static int guidebot_resolve_key_object(
    const level_metadata_scan_view *view,
    level_metadata_route_step *step)
{
	int object;
	int carrier = -1;
	int direct = -1;
	int count;
	int powerup;

	if (!view->object_count || !view->object_type || !view->object_id ||
	    !view->object_segment)
		return 0;
	powerup = guidebot_key_powerup(view, step->key_index);
	if (powerup < 0)
		return 0;
	count = view->object_count(view->user);
	for (object = 0; object < count; ++object) {
		if (!guidebot_object_alive(view, object))
			continue;
		if (view->object_type(view->user, object) == view->obj_type_powerup &&
		    view->object_id(view->user, object) == powerup) {
			direct = object;
			break;
		}
		if (carrier < 0 && view->object_contains_count &&
		    view->object_contains_type && view->object_contains_id &&
		    view->object_contains_count(view->user, object) > 0 &&
		    view->object_contains_type(view->user, object) ==
		        view->obj_type_powerup &&
		    view->object_contains_id(view->user, object) == powerup)
			carrier = object;
	}
	object = direct >= 0 ? direct : carrier;
	if (object < 0)
		return 0;
	step->key_carrier_objnum = object;
	step->seg = view->object_segment(view->user, object);
	if (view->object_position)
		step->label_pos_valid = view->object_position(
		    view->user, object, step->label_pos);
	return guidebot_valid_segment(view, step->seg);
}

static int guidebot_resolve_primary_object(
    const level_metadata_scan_view *view,
    level_metadata_route_step *step)
{
	int object;
	int count;

	if (!view->object_count || !view->object_type || !view->object_segment)
		return 0;
	count = view->object_count(view->user);
	for (object = 0; object < count; ++object) {
		int matches;

		if (!guidebot_object_alive(view, object))
			continue;
		matches = step->kind == LEVEL_METADATA_ROUTE_REACTOR
		              ? view->object_type(view->user, object) ==
		                    view->obj_type_control_center
		              : step->kind == LEVEL_METADATA_ROUTE_BOSS &&
		                    view->object_is_boss &&
		                    view->object_is_boss(view->user, object);
		if (!matches)
			continue;
		step->key_carrier_objnum = object;
		step->seg = view->object_segment(view->user, object);
		if (view->object_position)
			step->label_pos_valid = view->object_position(
			    view->user, object, step->label_pos);
		return guidebot_valid_segment(view, step->seg);
	}
	return 0;
}

static int guidebot_step_target_segment(
    const level_metadata_scan_view *view,
    level_metadata_route_step *step)
{
	if (step->kind == LEVEL_METADATA_ROUTE_KEY)
		guidebot_resolve_key_object(view, step);
	if ((step->kind == LEVEL_METADATA_ROUTE_REACTOR ||
	     step->kind == LEVEL_METADATA_ROUTE_BOSS))
		guidebot_resolve_primary_object(view, step);
	if (guidebot_valid_segment(view, step->path_terminal_segment))
		return step->path_terminal_segment;
	return guidebot_valid_segment(view, step->seg) ? step->seg : -1;
}

static int guidebot_step_usable(
    const level_metadata_scan_view *view,
    const level_metadata_route_step *step)
{
	if (step->kind == LEVEL_METADATA_ROUTE_TRIGGER &&
	    (step->trigger_num < 0 || step->trigger_num >= view->num_triggers ||
	     (view->trigger_flags &&
	      (view->trigger_flags(view->user, step->trigger_num) &
	       view->trigger_flag_disabled) != 0)))
		return 0;
	if (step->kind == LEVEL_METADATA_ROUTE_TRIGGER &&
	    step->activation_kind == LEVEL_METADATA_ROUTE_ACTIVATION_SHOOT_SWITCH) {
		if (step->wall_num < 0 || step->wall_num >= view->num_walls ||
		    !view->wall_type ||
		    view->wall_type(view->user, step->wall_num) == view->wall_type_open ||
		    (view->wall_is_shootable_trigger &&
		     !view->wall_is_shootable_trigger(view->user, step->wall_num)) ||
		    (view->wall_trigger && step->trigger_num >= 0 &&
		     view->wall_trigger(view->user, step->wall_num) != step->trigger_num))
			return 0;
	}
	return 1;
}

static int guidebot_trigger_is_spent(
    const level_metadata_scan_view *view,
    int trigger)
{
	int flags;

	if (trigger < 0 || trigger >= view->num_triggers)
		return 1;
	flags = view->trigger_flags
	            ? view->trigger_flags(view->user, trigger)
	            : 0;
	return (view->trigger_flag_disabled != 0 &&
	        (flags & view->trigger_flag_disabled) != 0) ||
	       (view->trigger_flag_one_shot != 0 &&
	        (flags & view->trigger_flag_one_shot) != 0 &&
	        view->trigger_was_activated &&
	        view->trigger_was_activated(view->user, trigger));
}

static int guidebot_prepare_switch_restorer(
    const level_metadata_scan_view *view,
    level_metadata_route_step *step,
    guidebot_route_certifier_summary *summary)
{
	int source_wall;
	int trigger;

	if (!view || !step || !summary ||
	    step->kind != LEVEL_METADATA_ROUTE_TRIGGER ||
	    step->activation_kind != LEVEL_METADATA_ROUTE_ACTIVATION_SHOOT_SWITCH ||
	    step->wall_num < 0 || step->wall_num >= view->num_walls ||
	    !view->trigger_type || !view->trigger_link_count ||
	    !view->trigger_link_segment || !view->trigger_link_side ||
	    !view->wall_num || !view->wall_trigger || !view->wall_segment ||
	    !view->wall_side)
		return 0;
	for (trigger = 0; trigger < view->num_triggers; ++trigger) {
		const int type = view->trigger_type(view->user, trigger);
		int restores_target = 0;
		int link;

		if ((type != view->trigger_type_close_wall &&
		     type != view->trigger_type_close_door &&
		     type != view->trigger_type_illusion_on) ||
		    guidebot_trigger_is_spent(view, trigger))
			continue;
		for (link = 0;
		     link < view->trigger_link_count(view->user, trigger) &&
		     link < LEVEL_METADATA_MAX_ROUTE_LINKS;
		     ++link) {
			const int segment = view->trigger_link_segment(
			    view->user, trigger, link);
			const int side = view->trigger_link_side(
			    view->user, trigger, link);

			if (guidebot_valid_segment(view, segment) && side >= 0 &&
			    side < LEVEL_METADATA_MAX_SIDES &&
			    view->wall_num(view->user, segment, side) == step->wall_num) {
				restores_target = 1;
				break;
			}
		}
		if (!restores_target)
			continue;
		for (source_wall = 0; source_wall < view->num_walls; ++source_wall) {
			level_metadata_route_step recovery;
			const int segment = view->wall_segment(view->user, source_wall);
			const int side = view->wall_side(view->user, source_wall);
			int position[3];

			if (view->wall_trigger(view->user, source_wall) != trigger ||
			    !guidebot_valid_segment(view, segment) || side < 0 ||
			    side >= LEVEL_METADATA_MAX_SIDES)
				continue;
			memset(&recovery, 0, sizeof(recovery));
			recovery.kind = LEVEL_METADATA_ROUTE_TRIGGER;
			recovery.seg = segment;
			recovery.side = side;
			recovery.wall_num = source_wall;
			recovery.trigger_num = trigger;
			recovery.trigger_type = type;
			recovery.key_index = -1;
			recovery.key_carrier_objnum = -1;
			recovery.path_segment_count = 1;
			recovery.path_terminal_segment = segment;
			if (view->wall_is_shootable_trigger &&
			    view->wall_is_shootable_trigger(view->user, source_wall))
				recovery.activation_kind =
				    LEVEL_METADATA_ROUTE_ACTIVATION_SHOOT_SWITCH;
			else if ((view->wall_type &&
			          view->wall_type(view->user, source_wall) ==
			              view->wall_type_open) ||
			         (view->side_is_flyable &&
			          view->side_is_flyable(view->user, segment, side)))
				recovery.activation_kind =
				    LEVEL_METADATA_ROUTE_ACTIVATION_FLY_THROUGH_TRIGGER;
			else
				recovery.activation_kind =
				    LEVEL_METADATA_ROUTE_ACTIVATION_PASS_THROUGH_TRIGGER;
			if (view->segment_center &&
			    view->segment_center(view->user, segment, position)) {
				recovery.activation_pos_valid = 1;
				memcpy(recovery.activation_pos, position, sizeof(position));
			}
			if (view->side_center &&
			    view->side_center(view->user, segment, side, position)) {
				recovery.aim_pos_valid = 1;
				recovery.label_pos_valid = 1;
				memcpy(recovery.aim_pos, position, sizeof(position));
				memcpy(recovery.label_pos, position, sizeof(position));
			}
			for (link = 0;
			     link < view->trigger_link_count(view->user, trigger) &&
			     link < LEVEL_METADATA_MAX_ROUTE_LINKS;
			     ++link) {
				const int target_segment = view->trigger_link_segment(
				    view->user, trigger, link);
				const int target_side = view->trigger_link_side(
				    view->user, trigger, link);

				recovery.opened_link_seg[link] = target_segment;
				recovery.opened_link_side[link] = target_side;
				recovery.opened_link_wall[link] =
				    guidebot_valid_segment(view, target_segment) &&
				            target_side >= 0 &&
				            target_side < LEVEL_METADATA_MAX_SIDES
				        ? view->wall_num(
				              view->user, target_segment, target_side)
				        : -1;
				recovery.opened_link_count++;
			}
			if (recovery.activation_kind ==
			    LEVEL_METADATA_ROUTE_ACTIVATION_SHOOT_SWITCH)
				snprintf(
				    recovery.label, sizeof(recovery.label),
				    "Shoot switch trigger %d", trigger);
			else if (recovery.activation_kind ==
			         LEVEL_METADATA_ROUTE_ACTIVATION_FLY_THROUGH_TRIGGER)
				snprintf(
				    recovery.label, sizeof(recovery.label),
				    "Fly-through trigger %d", trigger);
			else
				snprintf(
				    recovery.label, sizeof(recovery.label),
				    "Pass through trigger %d", trigger);
			*step = recovery;
			summary->used_prepared_fallback = 1;
			return 1;
		}
	}
	return 0;
}

static int guidebot_compiled_switch_fallback(
    const level_metadata_scan_view *view,
    level_metadata_route_step *step,
    guidebot_route_certifier_summary *summary)
{
	int position[3];

	if (step->activation_pos_valid &&
	    (!step->aim_pos_valid ||
	     memcmp(step->activation_pos, step->aim_pos, sizeof(step->activation_pos)) != 0))
		return 1;
	if (!view->segment_center || !guidebot_valid_segment(view, step->seg) ||
	    !view->segment_center(view->user, step->seg, position) ||
	    (step->aim_pos_valid &&
	     memcmp(position, step->aim_pos, sizeof(position)) == 0))
		return 0;
	step->activation_pos_valid = 1;
	memcpy(step->activation_pos, position, sizeof(step->activation_pos));
	step->path_terminal_segment = step->seg;
	if (step->path_segment_count <= 0)
		step->path_segment_count = 1;
	step->switch_shot_quality = LEVEL_METADATA_SWITCH_SHOT_APPROXIMATE;
	step->switch_shot_incidence_cosine = LEVEL_METADATA_SHOT_COSINE_ONE;
	summary->approximate_firing_position = 1;
	return 1;
}

static int guidebot_select_compiled_switch_guidance(
    const level_metadata_scan_view *view,
    level_metadata_route_step *step,
    guidebot_route_certifier_summary *summary)
{
	int distance[LEVEL_METADATA_MAX_SEGMENTS];
	int queue[LEVEL_METADATA_MAX_SEGMENTS];
	long long best_score = 0;
	int best = -1;
	int head = 0;
	int tail = 0;
	int segment;

	if (step->activation_kind !=
	    LEVEL_METADATA_ROUTE_ACTIVATION_SHOOT_SWITCH)
		return 1;
	if (step->switch_guidance_candidate_count <= 0 || !view->segment_child ||
	    !guidebot_valid_segment(view, view->start_segment))
		return guidebot_compiled_switch_fallback(view, step, summary);
	for (segment = 0; segment < view->num_segments; ++segment)
		distance[segment] = -1;
	distance[view->start_segment] = 0;
	queue[tail++] = view->start_segment;
	while (head < tail) {
		int side;

		segment = queue[head++];
		summary->visited_segments++;
		for (side = 0; side < LEVEL_METADATA_MAX_SIDES; ++side) {
			const int child =
			    view->segment_child(view->user, segment, side);

			summary->evaluated_edges++;
			if (!guidebot_valid_segment(view, child) ||
			    distance[child] >= 0 ||
			    !guidebot_route_side_passable_current(view, segment, side))
				continue;
			distance[child] = distance[segment] + 1;
			queue[tail++] = child;
		}
	}
	for (segment = 0;
	     segment < step->switch_guidance_candidate_count &&
	     segment < LEVEL_METADATA_MAX_SWITCH_GUIDANCE_CANDIDATES;
	     ++segment) {
		const int candidate_segment =
		    step->switch_guidance_candidate_seg[segment];
		const int quality =
		    step->switch_guidance_candidate_quality[segment];
		const int incidence =
		    step->switch_guidance_candidate_incidence[segment];
		long long score;

		if (!guidebot_valid_segment(view, candidate_segment) ||
		    distance[candidate_segment] < 0 ||
		    (step->aim_pos_valid &&
		     memcmp(
		         step->switch_guidance_candidate_pos[segment], step->aim_pos,
		         sizeof(step->aim_pos)) == 0))
			continue;
		score = (long long) distance[candidate_segment] *
		            LEVEL_METADATA_SHOT_COSINE_ONE +
		        4LL * (LEVEL_METADATA_SHOT_COSINE_ONE - incidence);
		if (quality == LEVEL_METADATA_SWITCH_SHOT_APPROXIMATE)
			score += 1LL << 40;
		if (best >= 0 && score >= best_score)
			continue;
		best = segment;
		best_score = score;
	}
	if (best < 0)
		return guidebot_compiled_switch_fallback(view, step, summary);
	step->seg = step->switch_guidance_candidate_seg[best];
	step->path_terminal_segment = step->seg;
	step->path_segment_count = distance[step->seg] + 1;
	step->activation_pos_valid = 1;
	memcpy(
	    step->activation_pos, step->switch_guidance_candidate_pos[best],
	    sizeof(step->activation_pos));
	step->switch_shot_quality =
	    step->switch_guidance_candidate_quality[best];
	step->switch_shot_incidence_cosine =
	    step->switch_guidance_candidate_incidence[best];
	summary->reranked_firing_position = best != 0;
	return 1;
}

int guidebot_route_select_compiled_current_state(
    const level_metadata_scan_view *view,
    const level_metadata_state *compiled_state,
    const route_planner_plan_summary *compiled_plan,
    level_metadata_state *live_state,
    route_planner_plan_summary *live_plan,
    guidebot_route_validity_certificate *certificate,
    guidebot_route_certifier_summary *summary)
{
	guidebot_route_certifier_summary local_summary;
	int selected = -1;
	int selected_segment = -1;
	int step;

	if (!view || !compiled_state || !compiled_plan || !live_state ||
	    !live_plan || !certificate || compiled_state->route_step_count < 0 ||
	    compiled_state->route_step_count > LEVEL_METADATA_MAX_ROUTE_STEPS)
		return GUIDEBOT_ROUTE_CERTIFIER_INVALID;
	memset(&local_summary, 0, sizeof(local_summary));
	local_summary.selected_step = -1;
	local_summary.selected_segment = -1;
	local_summary.blocking_step = -1;
	local_summary.blocking_segment = -1;
	*live_state = *compiled_state;
	*live_plan = *compiled_plan;
	live_plan->first_pending_step = -1;
	live_plan->first_pending_path_segment_count = 0;
	live_plan->first_pending_path_terminal_segment = -1;
	live_plan->partial_frontier_segment = -1;
	memset(certificate, 0, sizeof(*certificate));
	certificate->status = GUIDEBOT_ROUTE_CERTIFICATE_INVALID;
	certificate->source_trigger = -1;
	certificate->source_wall = -1;
	certificate->source_object = -1;
	certificate->frontier_segment = -1;

	for (step = 0; step < live_state->route_step_count; ++step) {
		level_metadata_route_step *candidate = &live_state->route_steps[step];

		if (candidate->kind == LEVEL_METADATA_ROUTE_START ||
		    !level_metadata_route_step_required_by_world_state(view, candidate))
			continue;
		if (step < 64)
			local_summary.required_steps_low |= 1ULL << step;
		local_summary.evaluated_actions++;
		if (!guidebot_step_usable(view, candidate)) {
			if (!guidebot_prepare_switch_restorer(
			        view, candidate, &local_summary)) {
				local_summary.rejected_actions++;
				local_summary.blocking_step = step;
				local_summary.blocking_reason =
				    GUIDEBOT_ROUTE_CERTIFIER_REJECTION_INVALID_TARGET;
				break;
			}
		}
		if (!guidebot_select_compiled_switch_guidance(
		        view, candidate, &local_summary)) {
			local_summary.rejected_actions++;
			local_summary.blocking_step = step;
			local_summary.blocking_reason =
			    GUIDEBOT_ROUTE_CERTIFIER_REJECTION_INVALID_TARGET;
			break;
		}
		selected_segment = guidebot_step_target_segment(view, candidate);
		if ((candidate->kind == LEVEL_METADATA_ROUTE_KEY ||
		     candidate->kind == LEVEL_METADATA_ROUTE_REACTOR ||
		     candidate->kind == LEVEL_METADATA_ROUTE_BOSS) &&
		    guidebot_valid_segment(view, candidate->seg))
			selected_segment = candidate->seg;
		if (!guidebot_valid_segment(view, selected_segment)) {
			local_summary.rejected_actions++;
			local_summary.blocking_step = step;
			local_summary.blocking_reason =
			    GUIDEBOT_ROUTE_CERTIFIER_REJECTION_INVALID_TARGET;
			break;
		}
		selected = step;
		break;
	}
	if (selected < 0 && local_summary.blocking_step >= 0) {
		if (summary)
			*summary = local_summary;
		return GUIDEBOT_ROUTE_CERTIFIER_INVALID;
	}
	if (selected >= 0) {
		const level_metadata_route_step *pending =
		    &live_state->route_steps[selected];

		live_plan->first_pending_step = selected;
		live_plan->first_pending_path_segment_count =
		    pending->path_segment_count > 0 ? pending->path_segment_count : 1;
		live_plan->first_pending_path_terminal_segment = selected_segment;
		certificate->source_trigger = pending->trigger_num;
		certificate->source_wall = pending->wall_num;
		certificate->source_object = pending->key_carrier_objnum;
		certificate->frontier_segment = selected_segment;
	} else
		live_state->route_status = LEVEL_METADATA_ROUTE_OK;
	certificate->status = GUIDEBOT_ROUTE_CERTIFICATE_VALID;
	local_summary.selected_step = selected;
	local_summary.selected_segment = selected_segment;
	if (summary)
		*summary = local_summary;
	return GUIDEBOT_ROUTE_CERTIFIER_VALID;
}

static long double guidebot_position_distance_squared(
    const int left[3],
    const int right[3])
{
	long double result = 0.0;
	int coordinate;

	for (coordinate = 0; coordinate < 3; ++coordinate) {
		const long double delta =
		    (long double) left[coordinate] - right[coordinate];
		result += delta * delta;
	}
	return result;
}

#define GUIDEBOT_STEEP_SHOT_COSINE 16962
#define GUIDEBOT_MIN_SHOT_COSINE   655

static int guidebot_shot_incidence_cosine(
    const level_metadata_scan_view *view,
    const int firing_pos[3],
    int wall_num)
{
	int incidence = LEVEL_METADATA_SHOT_COSINE_ONE;

	if (view->wall_shot_incidence_cosine)
		incidence = view->wall_shot_incidence_cosine(
		    view->user, firing_pos, wall_num);
	if (incidence < 0)
		return 0;
	return incidence > LEVEL_METADATA_SHOT_COSINE_ONE
	           ? LEVEL_METADATA_SHOT_COSINE_ONE
	           : incidence;
}

static long double guidebot_shot_score(
    long double distance_squared, int incidence)
{
	long double multiplier;

	if (incidence >= GUIDEBOT_STEEP_SHOT_COSINE)
		return distance_squared;
	if (incidence < GUIDEBOT_MIN_SHOT_COSINE)
		incidence = GUIDEBOT_MIN_SHOT_COSINE;
	multiplier =
	    (long double) GUIDEBOT_STEEP_SHOT_COSINE / incidence;
	return distance_squared * multiplier * multiplier;
}

static void guidebot_weighted_position(
    const int first[3], const int second[3], int second_weight, int result[3])
{
	const int total = 1 + second_weight;
	int coordinate;

	for (coordinate = 0; coordinate < 3; ++coordinate)
		result[coordinate] = (int) (((long long) first[coordinate] +
		                             (long long) second[coordinate] *
		                                 second_weight) /
		                            total);
}

static int guidebot_detailed_firing_sample(
    const level_metadata_scan_view *view,
    int segment,
    int sample,
    int result[3])
{
	static const unsigned char edges[12][2] = {
		{ 7, 6 }, { 6, 2 }, { 2, 3 }, { 3, 7 }, { 0, 4 }, { 4, 7 }, { 3, 0 }, { 0, 1 }, { 1, 5 }, { 5, 4 }, { 6, 5 }, { 1, 2 }
	};
	int center[3];
	int point[3];

	if (!view->segment_center ||
	    !view->segment_center(view->user, segment, center))
		return 0;
	if (sample == 0) {
		if (!view->side_center ||
		    !view->side_center(view->user, segment, 0, point))
			return 0;
		memcpy(result, center, sizeof(center));
		return 1;
	}
	sample--;
	if (sample < 12) {
		if (!view->side_center ||
		    !view->side_center(view->user, segment, sample / 2, point))
			return 0;
		guidebot_weighted_position(
		    center, point, sample & 1 ? 3 : 1, result);
		return 1;
	}
	if (sample < 20) {
		if (!view->segment_vertex ||
		    !view->segment_vertex(view->user, segment, sample - 12, point))
			return 0;
		guidebot_weighted_position(center, point, 1, result);
		return 1;
	}
	if (sample < 32) {
		int first[3];
		int second[3];
		int coordinate;
		const int edge = sample - 20;

		if (!view->segment_vertex ||
		    !view->segment_vertex(
		        view->user, segment, edges[edge][0], first) ||
		    !view->segment_vertex(
		        view->user, segment, edges[edge][1], second))
			return 0;
		for (coordinate = 0; coordinate < 3; ++coordinate)
			point[coordinate] = (first[coordinate] + second[coordinate]) / 2;
		guidebot_weighted_position(center, point, 1, result);
		return 1;
	}
	return 0;
}

#define GUIDEBOT_DETAILED_FIRING_SEGMENTS 8

static int guidebot_select_detailed_firing_segments_budgeted(
    const level_metadata_scan_view *view,
    const level_metadata_route_step *step,
    guidebot_route_certifier_workspace *workspace,
    const guidebot_route_certifier_budget *budget,
    unsigned int *tick_work)
{
	const int *target = step->aim_pos_valid ? step->aim_pos : step->activation_pos;

	while (workspace->firing_search_selection_segment < view->num_segments) {
		long double score;
		int center[3];
		int incidence;
		int insert;
		int segment;

		if (guidebot_certifier_budget_exhausted(budget, *tick_work))
			return GUIDEBOT_ROUTE_CERTIFIER_PENDING;
		segment = workspace->firing_search_selection_segment++;
		(*tick_work)++;
		if (!workspace->reachable[segment] || !view->segment_center ||
		    !view->segment_center(view->user, segment, center))
			continue;
		incidence = guidebot_shot_incidence_cosine(
		    view, center, step->wall_num);
		score = guidebot_shot_score(
		    guidebot_position_distance_squared(center, target), incidence);
		for (insert = 0; insert < workspace->firing_search_detailed_count;
		     ++insert)
			if (score < workspace->firing_search_detailed_scores[insert] ||
			    (score == workspace->firing_search_detailed_scores[insert] &&
			     segment < workspace->firing_search_detailed_segments[insert]))
				break;
		if (insert >= GUIDEBOT_DETAILED_FIRING_SEGMENTS)
			continue;
		if (workspace->firing_search_detailed_count <
		    GUIDEBOT_DETAILED_FIRING_SEGMENTS)
			workspace->firing_search_detailed_count++;
		{
			int move;

			for (move = workspace->firing_search_detailed_count - 1;
			     move > insert; --move) {
				workspace->firing_search_detailed_segments[move] =
				    workspace->firing_search_detailed_segments[move - 1];
				workspace->firing_search_detailed_scores[move] =
				    workspace->firing_search_detailed_scores[move - 1];
			}
		}
		workspace->firing_search_detailed_segments[insert] = segment;
		workspace->firing_search_detailed_scores[insert] = score;
	}
	workspace->firing_search_selection_complete = 1;
	workspace->firing_search_detailed_index = 0;
	workspace->firing_search_detailed_sample = 0;
	return GUIDEBOT_ROUTE_CERTIFIER_VALID;
}

#define GUIDEBOT_FIRING_FRONTIER_INITIALIZE 0
#define GUIDEBOT_FIRING_FRONTIER_STRATEGIC  1
#define GUIDEBOT_FIRING_FRONTIER_PROGRESS   2
#define GUIDEBOT_FIRING_FRONTIER_ANGLE      3
#define GUIDEBOT_FIRING_FRONTIER_COMPLETE   4

static int guidebot_firing_frontier_budgeted(
    const level_metadata_scan_view *view,
    int goal_segment,
    int wall_num,
    guidebot_route_certifier_workspace *workspace,
    const guidebot_route_certifier_budget *budget,
    unsigned int *tick_work)
{
	if (!view || !workspace || !tick_work || !view->segment_child ||
	    !view->reverse_side ||
	    !guidebot_valid_segment(view, workspace->job_start_segment) ||
	    !guidebot_valid_segment(view, goal_segment))
		return GUIDEBOT_ROUTE_CERTIFIER_INVALID;
	if (!workspace->firing_frontier_active ||
	    workspace->firing_frontier_goal_segment != goal_segment) {
		workspace->firing_frontier_active = 1;
		workspace->firing_frontier_phase =
		    GUIDEBOT_FIRING_FRONTIER_INITIALIZE;
		workspace->firing_frontier_goal_segment = goal_segment;
		workspace->firing_frontier_init_segment = 0;
		workspace->firing_frontier_best_segment = -1;
	}
	while (workspace->firing_frontier_phase !=
	       GUIDEBOT_FIRING_FRONTIER_COMPLETE) {
		if (workspace->firing_frontier_phase ==
		    GUIDEBOT_FIRING_FRONTIER_INITIALIZE) {
			while (workspace->firing_frontier_init_segment <
			       view->num_segments) {
				if (guidebot_certifier_budget_exhausted(
				        budget, *tick_work))
					return GUIDEBOT_ROUTE_CERTIFIER_PENDING;
				workspace->strategic_distance
				    [workspace->firing_frontier_init_segment++] = -1;
				(*tick_work)++;
			}
			workspace->strategic_distance[goal_segment] = 0;
			workspace->queue[0] = goal_segment;
			workspace->firing_frontier_head = 0;
			workspace->firing_frontier_tail = 1;
			workspace->firing_frontier_side = 0;
			workspace->firing_frontier_phase =
			    GUIDEBOT_FIRING_FRONTIER_STRATEGIC;
		}
		if (workspace->firing_frontier_phase ==
		    GUIDEBOT_FIRING_FRONTIER_STRATEGIC) {
			while (workspace->firing_frontier_head <
			       workspace->firing_frontier_tail) {
				const int segment = workspace->queue
				                        [workspace->firing_frontier_head];

				while (workspace->firing_frontier_side <
				       LEVEL_METADATA_MAX_SIDES) {
					int child;
					int reverse;
					int side;

					if (guidebot_certifier_budget_exhausted(
					        budget, *tick_work))
						return GUIDEBOT_ROUTE_CERTIFIER_PENDING;
					side = workspace->firing_frontier_side++;
					(*tick_work)++;
					child = view->segment_child(
					    view->user, segment, side);
					if (!guidebot_valid_segment(view, child) ||
					    workspace->strategic_distance[child] >= 0)
						continue;
					reverse = view->reverse_side(
					    view->user, segment, child);
					if (reverse < 0 ||
					    reverse >= LEVEL_METADATA_MAX_SIDES ||
					    !guidebot_route_side_passable(
					        view, child, reverse, 1, 0, 1))
						continue;
					workspace->strategic_distance[child] =
					    workspace->strategic_distance[segment] + 1;
					workspace->queue
					    [workspace->firing_frontier_tail++] = child;
				}
				workspace->firing_frontier_side = 0;
				workspace->firing_frontier_head++;
			}
			if (workspace->strategic_distance
			        [workspace->job_start_segment] < 0) {
				workspace->firing_frontier_phase =
				    GUIDEBOT_FIRING_FRONTIER_COMPLETE;
				continue;
			}
			workspace->firing_frontier_scan_segment = 0;
			workspace->firing_frontier_best_remaining =
			    view->num_segments + 1;
			workspace->firing_frontier_phase =
			    GUIDEBOT_FIRING_FRONTIER_PROGRESS;
		}
		if (workspace->firing_frontier_phase ==
		    GUIDEBOT_FIRING_FRONTIER_PROGRESS) {
			while (workspace->firing_frontier_scan_segment <
			       view->num_segments) {
				int segment;

				if (guidebot_certifier_budget_exhausted(
				        budget, *tick_work))
					return GUIDEBOT_ROUTE_CERTIFIER_PENDING;
				segment = workspace->firing_frontier_scan_segment++;
				(*tick_work)++;
				if (workspace->physical_distance[segment] < 0 ||
				    workspace->strategic_distance[segment] < 0 ||
				    workspace->strategic_distance[segment] >=
				        workspace->firing_frontier_best_remaining)
					continue;
				workspace->firing_frontier_best_segment = segment;
				workspace->firing_frontier_best_remaining =
				    workspace->strategic_distance[segment];
			}
			workspace->firing_frontier_scan_segment = 0;
			workspace->firing_frontier_best_incidence = -1;
			workspace->firing_frontier_phase =
			    GUIDEBOT_FIRING_FRONTIER_ANGLE;
		}
		if (workspace->firing_frontier_phase ==
		    GUIDEBOT_FIRING_FRONTIER_ANGLE) {
			while (workspace->firing_frontier_scan_segment <
			       view->num_segments) {
				int candidate_pos[3];
				int incidence;
				int segment;

				if (guidebot_certifier_budget_exhausted(
				        budget, *tick_work))
					return GUIDEBOT_ROUTE_CERTIFIER_PENDING;
				segment = workspace->firing_frontier_scan_segment++;
				(*tick_work)++;
				if (workspace->physical_distance[segment] < 0 ||
				    workspace->strategic_distance[segment] <
				        workspace->firing_frontier_best_remaining ||
				    workspace->strategic_distance[segment] >
				        workspace->firing_frontier_best_remaining + 2 ||
				    !view->segment_center ||
				    !view->segment_center(
				        view->user, segment, candidate_pos))
					continue;
				incidence = guidebot_shot_incidence_cosine(
				    view, candidate_pos, wall_num);
				if (workspace->firing_frontier_best_incidence >= 0 &&
				    (incidence <
				         workspace->firing_frontier_best_incidence ||
				     (incidence ==
				          workspace->firing_frontier_best_incidence &&
				      (workspace->strategic_distance[segment] >
				           workspace->strategic_distance
				               [workspace->firing_frontier_best_segment] ||
				       (workspace->strategic_distance[segment] ==
				            workspace->strategic_distance
				                [workspace->firing_frontier_best_segment] &&
				        workspace->physical_distance[segment] >=
				            workspace->physical_distance
				                [workspace->firing_frontier_best_segment])))))
					continue;
				workspace->firing_frontier_best_segment = segment;
				workspace->firing_frontier_best_incidence = incidence;
			}
			workspace->firing_frontier_phase =
			    GUIDEBOT_FIRING_FRONTIER_COMPLETE;
		}
	}
	return GUIDEBOT_ROUTE_CERTIFIER_VALID;
}

static int guidebot_firing_cache_matches(
    const level_metadata_scan_view *view,
    const level_metadata_route_step *step,
    const guidebot_route_certifier_workspace *workspace)
{
	return workspace->firing_cache_valid && step->aim_pos_valid &&
	       memcmp(
	           workspace->firing_cache_position, step->aim_pos,
	           sizeof(workspace->firing_cache_position)) != 0 &&
	       workspace->firing_cache_num_segments == view->num_segments &&
	       workspace->firing_cache_num_walls == view->num_walls &&
	       workspace->firing_cache_trigger == step->trigger_num &&
	       workspace->firing_cache_wall == step->wall_num &&
	       memcmp(
	           workspace->firing_cache_aim, step->aim_pos,
	           sizeof(workspace->firing_cache_aim)) == 0;
}

static int guidebot_prepare_shoot_switch_position(
    const level_metadata_scan_view *view,
    level_metadata_route_step *step,
    guidebot_route_certifier_workspace *workspace,
    guidebot_route_certifier_summary *summary,
    const guidebot_route_certifier_budget *budget,
    unsigned int *tick_work)
{
	int firing_pos[3];
	int original_segment;

	if (step->activation_kind !=
	    LEVEL_METADATA_ROUTE_ACTIVATION_SHOOT_SWITCH)
		return GUIDEBOT_ROUTE_CERTIFIER_VALID;
	if (!step->activation_pos_valid || step->wall_num < 0 ||
	    !view->wall_shootable_from_position)
		return GUIDEBOT_ROUTE_CERTIFIER_INVALID;
	memcpy(firing_pos, step->activation_pos, sizeof(firing_pos));
	if (step->aim_pos_valid &&
	    memcmp(firing_pos, step->aim_pos, sizeof(firing_pos)) == 0) {
		if (!view->segment_center ||
		    !view->segment_center(view->user, step->seg, firing_pos))
			return GUIDEBOT_ROUTE_CERTIFIER_INVALID;
	}
	original_segment = step->seg;
	if (guidebot_firing_cache_matches(view, step, workspace) &&
	    guidebot_valid_segment(view, workspace->firing_cache_segment) &&
	    workspace->reachable[workspace->firing_cache_segment] &&
	    ((workspace->firing_cache_shot_quality !=
	          LEVEL_METADATA_SWITCH_SHOT_APPROXIMATE &&
	      view->wall_shootable_from_position(
	          view->user, workspace->firing_cache_segment,
	          workspace->firing_cache_position, step->wall_num)) ||
	     (workspace->firing_cache_shot_quality ==
	          LEVEL_METADATA_SWITCH_SHOT_APPROXIMATE &&
	      view->wall_potentially_shootable_from_position &&
	      view->wall_potentially_shootable_from_position(
	          view->user, workspace->firing_cache_segment,
	          workspace->firing_cache_position, step->wall_num)))) {
		step->seg = workspace->firing_cache_segment;
		step->path_terminal_segment = workspace->firing_cache_segment;
		step->path_segment_count =
		    workspace->firing_cache_path_segment_count;
		memcpy(
		    step->activation_pos, workspace->firing_cache_position,
		    sizeof(step->activation_pos));
		step->switch_shot_quality = workspace->firing_cache_shot_quality;
		step->switch_shot_incidence_cosine =
		    workspace->firing_cache_incidence_cosine;
		summary->firing_cache_hit = 1;
		summary->approximate_firing_position =
		    step->switch_shot_quality ==
		    LEVEL_METADATA_SWITCH_SHOT_APPROXIMATE;
		summary->steep_firing_position =
		    step->switch_shot_quality ==
		    LEVEL_METADATA_SWITCH_SHOT_CONFIRMED_STEEP;
		workspace->firing_search_active = 0;
		return GUIDEBOT_ROUTE_CERTIFIER_VALID;
	}
	if (!workspace->firing_search_active) {
		workspace->firing_search_active = 1;
		workspace->firing_search_pass = 0;
		workspace->firing_search_segment = -1;
		workspace->firing_search_detailed_count = 0;
		workspace->firing_search_selection_segment = 0;
		workspace->firing_search_selection_complete = 0;
		workspace->firing_frontier_active = 0;
		workspace->firing_search_original_segment = original_segment;
		memcpy(
		    workspace->firing_search_original_position, firing_pos,
		    sizeof(workspace->firing_search_original_position));
		workspace->firing_search_best_segment = -1;
		workspace->firing_search_best_path_distance = -1;
		workspace->firing_search_best_quality =
		    LEVEL_METADATA_SWITCH_SHOT_NONE;
		workspace->firing_search_best_incidence =
		    LEVEL_METADATA_SHOT_COSINE_ONE;
		workspace->firing_search_best_score = 0.0;
	}
	if (!workspace->firing_search_selection_complete) {
		const int selection =
		    guidebot_select_detailed_firing_segments_budgeted(
		        view, step, workspace, budget, tick_work);

		if (selection == GUIDEBOT_ROUTE_CERTIFIER_PENDING)
			return selection;
	}
	while (workspace->firing_search_pass < 4) {
		const int pass = workspace->firing_search_pass;
		int (*shootable)(void *, int, const int[3], int) =
		    !(pass & 1) ? view->wall_shootable_from_position
		                : view->wall_potentially_shootable_from_position;
		const int quality = !(pass & 1)
		                        ? LEVEL_METADATA_SWITCH_SHOT_CONFIRMED
		                        : LEVEL_METADATA_SWITCH_SHOT_APPROXIMATE;
		const int detailed_pass = pass < 2;

		/* Cached metadata can call a shot confirmed even after a live geometry
		 * check rejects it.  In that case still run the conservative potential
		 * visibility pass and publish it as approximate guidance. */
		if (!shootable) {
			if (detailed_pass)
				workspace->firing_search_detailed_index =
				    workspace->firing_search_detailed_count;
			else
				workspace->firing_search_segment = view->num_segments;
		}
		/* Search detailed poses in the nearest reachable cells before asking the
		 * collision system about every cell center in the mine.  A square-enough
		 * nearby shot is already useful guidance and avoids the broad scan. */
		while (detailed_pass &&
		       workspace->firing_search_detailed_index <
		           workspace->firing_search_detailed_count) {
			long double distance_squared;
			long double score;
			int candidate_pos[3];
			int incidence;
			const int candidate_segment =
			    workspace->firing_search_detailed_segments
			        [workspace->firing_search_detailed_index];
			int sample;

			if (workspace->firing_search_detailed_sample >= 33) {
				workspace->firing_search_detailed_index++;
				workspace->firing_search_detailed_sample = 0;
				continue;
			}
			if (guidebot_certifier_budget_exhausted(budget, *tick_work))
				return GUIDEBOT_ROUTE_CERTIFIER_PENDING;
			sample = workspace->firing_search_detailed_sample++;
			(*tick_work)++;
			if (!guidebot_detailed_firing_sample(
			        view, candidate_segment, sample, candidate_pos))
				continue;
			if (step->aim_pos_valid &&
			    memcmp(candidate_pos, step->aim_pos, sizeof(candidate_pos)) == 0)
				continue;
			workspace->job_evaluated_firing_positions++;
			if (!shootable(
			        view->user, candidate_segment, candidate_pos,
			        step->wall_num))
				continue;
			incidence = guidebot_shot_incidence_cosine(
			    view, candidate_pos, step->wall_num);
			distance_squared = step->aim_pos_valid
			                       ? guidebot_position_distance_squared(
			                             candidate_pos, step->aim_pos)
			                       : 0.0;
			score = guidebot_shot_score(distance_squared, incidence);
			if (workspace->firing_search_best_segment >= 0 &&
			    score >= workspace->firing_search_best_score)
				continue;
			workspace->firing_search_best_segment = candidate_segment;
			workspace->firing_search_best_path_distance =
			    workspace->physical_distance[candidate_segment];
			workspace->firing_search_best_score = score;
			workspace->firing_search_best_incidence = incidence;
			workspace->firing_search_best_quality =
			    quality == LEVEL_METADATA_SWITCH_SHOT_CONFIRMED &&
			            incidence < GUIDEBOT_STEEP_SHOT_COSINE
			        ? LEVEL_METADATA_SWITCH_SHOT_CONFIRMED_STEEP
			        : quality;
			memcpy(
			    workspace->firing_search_best_position, candidate_pos,
			    sizeof(workspace->firing_search_best_position));
			if (incidence >= GUIDEBOT_STEEP_SHOT_COSINE) {
				workspace->firing_search_detailed_index =
				    workspace->firing_search_detailed_count;
				break;
			}
		}
		if (detailed_pass &&
		    workspace->firing_search_best_segment >= 0)
			break;
		while (!detailed_pass &&
		       workspace->firing_search_segment < view->num_segments) {
			long double distance_squared;
			long double score;
			int candidate_pos[3];
			int candidate_segment;
			int incidence;
			int segment;

			if (guidebot_certifier_budget_exhausted(budget, *tick_work))
				return GUIDEBOT_ROUTE_CERTIFIER_PENDING;
			segment = workspace->firing_search_segment++;

			if (segment < 0) {
				candidate_segment = original_segment;
				memcpy(candidate_pos, firing_pos, sizeof(candidate_pos));
			} else {
				candidate_segment = segment;
				if (!view->segment_center ||
				    !view->segment_center(
				        view->user, candidate_segment, candidate_pos))
					continue;
			}
			if (!guidebot_valid_segment(view, candidate_segment) ||
			    !workspace->reachable[candidate_segment])
				continue;
			if (step->aim_pos_valid &&
			    memcmp(candidate_pos, step->aim_pos, sizeof(candidate_pos)) == 0)
				continue;
			workspace->job_evaluated_firing_positions++;
			(*tick_work)++;
			if (!shootable(
			        view->user, candidate_segment, candidate_pos,
			        step->wall_num))
				continue;
			incidence = guidebot_shot_incidence_cosine(
			    view, candidate_pos, step->wall_num);
			distance_squared = step->aim_pos_valid
			                       ? guidebot_position_distance_squared(
			                             candidate_pos, step->aim_pos)
			                       : 0.0;
			score = guidebot_shot_score(distance_squared, incidence);
			if (workspace->firing_search_best_segment >= 0 &&
			    (score > workspace->firing_search_best_score ||
			     (score == workspace->firing_search_best_score &&
			      (workspace->physical_distance[candidate_segment] >
			           workspace->firing_search_best_path_distance ||
			       (workspace->physical_distance[candidate_segment] ==
			            workspace->firing_search_best_path_distance &&
			        candidate_segment >=
			            workspace->firing_search_best_segment)))))
				continue;
			workspace->firing_search_best_segment = candidate_segment;
			workspace->firing_search_best_path_distance =
			    workspace->physical_distance[candidate_segment];
			workspace->firing_search_best_score = score;
			workspace->firing_search_best_incidence = incidence;
			workspace->firing_search_best_quality =
			    quality == LEVEL_METADATA_SWITCH_SHOT_CONFIRMED &&
			            incidence < GUIDEBOT_STEEP_SHOT_COSINE
			        ? LEVEL_METADATA_SWITCH_SHOT_CONFIRMED_STEEP
			        : quality;
			memcpy(
			    workspace->firing_search_best_position, candidate_pos,
			    sizeof(workspace->firing_search_best_position));
		}
		if (workspace->firing_search_best_segment >= 0)
			break;
		workspace->firing_search_pass++;
		workspace->firing_search_segment = -1;
		workspace->firing_search_detailed_index = 0;
		workspace->firing_search_detailed_sample = 0;
	}
	if (workspace->firing_search_best_segment < 0) {
		int (*shootable)(void *, int, const int[3], int) =
		    view->wall_shootable_from_position;
		int frontier_result;
		int frontier;

		frontier_result = guidebot_firing_frontier_budgeted(
		    view, original_segment, step->wall_num, workspace, budget,
		    tick_work);
		if (frontier_result == GUIDEBOT_ROUTE_CERTIFIER_PENDING)
			return GUIDEBOT_ROUTE_CERTIFIER_PENDING;
		frontier = frontier_result == GUIDEBOT_ROUTE_CERTIFIER_VALID
		               ? workspace->firing_frontier_best_segment
		               : -1;

		if (guidebot_valid_segment(view, frontier) &&
		    view->segment_center &&
		    view->segment_center(
		        view->user, frontier,
		        workspace->firing_search_best_position) &&
		    (!step->aim_pos_valid ||
		     memcmp(
		         workspace->firing_search_best_position, step->aim_pos,
		         sizeof(workspace->firing_search_best_position)) != 0)) {
			workspace->firing_search_best_segment = frontier;
			workspace->firing_search_best_path_distance =
			    workspace->physical_distance[frontier];
			workspace->firing_search_best_quality =
			    LEVEL_METADATA_SWITCH_SHOT_APPROXIMATE;
			workspace->firing_search_best_incidence =
			    LEVEL_METADATA_SHOT_COSINE_ONE;
		}

		if (workspace->firing_search_best_segment < 0) {
			workspace->firing_search_best_quality =
			    LEVEL_METADATA_SWITCH_SHOT_CONFIRMED;
		}
		if (workspace->firing_search_best_segment < 0 &&
		    step->aim_pos_valid &&
		    memcmp(firing_pos, step->aim_pos, sizeof(firing_pos)) == 0)
			return GUIDEBOT_ROUTE_CERTIFIER_INVALID;
		if (workspace->firing_search_best_segment < 0 &&
		    !shootable(
		        view->user, original_segment, firing_pos, step->wall_num)) {
			if (step->switch_shot_quality !=
			        LEVEL_METADATA_SWITCH_SHOT_APPROXIMATE ||
			    !view->wall_potentially_shootable_from_position)
				return GUIDEBOT_ROUTE_CERTIFIER_INVALID;
			shootable = view->wall_potentially_shootable_from_position;
			if (!shootable(
			        view->user, original_segment, firing_pos,
			        step->wall_num))
				return GUIDEBOT_ROUTE_CERTIFIER_INVALID;
			workspace->firing_search_best_quality =
			    LEVEL_METADATA_SWITCH_SHOT_APPROXIMATE;
		}
		if (workspace->firing_search_best_segment < 0) {
			workspace->firing_search_best_segment = original_segment;
			workspace->firing_search_best_incidence =
			    guidebot_shot_incidence_cosine(
			        view, firing_pos, step->wall_num);
			if (workspace->firing_search_best_quality ==
			        LEVEL_METADATA_SWITCH_SHOT_CONFIRMED &&
			    workspace->firing_search_best_incidence <
			        GUIDEBOT_STEEP_SHOT_COSINE)
				workspace->firing_search_best_quality =
				    LEVEL_METADATA_SWITCH_SHOT_CONFIRMED_STEEP;
			memcpy(
			    workspace->firing_search_best_position, firing_pos,
			    sizeof(workspace->firing_search_best_position));
		}
	}
	step->seg = workspace->firing_search_best_segment;
	step->path_terminal_segment = workspace->firing_search_best_segment;
	step->path_segment_count = workspace->firing_search_best_path_distance >= 0
	                               ? workspace->firing_search_best_path_distance + 1
	                               : step->path_segment_count;
	memcpy(
	    step->activation_pos, workspace->firing_search_best_position,
	    sizeof(step->activation_pos));
	step->switch_shot_quality = workspace->firing_search_best_quality;
	step->switch_shot_incidence_cosine =
	    workspace->firing_search_best_incidence;
	workspace->firing_cache_valid = 1;
	workspace->firing_cache_num_segments = view->num_segments;
	workspace->firing_cache_num_walls = view->num_walls;
	workspace->firing_cache_trigger = step->trigger_num;
	workspace->firing_cache_wall = step->wall_num;
	memcpy(
	    workspace->firing_cache_aim, step->aim_pos,
	    sizeof(workspace->firing_cache_aim));
	workspace->firing_cache_segment = workspace->firing_search_best_segment;
	workspace->firing_cache_path_segment_count = step->path_segment_count;
	memcpy(
	    workspace->firing_cache_position,
	    workspace->firing_search_best_position,
	    sizeof(workspace->firing_cache_position));
	workspace->firing_cache_shot_quality =
	    workspace->firing_search_best_quality;
	workspace->firing_cache_incidence_cosine =
	    workspace->firing_search_best_incidence;
	summary->reranked_firing_position =
	    workspace->firing_search_best_segment != original_segment ||
	    memcmp(
	        workspace->firing_search_best_position, firing_pos,
	        sizeof(workspace->firing_search_best_position)) != 0;
	summary->approximate_firing_position =
	    workspace->firing_search_best_quality ==
	    LEVEL_METADATA_SWITCH_SHOT_APPROXIMATE;
	summary->steep_firing_position =
	    workspace->firing_search_best_quality ==
	    LEVEL_METADATA_SWITCH_SHOT_CONFIRMED_STEEP;
	summary->evaluated_firing_positions =
	    workspace->job_evaluated_firing_positions;
	workspace->firing_search_active = 0;
	return GUIDEBOT_ROUTE_CERTIFIER_VALID;
}

static int guidebot_step_starts_countdown(
    const level_metadata_route_step *step)
{
	return step &&
	       (step->kind == LEVEL_METADATA_ROUTE_REACTOR ||
	        step->kind == LEVEL_METADATA_ROUTE_BOSS ||
	        step->activation_kind ==
	            LEVEL_METADATA_ROUTE_ACTIVATION_DESTROY_REACTOR ||
	        step->activation_kind ==
	            LEVEL_METADATA_ROUTE_ACTIVATION_DESTROY_BOSS);
}

int guidebot_route_select_exit_step_current_state(
    const level_metadata_scan_view *view,
    const level_metadata_state *state,
    level_metadata_route_step *selected_step,
    int *selected_index,
    int *selected_segment)
{
	int step;

	if (selected_index)
		*selected_index = -1;
	if (selected_segment)
		*selected_segment = -1;
	if (!view || !state || !selected_step || state->route_step_count < 0 ||
	    state->route_step_count > LEVEL_METADATA_MAX_ROUTE_STEPS)
		return 0;
	for (step = 0; step < state->route_step_count; ++step) {
		level_metadata_route_step candidate = state->route_steps[step];
		int target_segment;

		if (candidate.kind == LEVEL_METADATA_ROUTE_START ||
		    !level_metadata_route_step_required_by_world_state(
		        view, &candidate))
			continue;
		if (guidebot_step_starts_countdown(&candidate))
			continue;
		if (!guidebot_step_usable(view, &candidate))
			return 0;
		target_segment = guidebot_step_target_segment(view, &candidate);
		if (!guidebot_valid_segment(view, target_segment))
			return 0;
		*selected_step = candidate;
		if (selected_index)
			*selected_index = step;
		if (selected_segment)
			*selected_segment = target_segment;
		return 1;
	}
	return 0;
}

static int guidebot_later_required_target_reachable(
    const level_metadata_scan_view *view,
    level_metadata_state *state,
    const guidebot_route_certifier_workspace *workspace,
    int current_step)
{
	int step;

	for (step = current_step + 1; step < state->route_step_count; ++step) {
		level_metadata_route_step *candidate = &state->route_steps[step];
		int target_segment;

		if (candidate->kind == LEVEL_METADATA_ROUTE_START ||
		    !level_metadata_route_step_required_by_world_state(view, candidate) ||
		    !guidebot_step_usable(view, candidate))
			continue;
		target_segment = guidebot_step_target_segment(view, candidate);
		if (guidebot_valid_segment(view, target_segment) &&
		    workspace->reachable[target_segment])
			return 1;
	}
	return 0;
}

int guidebot_route_certify_current_state_budgeted(
    const level_metadata_scan_view *view,
    const level_metadata_state *prepared_state,
    const route_planner_plan_summary *prepared_plan,
    guidebot_route_certifier_workspace *workspace,
    level_metadata_state *live_state,
    route_planner_plan_summary *live_plan,
    guidebot_route_validity_certificate *certificate,
    guidebot_route_certifier_summary *certifier_summary,
    const guidebot_route_certifier_budget *budget)
{
	guidebot_route_certifier_summary local_summary;
	unsigned int tick_work = 0;
	int selected = -1;
	int selected_segment = -1;
	int requires_control_center = 0;
	int step;

	if (!view || !prepared_state || !prepared_plan || !workspace ||
	    !live_state || !live_plan || !certificate)
		return 0;
	memset(&local_summary, 0, sizeof(local_summary));
	local_summary.selected_step = -1;
	local_summary.selected_segment = -1;
	local_summary.blocking_step = -1;
	local_summary.blocking_segment = -1;
	memset(certificate, 0, sizeof(*certificate));
	certificate->status = GUIDEBOT_ROUTE_CERTIFICATE_INVALID;
	certificate->source_trigger = -1;
	certificate->source_wall = -1;
	certificate->source_object = -1;
	certificate->frontier_segment = -1;
	if (prepared_state->route_step_count < 0 ||
	    prepared_state->route_step_count > LEVEL_METADATA_MAX_ROUTE_STEPS) {
		guidebot_route_certifier_reset_job(workspace);
		return GUIDEBOT_ROUTE_CERTIFIER_INVALID;
	}
	{
		const int reachability = guidebot_build_reachability(
		    view, workspace, &local_summary, budget, &tick_work);

		if (reachability == GUIDEBOT_ROUTE_CERTIFIER_PENDING) {
			local_summary.visited_segments =
			    workspace->job_visited_segments;
			local_summary.evaluated_edges =
			    workspace->job_evaluated_edges;
			if (certifier_summary)
				*certifier_summary = local_summary;
			return GUIDEBOT_ROUTE_CERTIFIER_PENDING;
		}
		if (reachability != GUIDEBOT_ROUTE_CERTIFIER_VALID) {
			guidebot_route_certifier_reset_job(workspace);
			return GUIDEBOT_ROUTE_CERTIFIER_INVALID;
		}
	}
	*live_state = *prepared_state;
	*live_plan = *prepared_plan;
	live_plan->first_pending_step = -1;
	live_plan->first_pending_path_segment_count = 0;
	live_plan->first_pending_path_terminal_segment = -1;
	live_plan->partial_frontier_segment = -1;
	for (step = 0; step < live_state->route_step_count; ++step) {
		if (live_state->route_steps[step].kind == LEVEL_METADATA_ROUTE_REACTOR ||
		    live_state->route_steps[step].kind == LEVEL_METADATA_ROUTE_BOSS)
			requires_control_center = 1;
		if (step < 64 &&
		    level_metadata_route_step_required_by_world_state(
		        view, &live_state->route_steps[step]))
			local_summary.required_steps_low |= 1ULL << step;
	}
	for (step = 0; step < live_state->route_step_count; ++step) {
		level_metadata_route_step *candidate = &live_state->route_steps[step];
		int target_segment;

		if (candidate->kind == LEVEL_METADATA_ROUTE_START)
			continue;
		if (!level_metadata_route_step_required_by_world_state(view, candidate))
			continue;
		local_summary.evaluated_actions++;
		/* An auto-closing hidden door behind the current start must not pull the
		 * route backward when a later required objective is already reachable. */
		if (candidate->kind == LEVEL_METADATA_ROUTE_HIDDEN_DOOR &&
		    guidebot_later_required_target_reachable(
		        view, live_state, workspace, step))
			continue;
		if (candidate->kind == LEVEL_METADATA_ROUTE_EXIT &&
		    requires_control_center &&
		    !view->initial_control_center_destroyed) {
			local_summary.rejected_actions++;
			continue;
		}
		if (!guidebot_step_usable(view, candidate)) {
			if (!guidebot_prepare_switch_restorer(
			        view, candidate, &local_summary)) {
				local_summary.rejected_actions++;
				local_summary.blocking_step = step;
				local_summary.blocking_reason =
				    GUIDEBOT_ROUTE_CERTIFIER_REJECTION_INVALID_TARGET;
				break;
			}
		}
		/* A usable, still-required action is an ordering barrier.  If its
		 * prepared target cannot be certified, require a live replan instead
		 * of silently advancing to a later objective. */
		target_segment = guidebot_step_target_segment(view, candidate);
		if (!guidebot_valid_segment(view, target_segment)) {
			local_summary.rejected_actions++;
			local_summary.blocking_step = step;
			local_summary.blocking_reason =
			    GUIDEBOT_ROUTE_CERTIFIER_REJECTION_INVALID_TARGET;
			break;
		}
		{
			const int firing = guidebot_prepare_shoot_switch_position(
			    view, candidate, workspace, &local_summary, budget,
			    &tick_work);

			if (firing == GUIDEBOT_ROUTE_CERTIFIER_PENDING) {
				local_summary.visited_segments =
				    workspace->job_visited_segments;
				local_summary.evaluated_edges =
				    workspace->job_evaluated_edges;
				local_summary.evaluated_firing_positions =
				    workspace->job_evaluated_firing_positions;
				if (certifier_summary)
					*certifier_summary = local_summary;
				return GUIDEBOT_ROUTE_CERTIFIER_PENDING;
			}
			if (firing != GUIDEBOT_ROUTE_CERTIFIER_VALID) {
				local_summary.rejected_actions++;
				local_summary.blocking_step = step;
				local_summary.blocking_segment = target_segment;
				local_summary.blocking_reason =
				    GUIDEBOT_ROUTE_CERTIFIER_REJECTION_INVALID_TARGET;
				break;
			}
		}
		/* A trigger can complete while a firing-position search is yielding.
		 * Do not publish the now-obsolete action and force another job cycle */
		if (!level_metadata_route_step_required_by_world_state(
		        view, candidate))
			continue;
		target_segment = guidebot_step_target_segment(view, candidate);
		if (!guidebot_valid_segment(view, target_segment) ||
		    !workspace->reachable[target_segment]) {
			local_summary.rejected_actions++;
			local_summary.blocking_step = step;
			local_summary.blocking_segment = target_segment;
			local_summary.blocking_reason =
			    GUIDEBOT_ROUTE_CERTIFIER_REJECTION_UNREACHABLE_TARGET;
			break;
		}
		selected = step;
		selected_segment = target_segment;
		break;
	}
	if (selected < 0) {
		int incomplete = 0;

		for (step = 0; step < live_state->route_step_count; ++step)
			if (live_state->route_steps[step].kind !=
			        LEVEL_METADATA_ROUTE_START &&
			    level_metadata_route_step_required_by_world_state(
			        view, &live_state->route_steps[step])) {
				incomplete = 1;
				break;
			}
		if (incomplete) {
			if (certifier_summary)
				*certifier_summary = local_summary;
			guidebot_route_certifier_reset_job(workspace);
			return GUIDEBOT_ROUTE_CERTIFIER_INVALID;
		}
		live_state->route_status = LEVEL_METADATA_ROUTE_OK;
	} else {
		const level_metadata_route_step *pending =
		    &live_state->route_steps[selected];

		live_plan->first_pending_step = selected;
		live_plan->first_pending_path_segment_count =
		    pending->path_segment_count > 0 ? pending->path_segment_count : 1;
		live_plan->first_pending_path_terminal_segment = selected_segment;
		certificate->source_trigger = pending->trigger_num;
		certificate->source_wall = pending->wall_num;
		certificate->source_object = pending->key_carrier_objnum;
		certificate->frontier_segment = selected_segment;
	}
	certificate->status = GUIDEBOT_ROUTE_CERTIFICATE_VALID;
	local_summary.selected_step = selected;
	local_summary.selected_segment = selected_segment;
	if (certifier_summary)
		*certifier_summary = local_summary;
	guidebot_route_certifier_reset_job(workspace);
	return GUIDEBOT_ROUTE_CERTIFIER_VALID;
}

int guidebot_route_certify_current_state(
    const level_metadata_scan_view *view,
    const level_metadata_state *prepared_state,
    const route_planner_plan_summary *prepared_plan,
    guidebot_route_certifier_workspace *workspace,
    level_metadata_state *live_state,
    route_planner_plan_summary *live_plan,
    guidebot_route_validity_certificate *certificate,
    guidebot_route_certifier_summary *certifier_summary)
{
	guidebot_route_certifier_reset_job(workspace);
	return guidebot_route_certify_current_state_budgeted(
	    view, prepared_state, prepared_plan, workspace, live_state, live_plan,
	    certificate, certifier_summary, NULL);
}
