/* Guidebot runtime save schema 1: explicit fields, no ABI padding or pointers
 * Changing these field lists requires a new save schema */
#ifndef DXX_GUIDEBOT_METADATA_SAVE_FIELDS_H
#define DXX_GUIDEBOT_METADATA_SAVE_FIELDS_H
#include "guidebot_save_io.h"

static void gb_save_level_metadata_route_step(guidebot_save_stream *s, level_metadata_route_step *v)
{
	GB_FIELD(s, v->kind, GB_SIGNED);
	GB_FIELD(s, v->seg, GB_SIGNED);
	GB_FIELD(s, v->side, GB_SIGNED);
	GB_FIELD(s, v->wall_num, GB_SIGNED);
	GB_FIELD(s, v->trigger_num, GB_SIGNED);
	GB_FIELD(s, v->trigger_type, GB_SIGNED);
	GB_FIELD(s, v->key_index, GB_SIGNED);
	GB_FIELD(s, v->key_carrier_objnum, GB_SIGNED);
	GB_FIELD(s, v->can_be_bypassed, GB_SIGNED);
	GB_FIELD(s, v->activation_kind, GB_SIGNED);
	GB_FIELD(s, v->requires_guided_missile, GB_SIGNED);
	GB_FIELD(s, v->guided_missile_point_count, GB_SIGNED);
	GB_FIELD(s, v->is_switch_restorer, GB_SIGNED);
	GB_FIELD(s, v->restored_wall_num, GB_SIGNED);
	GB_FIELD(s, v->switch_shot_quality, GB_SIGNED);
	GB_FIELD(s, v->switch_shot_incidence_cosine, GB_SIGNED);
	GB_LIMIT(s, v->switch_guidance_candidate_count, GB_SIGNED, 0, LEVEL_METADATA_MAX_SWITCH_GUIDANCE_CANDIDATES);
	for (size_t i0 = 0; i0 < LEVEL_METADATA_MAX_SWITCH_GUIDANCE_CANDIDATES; ++i0) {
		GB_FIELD(s, v->switch_guidance_candidate_seg[i0], GB_SIGNED);
	}
	for (size_t i0 = 0; i0 < LEVEL_METADATA_MAX_SWITCH_GUIDANCE_CANDIDATES; ++i0) {
		for (size_t i1 = 0; i1 < 3; ++i1) {
			GB_FIELD(s, v->switch_guidance_candidate_pos[i0][i1], GB_SIGNED);
		}
	}
	for (size_t i0 = 0; i0 < LEVEL_METADATA_MAX_SWITCH_GUIDANCE_CANDIDATES; ++i0) {
		GB_FIELD(s, v->switch_guidance_candidate_quality[i0], GB_SIGNED);
	}
	for (size_t i0 = 0; i0 < LEVEL_METADATA_MAX_SWITCH_GUIDANCE_CANDIDATES; ++i0) {
		GB_FIELD(s, v->switch_guidance_candidate_incidence[i0], GB_SIGNED);
	}
	GB_FIELD(s, v->path_segment_count, GB_SIGNED);
	GB_FIELD(s, v->path_terminal_segment, GB_SIGNED);
	GB_FIELD(s, v->activation_pos_valid, GB_SIGNED);
	for (size_t i0 = 0; i0 < 3; ++i0) {
		GB_FIELD(s, v->activation_pos[i0], GB_SIGNED);
	}
	GB_FIELD(s, v->aim_pos_valid, GB_SIGNED);
	for (size_t i0 = 0; i0 < 3; ++i0) {
		GB_FIELD(s, v->aim_pos[i0], GB_SIGNED);
	}
	GB_FIELD(s, v->label_pos_valid, GB_SIGNED);
	for (size_t i0 = 0; i0 < 3; ++i0) {
		GB_FIELD(s, v->label_pos[i0], GB_SIGNED);
	}
	GB_FIELD(s, v->distance_from_previous, GB_DOUBLE);
	GB_TEXT(s, v->label);
	GB_TEXT(s, v->trigger_type_name);
	GB_LIMIT(s, v->opened_link_count, GB_SIGNED, 0, LEVEL_METADATA_MAX_ROUTE_LINKS);
	for (size_t i0 = 0; i0 < LEVEL_METADATA_MAX_ROUTE_LINKS; ++i0) {
		GB_FIELD(s, v->opened_link_seg[i0], GB_SIGNED);
	}
	for (size_t i0 = 0; i0 < LEVEL_METADATA_MAX_ROUTE_LINKS; ++i0) {
		GB_FIELD(s, v->opened_link_side[i0], GB_SIGNED);
	}
	for (size_t i0 = 0; i0 < LEVEL_METADATA_MAX_ROUTE_LINKS; ++i0) {
		GB_FIELD(s, v->opened_link_wall[i0], GB_SIGNED);
	}
}

