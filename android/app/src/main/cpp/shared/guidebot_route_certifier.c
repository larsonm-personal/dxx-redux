#include "guidebot_route_certifier.h"

#include <string.h>

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

int guidebot_route_side_passable_current(
    const level_metadata_scan_view *view,
    int segment,
    int side)
{
	int child;
	int clearance;
	int wall;
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
	if (view->initial_control_center_destroyed &&
	    view->side_is_control_center_link &&
	    (view->side_is_control_center_link(view->user, segment, side) ||
	     (reverse >= 0 && view->side_is_control_center_link(
	                          view->user, child, reverse))))
		return 1;
	if (view->side_is_hard_blocked &&
	    (view->side_is_hard_blocked(view->user, segment, side) ||
	     (reverse >= 0 &&
	      view->side_is_hard_blocked(view->user, child, reverse))))
		return 0;
	wall = view->wall_num ? view->wall_num(view->user, segment, side) : -1;
	if (wall < 0 || wall >= view->num_walls)
		return 1;
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
	if (view->triggered_side_opener_count &&
	    (view->triggered_side_opener_count(view->user, segment, side) > 0 ||
	     (reverse >= 0 && view->triggered_side_opener_count(
	                          view->user, child, reverse) > 0)) &&
	    (!view->wall_keys ||
	     view->wall_keys(view->user, wall) == view->wall_key_none))
		return 0;
	return !view->wall_keys ||
	       guidebot_key_allowed(view, view->wall_keys(view->user, wall));
}

static int guidebot_build_reachability(
    const level_metadata_scan_view *view,
    guidebot_route_certifier_workspace *workspace,
    guidebot_route_certifier_summary *summary)
{
	int head = 0;
	int tail = 0;

	if (!guidebot_valid_segment(view, view->start_segment) ||
	    !view->segment_child)
		return 0;
	memset(workspace->reachable, 0, sizeof(workspace->reachable));
	workspace->reachable[view->start_segment] = 1;
	workspace->queue[tail++] = view->start_segment;
	while (head < tail) {
		int segment = workspace->queue[head++];
		int side;

		summary->visited_segments++;
		for (side = 0; side < LEVEL_METADATA_MAX_SIDES; ++side) {
			int child;

			summary->evaluated_edges++;
			child = view->segment_child(view->user, segment, side);
			if (!guidebot_valid_segment(view, child) ||
			    workspace->reachable[child] ||
			    !guidebot_route_side_passable_current(view, segment, side))
				continue;
			workspace->reachable[child] = 1;
			workspace->queue[tail++] = child;
		}
	}
	return 1;
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
	return 1;
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
	guidebot_route_certifier_summary local_summary;
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
	memset(certificate, 0, sizeof(*certificate));
	certificate->status = GUIDEBOT_ROUTE_CERTIFICATE_INVALID;
	certificate->source_trigger = -1;
	certificate->source_wall = -1;
	certificate->source_object = -1;
	certificate->frontier_segment = -1;
	if (prepared_state->route_step_count < 0 ||
	    prepared_state->route_step_count > LEVEL_METADATA_MAX_ROUTE_STEPS ||
	    !guidebot_build_reachability(view, workspace, &local_summary))
		return 0;
	*live_state = *prepared_state;
	*live_plan = *prepared_plan;
	live_plan->first_pending_step = -1;
	live_plan->first_pending_path_segment_count = 0;
	live_plan->first_pending_path_terminal_segment = -1;
	live_plan->partial_frontier_segment = -1;
	for (step = 0; step < live_state->route_step_count; ++step)
		if (live_state->route_steps[step].kind == LEVEL_METADATA_ROUTE_REACTOR ||
		    live_state->route_steps[step].kind == LEVEL_METADATA_ROUTE_BOSS)
			requires_control_center = 1;
	for (step = 0; step < live_state->route_step_count; ++step) {
		level_metadata_route_step *candidate = &live_state->route_steps[step];
		int target_segment;

		if (candidate->kind == LEVEL_METADATA_ROUTE_START ||
		    !level_metadata_route_step_required_by_world_state(view, candidate))
			continue;
		local_summary.evaluated_actions++;
		if ((candidate->kind == LEVEL_METADATA_ROUTE_EXIT &&
		     requires_control_center &&
		     !view->initial_control_center_destroyed) ||
		    !guidebot_step_usable(view, candidate)) {
			local_summary.rejected_actions++;
			continue;
		}
		/* A usable, still-required action is an ordering barrier.  If its
		 * prepared target cannot be certified, require a live replan instead
		 * of silently advancing to a later objective. */
		target_segment = guidebot_step_target_segment(view, candidate);
		if (!guidebot_valid_segment(view, target_segment)) {
			local_summary.rejected_actions++;
			break;
		}
		if (!workspace->reachable[target_segment]) {
			local_summary.rejected_actions++;
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
			return 0;
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
	return 1;
}