static void gb_save_level_metadata_state(guidebot_save_stream *s, level_metadata_state *v)
{
	GB_FIELD(s, v->energy_center_segment_count, GB_SIGNED);
	GB_FIELD(s, v->energy_center_raw_count, GB_SIGNED);
	GB_FIELD(s, v->energy_center_count, GB_SIGNED);
	GB_FIELD(s, v->energy_center_group_distance, GB_SIGNED);
	GB_FIELD(s, v->energy_center_nearest_raw_distance, GB_SIGNED);
	GB_FIELD(s, v->matcen_segment_count, GB_SIGNED);
	GB_FIELD(s, v->matcen_raw_count, GB_SIGNED);
	GB_FIELD(s, v->matcen_count, GB_SIGNED);
	GB_FIELD(s, v->mine_volume, GB_DOUBLE);
	GB_FIELD(s, v->mine_volume_normalized, GB_DOUBLE);
	GB_FIELD(s, v->travel_distance, GB_DOUBLE);
	GB_FIELD(s, v->travel_time_seconds, GB_SIGNED);
	GB_FIELD(s, v->guidebot_count, GB_SIGNED);
	GB_FIELD(s, v->guidebot_placed, GB_SIGNED);
	GB_FIELD(s, v->guidebot_accessible, GB_SIGNED);
	GB_TEXT(s, v->guidebot_placement_note);
	GB_TEXT(s, v->guidebot_note);
	GB_FIELD(s, v->route_status, GB_SIGNED);
	GB_TEXT(s, v->route_problem);
	GB_TEXT(s, v->route_note);
	GB_FIELD(s, v->route_required_key_mask, GB_SIGNED);
	GB_FIELD(s, v->route_completing_key_mask_set, GB_SIGNED);
	GB_FIELD(s, v->unnecessary_key_mask, GB_SIGNED);
	GB_LIMIT(s, v->route_step_count, GB_SIGNED, 0, LEVEL_METADATA_MAX_ROUTE_STEPS);
	for (size_t i0 = 0; i0 < LEVEL_METADATA_MAX_ROUTE_STEPS; ++i0) {
		gb_save_level_metadata_route_step(s, &v->route_steps[i0]);
	}
}

static void gb_save_route_planner_plan_summary(guidebot_save_stream *s, route_planner_plan_summary *v)
{
	GB_FIELD(s, v->endpoint_kind, GB_SIGNED);
	GB_LIMIT(s, v->route_step_count, GB_SIGNED, 0, LEVEL_METADATA_MAX_ROUTE_STEPS);
	GB_LIMIT(s, v->first_pending_step, GB_SIGNED, -1, LEVEL_METADATA_MAX_ROUTE_STEPS - 1);
	GB_FIELD(s, v->first_pending_path_segment_count, GB_SIGNED);
	GB_FIELD(s, v->first_pending_path_terminal_segment, GB_SIGNED);
	GB_FIELD(s, v->partial_frontier_segment, GB_SIGNED);
}

static void gb_save_route_snapshot_summary(guidebot_save_stream *s, route_snapshot_summary *v)
{
	GB_FIELD(s, v->topology_hash, GB_UNSIGNED);
	GB_FIELD(s, v->state_hash, GB_UNSIGNED);
	GB_FIELD(s, v->start_hash, GB_UNSIGNED);
	GB_FIELD(s, v->progression_hash, GB_UNSIGNED);
	GB_FIELD(s, v->navigation_hash, GB_UNSIGNED);
	GB_FIELD(s, v->trigger_hash, GB_UNSIGNED);
	GB_FIELD(s, v->object_hash, GB_UNSIGNED);
	GB_FIELD(s, v->automap_hash, GB_UNSIGNED);
	GB_FIELD(s, v->actor_hash, GB_UNSIGNED);
	GB_FIELD(s, v->segment_count, GB_SIGNED);
	GB_FIELD(s, v->wall_count, GB_SIGNED);
	GB_FIELD(s, v->trigger_count, GB_SIGNED);
	GB_FIELD(s, v->object_count, GB_SIGNED);
	GB_FIELD(s, v->start_segment, GB_SIGNED);
	GB_FIELD(s, v->key_mask, GB_SIGNED);
	GB_FIELD(s, v->control_center_destroyed, GB_SIGNED);
	GB_FIELD(s, v->topology_generation, GB_UNSIGNED);
	GB_FIELD(s, v->start_generation, GB_UNSIGNED);
	GB_FIELD(s, v->progression_generation, GB_UNSIGNED);
	GB_FIELD(s, v->navigation_generation, GB_UNSIGNED);
	GB_FIELD(s, v->trigger_generation, GB_UNSIGNED);
	GB_FIELD(s, v->object_generation, GB_UNSIGNED);
	GB_FIELD(s, v->automap_generation, GB_UNSIGNED);
	GB_FIELD(s, v->actor_generation, GB_UNSIGNED);
}

static void gb_save_guidebot_route_validity_certificate(guidebot_save_stream *s, guidebot_route_validity_certificate *v)
{
	GB_FIELD(s, v->status, GB_SIGNED);
	GB_FIELD(s, v->source_trigger, GB_SIGNED);
	GB_FIELD(s, v->source_wall, GB_SIGNED);
	GB_FIELD(s, v->source_object, GB_SIGNED);
	GB_FIELD(s, v->frontier_segment, GB_SIGNED);
}

static void gb_save_guidebot_route_decision(guidebot_save_stream *s, guidebot_route_decision *v)
{
	GB_FIELD(s, v->version, GB_UNSIGNED);
	GB_FIELD(s, v->status, GB_SIGNED);
	GB_FIELD(s, v->target_policy, GB_SIGNED);
	GB_FIELD(s, v->requested_target_segment, GB_SIGNED);
	GB_FIELD(s, v->objective_kind, GB_SIGNED);
	GB_FIELD(s, v->activation_kind, GB_SIGNED);
	GB_FIELD(s, v->requires_guided_missile, GB_SIGNED);
	GB_FIELD(s, v->objective_trigger, GB_SIGNED);
	GB_FIELD(s, v->objective_wall, GB_SIGNED);
	GB_FIELD(s, v->objective_key, GB_SIGNED);
	GB_FIELD(s, v->objective_object, GB_SIGNED);
	GB_FIELD(s, v->objective_segment, GB_SIGNED);
	GB_FIELD(s, v->objective_side, GB_SIGNED);
	GB_FIELD(s, v->guidance_position_valid, GB_SIGNED);
	for (size_t i0 = 0; i0 < 3; ++i0) {
		GB_FIELD(s, v->guidance_position[i0], GB_SIGNED);
	}
	GB_FIELD(s, v->path_terminal_segment, GB_SIGNED);
	GB_FIELD(s, v->partial_frontier_segment, GB_SIGNED);
	GB_FIELD(s, v->dependency_mask, GB_UNSIGNED);
	GB_FIELD(s, v->topology_hash, GB_UNSIGNED);
	GB_FIELD(s, v->state_hash, GB_UNSIGNED);
	GB_FIELD(s, v->start_hash, GB_UNSIGNED);
	GB_FIELD(s, v->progression_hash, GB_UNSIGNED);
	GB_FIELD(s, v->navigation_hash, GB_UNSIGNED);
	GB_FIELD(s, v->trigger_hash, GB_UNSIGNED);
	GB_FIELD(s, v->object_hash, GB_UNSIGNED);
	GB_FIELD(s, v->automap_hash, GB_UNSIGNED);
	GB_FIELD(s, v->actor_hash, GB_UNSIGNED);
	GB_FIELD(s, v->input_hash, GB_UNSIGNED);
	GB_FIELD(s, v->semantic_hash, GB_UNSIGNED);
	GB_FIELD(s, v->guidance_hash, GB_UNSIGNED);
	GB_FIELD(s, v->decision_hash, GB_UNSIGNED);
	gb_save_guidebot_route_validity_certificate(s, &v->certificate);
}

static void gb_save_guidebot_route_certifier_workspace(guidebot_save_stream *s, guidebot_route_certifier_workspace *v)
{
	for (size_t i0 = 0; i0 < LEVEL_METADATA_MAX_SEGMENTS; ++i0) {
		GB_FIELD(s, v->reachable[i0], GB_BYTES);
	}
	for (size_t i0 = 0; i0 < LEVEL_METADATA_MAX_SEGMENTS; ++i0) {
		GB_FIELD(s, v->queue[i0], GB_SIGNED);
	}
	for (size_t i0 = 0; i0 < LEVEL_METADATA_MAX_SEGMENTS; ++i0) {
		GB_FIELD(s, v->strategic_distance[i0], GB_SIGNED);
	}
	for (size_t i0 = 0; i0 < LEVEL_METADATA_MAX_SEGMENTS; ++i0) {
		GB_FIELD(s, v->physical_distance[i0], GB_SIGNED);
	}
	GB_FIELD(s, v->firing_cache_valid, GB_SIGNED);
	GB_LIMIT(s, v->firing_cache_num_segments, GB_SIGNED, 0, LEVEL_METADATA_MAX_SEGMENTS);
	GB_FIELD(s, v->firing_cache_num_walls, GB_SIGNED);
	GB_FIELD(s, v->firing_cache_trigger, GB_SIGNED);
	GB_FIELD(s, v->firing_cache_wall, GB_SIGNED);
	for (size_t i0 = 0; i0 < 3; ++i0) {
		GB_FIELD(s, v->firing_cache_aim[i0], GB_SIGNED);
	}
	GB_FIELD(s, v->firing_cache_segment, GB_SIGNED);
	GB_FIELD(s, v->firing_cache_path_segment_count, GB_SIGNED);
	for (size_t i0 = 0; i0 < 3; ++i0) {
		GB_FIELD(s, v->firing_cache_position[i0], GB_SIGNED);
	}
	GB_FIELD(s, v->firing_cache_shot_quality, GB_SIGNED);
	GB_FIELD(s, v->firing_cache_incidence_cosine, GB_SIGNED);
	GB_FIELD(s, v->job_active, GB_SIGNED);
	GB_FIELD(s, v->job_start_segment, GB_SIGNED);
	GB_LIMIT(s, v->job_num_segments, GB_SIGNED, 0, LEVEL_METADATA_MAX_SEGMENTS);
	GB_LIMIT(s, v->reach_head, GB_SIGNED, 0, LEVEL_METADATA_MAX_SEGMENTS);
	GB_LIMIT(s, v->reach_tail, GB_SIGNED, 0, LEVEL_METADATA_MAX_SEGMENTS);
	GB_LIMIT(s, v->reach_side, GB_SIGNED, 0, LEVEL_METADATA_MAX_SIDES);
	GB_FIELD(s, v->reach_complete, GB_SIGNED);
	GB_FIELD(s, v->job_visited_segments, GB_UNSIGNED);
	GB_FIELD(s, v->job_evaluated_edges, GB_UNSIGNED);
	GB_FIELD(s, v->job_evaluated_firing_positions, GB_UNSIGNED);
	GB_FIELD(s, v->firing_search_active, GB_SIGNED);
	GB_FIELD(s, v->firing_search_pass, GB_SIGNED);
	GB_FIELD(s, v->firing_search_segment, GB_SIGNED);
	GB_LIMIT(s, v->firing_search_detailed_count, GB_SIGNED, 0, 8);
	GB_FIELD(s, v->firing_search_selection_segment, GB_SIGNED);
	GB_FIELD(s, v->firing_search_selection_complete, GB_SIGNED);
	GB_LIMIT(s, v->firing_search_detailed_index, GB_SIGNED, 0, 8);
	GB_FIELD(s, v->firing_search_detailed_sample, GB_SIGNED);
	for (size_t i0 = 0; i0 < 8; ++i0) {
		GB_FIELD(s, v->firing_search_detailed_segments[i0], GB_SIGNED);
	}
	for (size_t i0 = 0; i0 < 8; ++i0) {
		GB_FIELD(s, v->firing_search_detailed_scores[i0], GB_LONG_DOUBLE);
	}
	GB_FIELD(s, v->firing_search_original_segment, GB_SIGNED);
	for (size_t i0 = 0; i0 < 3; ++i0) {
		GB_FIELD(s, v->firing_search_original_position[i0], GB_SIGNED);
	}
	GB_FIELD(s, v->firing_search_best_segment, GB_SIGNED);
	GB_FIELD(s, v->firing_search_best_path_distance, GB_SIGNED);
	for (size_t i0 = 0; i0 < 3; ++i0) {
		GB_FIELD(s, v->firing_search_best_position[i0], GB_SIGNED);
	}
	GB_FIELD(s, v->firing_search_best_quality, GB_SIGNED);
	GB_FIELD(s, v->firing_search_best_incidence, GB_SIGNED);
	GB_FIELD(s, v->firing_search_best_score, GB_LONG_DOUBLE);
	GB_FIELD(s, v->firing_frontier_active, GB_SIGNED);
	GB_FIELD(s, v->firing_frontier_phase, GB_SIGNED);
	GB_FIELD(s, v->firing_frontier_goal_segment, GB_SIGNED);
	GB_FIELD(s, v->firing_frontier_init_segment, GB_SIGNED);
	GB_LIMIT(s, v->firing_frontier_head, GB_SIGNED, 0, LEVEL_METADATA_MAX_SEGMENTS);
	GB_LIMIT(s, v->firing_frontier_tail, GB_SIGNED, 0, LEVEL_METADATA_MAX_SEGMENTS);
	GB_FIELD(s, v->firing_frontier_side, GB_SIGNED);
	GB_FIELD(s, v->firing_frontier_scan_segment, GB_SIGNED);
	GB_FIELD(s, v->firing_frontier_best_segment, GB_SIGNED);
	GB_FIELD(s, v->firing_frontier_best_remaining, GB_SIGNED);
	GB_FIELD(s, v->firing_frontier_best_incidence, GB_SIGNED);
	GB_FIELD(s, v->unexplored_active, GB_SIGNED);
	GB_FIELD(s, v->unexplored_init_segment, GB_SIGNED);
	GB_FIELD(s, v->unexplored_scan_segment, GB_SIGNED);
	GB_LIMIT(s, v->unexplored_component_head, GB_SIGNED, 0, LEVEL_METADATA_MAX_SEGMENTS);
	GB_LIMIT(s, v->unexplored_component_tail, GB_SIGNED, 0, LEVEL_METADATA_MAX_SEGMENTS);
	GB_FIELD(s, v->unexplored_component_side, GB_SIGNED);
	GB_FIELD(s, v->unexplored_component_size, GB_SIGNED);
	GB_FIELD(s, v->unexplored_component_target, GB_SIGNED);
	GB_FIELD(s, v->unexplored_component_distance, GB_SIGNED);
	GB_FIELD(s, v->unexplored_best_size, GB_SIGNED);
	GB_FIELD(s, v->unexplored_best_target, GB_SIGNED);
	GB_FIELD(s, v->unexplored_best_distance, GB_SIGNED);
}

static void gb_save_guidebot_route_certifier_summary(guidebot_save_stream *s, guidebot_route_certifier_summary *v)
{
	GB_FIELD(s, v->selected_step, GB_SIGNED);
	GB_FIELD(s, v->selected_segment, GB_SIGNED);
	GB_FIELD(s, v->blocking_step, GB_SIGNED);
	GB_FIELD(s, v->blocking_segment, GB_SIGNED);
	GB_FIELD(s, v->blocking_reason, GB_SIGNED);
	GB_FIELD(s, v->used_prepared_fallback, GB_SIGNED);
	GB_FIELD(s, v->required_steps_low, GB_UNSIGNED);
	GB_FIELD(s, v->visited_segments, GB_UNSIGNED);
	GB_FIELD(s, v->evaluated_edges, GB_UNSIGNED);
	GB_FIELD(s, v->evaluated_actions, GB_UNSIGNED);
	GB_FIELD(s, v->rejected_actions, GB_UNSIGNED);
	GB_FIELD(s, v->evaluated_firing_positions, GB_UNSIGNED);
	GB_FIELD(s, v->reranked_firing_position, GB_SIGNED);
	GB_FIELD(s, v->firing_cache_hit, GB_SIGNED);
	GB_FIELD(s, v->approximate_firing_position, GB_SIGNED);
	GB_FIELD(s, v->steep_firing_position, GB_SIGNED);
}

static void gb_save_level_metadata_live_work_summary(guidebot_save_stream *s, level_metadata_live_work_summary *v)
{
	GB_FIELD(s, v->full_plan_calls, GB_UNSIGNED);
	GB_FIELD(s, v->blocked_full_plan_calls, GB_UNSIGNED);
	GB_FIELD(s, v->deferred_refreshes, GB_UNSIGNED);
	GB_FIELD(s, v->retained_incumbents, GB_UNSIGNED);
	GB_FIELD(s, v->deferred_without_incumbent, GB_UNSIGNED);
	GB_FIELD(s, v->route_ticks, GB_UNSIGNED);
	GB_FIELD(s, v->deferred_ticks, GB_UNSIGNED);
	GB_FIELD(s, v->completed_ticks, GB_UNSIGNED);
	GB_FIELD(s, v->tick_overruns, GB_UNSIGNED);
	GB_FIELD(s, v->compiled_selector_calls, GB_UNSIGNED);
	GB_FIELD(s, v->compiled_selector_successes, GB_UNSIGNED);
	GB_FIELD(s, v->compiled_selector_failures, GB_UNSIGNED);
	GB_FIELD(s, v->compiled_selector_max_evaluated_actions, GB_UNSIGNED);
	GB_FIELD(s, v->pending_event_mask, GB_UNSIGNED);
	GB_FIELD(s, v->pending, GB_SIGNED);
	GB_FIELD(s, v->reachability_cursor, GB_SIGNED);
	GB_FIELD(s, v->firing_candidate_cursor, GB_SIGNED);
	GB_FIELD(s, v->firing_candidate_pass, GB_SIGNED);
	GB_FIELD(s, v->unexplored_candidate_cursor, GB_SIGNED);
	GB_FIELD(s, v->last_tick_us, GB_UNSIGNED);
	GB_FIELD(s, v->max_tick_us, GB_UNSIGNED);
	GB_FIELD(s, v->last_refresh_us, GB_UNSIGNED);
	GB_FIELD(s, v->max_refresh_us, GB_UNSIGNED);
	GB_FIELD(s, v->refresh_overruns, GB_UNSIGNED);
}

#endif
