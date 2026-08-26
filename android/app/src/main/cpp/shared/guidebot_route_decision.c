#include "guidebot_route_decision.h"

#include <limits.h>
#include <string.h>

#define GUIDEBOT_HASH_OFFSET 1469598103934665603ULL
#define GUIDEBOT_HASH_PRIME  1099511628211ULL

static unsigned long long guidebot_hash_int(
    unsigned long long hash,
    int value)
{
	unsigned int input = (unsigned int) value;
	int byte;

	for (byte = 0; byte < 4; ++byte) {
		hash ^= (input >> (byte * 8)) & 0xffu;
		hash *= GUIDEBOT_HASH_PRIME;
	}
	return hash;
}

static int guidebot_route_identity_value(int value)
{
	return value < 0 ? INT_MAX : value;
}

int guidebot_route_objective_identity_compare(
    const guidebot_route_objective_identity *left,
    const guidebot_route_objective_identity *right)
{
	int left_values[6];
	int right_values[6];
	int index;

	if (left == right)
		return 0;
	if (!left)
		return 1;
	if (!right)
		return -1;
	left_values[0] = guidebot_route_identity_value(left->kind);
	left_values[1] = guidebot_route_identity_value(left->trigger);
	left_values[2] = guidebot_route_identity_value(left->wall);
	left_values[3] = guidebot_route_identity_value(left->object);
	left_values[4] = guidebot_route_identity_value(left->segment);
	left_values[5] = guidebot_route_identity_value(left->side);
	right_values[0] = guidebot_route_identity_value(right->kind);
	right_values[1] = guidebot_route_identity_value(right->trigger);
	right_values[2] = guidebot_route_identity_value(right->wall);
	right_values[3] = guidebot_route_identity_value(right->object);
	right_values[4] = guidebot_route_identity_value(right->segment);
	right_values[5] = guidebot_route_identity_value(right->side);
	for (index = 0; index < 6; ++index) {
		if (left_values[index] < right_values[index])
			return -1;
		if (left_values[index] > right_values[index])
			return 1;
	}
	return 0;
}

static unsigned long long guidebot_hash_u64(
    unsigned long long hash,
    unsigned long long value)
{
	int byte;

	for (byte = 0; byte < 8; ++byte) {
		hash ^= (value >> (byte * 8)) & 0xffu;
		hash *= GUIDEBOT_HASH_PRIME;
	}
	return hash;
}

static unsigned long long guidebot_input_hash(
    const guidebot_route_decision *decision)
{
	unsigned long long hash = GUIDEBOT_HASH_OFFSET;

	hash = guidebot_hash_int(hash, decision->target_policy);
	hash = guidebot_hash_int(hash, decision->requested_target_segment);
	hash = guidebot_hash_int(hash, (int) decision->dependency_mask);
	if (decision->dependency_mask & GUIDEBOT_ROUTE_DEPENDENCY_TOPOLOGY)
		hash = guidebot_hash_u64(hash, decision->topology_hash);
	if (decision->dependency_mask & GUIDEBOT_ROUTE_DEPENDENCY_START)
		hash = guidebot_hash_u64(hash, decision->start_hash);
	if (decision->dependency_mask & GUIDEBOT_ROUTE_DEPENDENCY_PROGRESSION)
		hash = guidebot_hash_u64(hash, decision->progression_hash);
	if (decision->dependency_mask & GUIDEBOT_ROUTE_DEPENDENCY_NAVIGATION)
		hash = guidebot_hash_u64(hash, decision->navigation_hash);
	if (decision->dependency_mask & GUIDEBOT_ROUTE_DEPENDENCY_TRIGGERS)
		hash = guidebot_hash_u64(hash, decision->trigger_hash);
	if (decision->dependency_mask & GUIDEBOT_ROUTE_DEPENDENCY_OBJECTS)
		hash = guidebot_hash_u64(hash, decision->object_hash);
	if (decision->dependency_mask & GUIDEBOT_ROUTE_DEPENDENCY_AUTOMAP)
		hash = guidebot_hash_u64(hash, decision->automap_hash);
	if (decision->dependency_mask & GUIDEBOT_ROUTE_DEPENDENCY_ACTOR)
		hash = guidebot_hash_u64(hash, decision->actor_hash);
	return hash;
}

static unsigned long long guidebot_semantic_hash(
    const guidebot_route_decision *decision)
{
	unsigned long long hash = GUIDEBOT_HASH_OFFSET;

	hash = guidebot_hash_int(hash, decision->version);
	hash = guidebot_hash_int(hash, decision->status);
	hash = guidebot_hash_int(hash, decision->target_policy);
	hash = guidebot_hash_int(hash, decision->requested_target_segment);
	hash = guidebot_hash_int(hash, decision->objective_kind);
	hash = guidebot_hash_int(hash, decision->activation_kind);
	hash = guidebot_hash_int(hash, decision->objective_trigger);
	hash = guidebot_hash_int(hash, decision->objective_wall);
	hash = guidebot_hash_int(hash, decision->objective_key);
	hash = guidebot_hash_int(hash, decision->objective_object);
	hash = guidebot_hash_int(hash, decision->objective_segment);
	hash = guidebot_hash_int(hash, decision->objective_side);
	return hash;
}

static unsigned long long guidebot_guidance_hash(
    const guidebot_route_decision *decision)
{
	unsigned long long hash = decision->semantic_hash;
	int coordinate;

	hash = guidebot_hash_int(hash, decision->guidance_position_valid);
	if (decision->guidance_position_valid)
		for (coordinate = 0; coordinate < 3; ++coordinate)
			hash = guidebot_hash_int(
			    hash, decision->guidance_position[coordinate]);
	if (decision->objective_kind < 0)
		hash = guidebot_hash_int(hash, decision->partial_frontier_segment);
	return hash;
}

void guidebot_route_decision_clear(guidebot_route_decision *decision)
{
	if (!decision)
		return;
	memset(decision, 0, sizeof(*decision));
	decision->version = GUIDEBOT_ROUTE_DECISION_VERSION;
	decision->status = GUIDEBOT_ROUTE_DECISION_UNAVAILABLE;
	decision->target_policy = -1;
	decision->requested_target_segment = -1;
	decision->objective_kind = -1;
	decision->activation_kind = LEVEL_METADATA_ROUTE_ACTIVATION_NONE;
	decision->objective_trigger = -1;
	decision->objective_wall = -1;
	decision->objective_key = -1;
	decision->objective_object = -1;
	decision->objective_segment = -1;
	decision->objective_side = -1;
	decision->path_terminal_segment = -1;
	decision->partial_frontier_segment = -1;
	decision->certificate.status = GUIDEBOT_ROUTE_CERTIFICATE_UNCHECKED;
	decision->certificate.source_trigger = -1;
	decision->certificate.source_wall = -1;
	decision->certificate.source_object = -1;
	decision->certificate.frontier_segment = -1;
}

static int guidebot_decision_status(
    const level_metadata_state *state,
    const route_planner_plan_summary *plan,
    int readiness)
{
	if (readiness == 0)
		return GUIDEBOT_ROUTE_DECISION_CALCULATING;
	if (!state || !plan)
		return readiness == 4 ? GUIDEBOT_ROUTE_DECISION_FAILED
		                      : GUIDEBOT_ROUTE_DECISION_UNAVAILABLE;
	if (plan->first_pending_step >= 0)
		return state->route_status == LEVEL_METADATA_ROUTE_PARTIAL
		           ? GUIDEBOT_ROUTE_DECISION_PARTIAL
		           : GUIDEBOT_ROUTE_DECISION_READY;
	if (state->route_status == LEVEL_METADATA_ROUTE_OK)
		return GUIDEBOT_ROUTE_DECISION_COMPLETE;
	if (state->route_status == LEVEL_METADATA_ROUTE_PARTIAL ||
	    plan->partial_frontier_segment >= 0)
		return GUIDEBOT_ROUTE_DECISION_PARTIAL;
	return GUIDEBOT_ROUTE_DECISION_FAILED;
}

static unsigned int guidebot_decision_dependency_mask(
    const guidebot_route_decision *decision)
{
	unsigned int mask =
	    GUIDEBOT_ROUTE_DEPENDENCY_TOPOLOGY |
	    GUIDEBOT_ROUTE_DEPENDENCY_START |
	    GUIDEBOT_ROUTE_DEPENDENCY_PROGRESSION |
	    GUIDEBOT_ROUTE_DEPENDENCY_NAVIGATION |
	    GUIDEBOT_ROUTE_DEPENDENCY_TARGET_POLICY |
	    GUIDEBOT_ROUTE_DEPENDENCY_ACTOR;

	if (decision->target_policy == ROUTE_PLANNER_ENDPOINT_UNEXPLORED)
		mask |= GUIDEBOT_ROUTE_DEPENDENCY_AUTOMAP;
	if (decision->objective_kind == LEVEL_METADATA_ROUTE_TRIGGER)
		mask |= GUIDEBOT_ROUTE_DEPENDENCY_TRIGGERS;
	if (decision->objective_kind == LEVEL_METADATA_ROUTE_KEY ||
	    decision->objective_kind == LEVEL_METADATA_ROUTE_REACTOR ||
	    decision->objective_kind == LEVEL_METADATA_ROUTE_BOSS)
		mask |= GUIDEBOT_ROUTE_DEPENDENCY_OBJECTS;
	if (decision->objective_kind < 0 &&
	    decision->status != GUIDEBOT_ROUTE_DECISION_COMPLETE)
		mask |= GUIDEBOT_ROUTE_DEPENDENCY_TRIGGERS |
		        GUIDEBOT_ROUTE_DEPENDENCY_OBJECTS;
	return mask;
}

int guidebot_route_decision_project(
    const level_metadata_state *state,
    const route_planner_plan_summary *plan,
    const route_snapshot_summary *snapshot,
    int readiness,
    int requested_target_segment,
    guidebot_route_decision *decision)
{
	const level_metadata_route_step *step = NULL;

	if (!decision)
		return 0;
	guidebot_route_decision_clear(decision);
	decision->status = guidebot_decision_status(state, plan, readiness);
	decision->requested_target_segment = requested_target_segment;
	if (plan) {
		decision->target_policy = plan->endpoint_kind;
		decision->path_terminal_segment =
		    plan->first_pending_path_terminal_segment;
		decision->partial_frontier_segment =
		    plan->partial_frontier_segment;
	}
	if (snapshot) {
		decision->topology_hash = snapshot->topology_hash;
		decision->state_hash = snapshot->state_hash;
		decision->start_hash = snapshot->start_hash;
		decision->progression_hash = snapshot->progression_hash;
		decision->navigation_hash = snapshot->navigation_hash;
		decision->trigger_hash = snapshot->trigger_hash;
		decision->object_hash = snapshot->object_hash;
		decision->automap_hash = snapshot->automap_hash;
		decision->actor_hash = snapshot->actor_hash;
	}
	if (state && plan && plan->first_pending_step >= 0 &&
	    plan->first_pending_step < state->route_step_count)
		step = &state->route_steps[plan->first_pending_step];
	if (step) {
		decision->objective_kind = step->kind;
		decision->activation_kind = step->activation_kind;
		decision->objective_trigger = step->trigger_num;
		decision->objective_wall = step->wall_num;
		decision->objective_key = step->key_index;
		decision->objective_object = step->key_carrier_objnum;
		decision->objective_segment = step->seg;
		decision->objective_side = step->side;
		decision->guidance_position_valid = step->activation_pos_valid;
		if (step->activation_pos_valid)
			memcpy(
			    decision->guidance_position, step->activation_pos,
			    sizeof(decision->guidance_position));
		decision->certificate.source_trigger = step->trigger_num;
		decision->certificate.source_wall = step->wall_num;
		decision->certificate.source_object = step->key_carrier_objnum;
	}
	decision->certificate.frontier_segment =
	    decision->partial_frontier_segment;
	decision->dependency_mask = guidebot_decision_dependency_mask(decision);
	decision->input_hash = guidebot_input_hash(decision);
	decision->semantic_hash = guidebot_semantic_hash(decision);
	decision->guidance_hash = guidebot_guidance_hash(decision);
	decision->decision_hash = guidebot_hash_u64(
	    guidebot_hash_u64(
	        GUIDEBOT_HASH_OFFSET, decision->input_hash),
	    decision->guidance_hash);
	return 1;
}

int guidebot_route_decision_semantic_equal(
    const guidebot_route_decision *left,
    const guidebot_route_decision *right)
{
	return left && right && left->semantic_hash == right->semantic_hash &&
	       left->version == right->version &&
	       left->status == right->status &&
	       left->target_policy == right->target_policy &&
	       left->requested_target_segment == right->requested_target_segment &&
	       left->objective_kind == right->objective_kind &&
	       left->activation_kind == right->activation_kind &&
	       left->objective_trigger == right->objective_trigger &&
	       left->objective_wall == right->objective_wall &&
	       left->objective_key == right->objective_key &&
	       left->objective_object == right->objective_object &&
	       left->objective_segment == right->objective_segment &&
	       left->objective_side == right->objective_side;
}

int guidebot_route_decision_guidance_equal(
    const guidebot_route_decision *left,
    const guidebot_route_decision *right)
{
	if (!guidebot_route_decision_semantic_equal(left, right) ||
	    left->guidance_hash != right->guidance_hash ||
	    left->guidance_position_valid != right->guidance_position_valid ||
	    (left->objective_kind < 0 &&
	     left->partial_frontier_segment != right->partial_frontier_segment))
		return 0;
	return !left->guidance_position_valid ||
	       memcmp(
	           left->guidance_position, right->guidance_position,
	           sizeof(left->guidance_position)) == 0;
}

static int guidebot_route_decision_has_guidance(
    const guidebot_route_decision *decision)
{
	return decision &&
	       (decision->status == GUIDEBOT_ROUTE_DECISION_READY ||
	        decision->status == GUIDEBOT_ROUTE_DECISION_PARTIAL) &&
	       (decision->objective_kind >= 0 ||
	        decision->partial_frontier_segment >= 0);
}

int guidebot_route_decision_supports_exit_command(
    const guidebot_route_decision *decision)
{
	return decision &&
	       (decision->status == GUIDEBOT_ROUTE_DECISION_READY ||
	        decision->status == GUIDEBOT_ROUTE_DECISION_PARTIAL) &&
	       decision->certificate.status == GUIDEBOT_ROUTE_CERTIFICATE_VALID &&
	       decision->objective_kind >= 0;
}

int guidebot_route_decision_adoption_action(
    int previous_valid,
    const guidebot_route_decision *previous,
    int previous_certificate_valid,
    int next_valid,
    const guidebot_route_decision *next)
{
	if (next_valid && next &&
	    next->status == GUIDEBOT_ROUTE_DECISION_CALCULATING)
		return previous_valid && previous_certificate_valid &&
		               guidebot_route_decision_has_guidance(previous)
		           ? GUIDEBOT_ROUTE_ADOPTION_RETAIN_PATH
		           : GUIDEBOT_ROUTE_ADOPTION_STOP;
	if (!next_valid || !guidebot_route_decision_has_guidance(next))
		return GUIDEBOT_ROUTE_ADOPTION_STOP;
	if (previous_valid && guidebot_route_decision_has_guidance(previous) &&
	    guidebot_route_decision_guidance_equal(previous, next))
		return GUIDEBOT_ROUTE_ADOPTION_RETAIN_PATH;
	return GUIDEBOT_ROUTE_ADOPTION_REPLACE_PATH;
}

const char *guidebot_route_decision_status_name(int status)
{
	switch (status) {
		case GUIDEBOT_ROUTE_DECISION_CALCULATING: return "calculating";
		case GUIDEBOT_ROUTE_DECISION_READY: return "ready";
		case GUIDEBOT_ROUTE_DECISION_COMPLETE: return "complete";
		case GUIDEBOT_ROUTE_DECISION_PARTIAL: return "partial";
		case GUIDEBOT_ROUTE_DECISION_FAILED: return "failed";
		default: return "unavailable";
	}
}

const char *guidebot_route_shadow_mismatch_name(int kind)
{
	switch (kind) {
		case GUIDEBOT_ROUTE_SHADOW_AVAILABILITY_MISMATCH:
			return "availability";
		case GUIDEBOT_ROUTE_SHADOW_SEMANTIC_MISMATCH:
			return "semantic";
		case GUIDEBOT_ROUTE_SHADOW_GUIDANCE_MISMATCH:
			return "guidance";
		case GUIDEBOT_ROUTE_SHADOW_PROOF_MISMATCH:
			return "proof";
		default: return "match";
	}
}

const char *guidebot_route_shadow_reason_name(int kind)
{
	switch (kind) {
		case GUIDEBOT_ROUTE_SHADOW_REASON_STALE_COMPLETION_HISTORY:
			return "stale_completion_history";
		case GUIDEBOT_ROUTE_SHADOW_REASON_REVERSIBLE_WALL_STATE:
			return "reversible_wall_state";
		case GUIDEBOT_ROUTE_SHADOW_REASON_OUT_OF_ORDER_ACTION:
			return "out_of_order_action";
		case GUIDEBOT_ROUTE_SHADOW_REASON_ACTOR_ACCESSIBILITY:
			return "actor_accessibility";
		case GUIDEBOT_ROUTE_SHADOW_REASON_TARGET_RANKING:
			return "target_ranking";
		case GUIDEBOT_ROUTE_SHADOW_REASON_ACTIVATION_VISIBILITY:
			return "activation_visibility";
		case GUIDEBOT_ROUTE_SHADOW_REASON_CACHE_READINESS:
			return "cache_readiness";
		case GUIDEBOT_ROUTE_SHADOW_REASON_PLANNER_DEFECT:
			return "planner_defect";
		default: return "none";
	}
}

static int guidebot_route_shadow_reason(
    const guidebot_route_shadow_summary *summary,
    unsigned int hints)
{
	if (!summary || summary->mismatch_kind == GUIDEBOT_ROUTE_SHADOW_MATCH)
		return GUIDEBOT_ROUTE_SHADOW_REASON_NONE;
	if (hints & GUIDEBOT_ROUTE_SHADOW_HINT_STALE_COMPLETION_HISTORY)
		return GUIDEBOT_ROUTE_SHADOW_REASON_STALE_COMPLETION_HISTORY;
	if (summary->mismatch_kind ==
	    GUIDEBOT_ROUTE_SHADOW_AVAILABILITY_MISMATCH)
		return GUIDEBOT_ROUTE_SHADOW_REASON_CACHE_READINESS;
	if (summary->mismatch_kind == GUIDEBOT_ROUTE_SHADOW_SEMANTIC_MISMATCH)
		return summary->primary.objective_kind ==
		               summary->shadow.objective_kind
		           ? GUIDEBOT_ROUTE_SHADOW_REASON_TARGET_RANKING
		           : GUIDEBOT_ROUTE_SHADOW_REASON_OUT_OF_ORDER_ACTION;
	if (summary->primary.guidance_position_valid !=
	        summary->shadow.guidance_position_valid ||
	    (summary->primary.guidance_position_valid &&
	     memcmp(
	         summary->primary.guidance_position,
	         summary->shadow.guidance_position,
	         sizeof(summary->primary.guidance_position)) != 0))
		return GUIDEBOT_ROUTE_SHADOW_REASON_ACTIVATION_VISIBILITY;
	if (summary->primary.path_terminal_segment !=
	        summary->shadow.path_terminal_segment ||
	    summary->primary.partial_frontier_segment !=
	        summary->shadow.partial_frontier_segment)
		return hints & GUIDEBOT_ROUTE_SHADOW_HINT_REVERSIBLE_WALL_STATE
		           ? GUIDEBOT_ROUTE_SHADOW_REASON_REVERSIBLE_WALL_STATE
		           : GUIDEBOT_ROUTE_SHADOW_REASON_ACTOR_ACCESSIBILITY;
	if (hints & GUIDEBOT_ROUTE_SHADOW_HINT_CACHE_READINESS)
		return GUIDEBOT_ROUTE_SHADOW_REASON_CACHE_READINESS;
	return GUIDEBOT_ROUTE_SHADOW_REASON_PLANNER_DEFECT;
}

void guidebot_route_shadow_record(
    guidebot_route_shadow_summary *summary,
    int primary_valid,
    const guidebot_route_decision *primary,
    int shadow_valid,
    const guidebot_route_decision *shadow,
    unsigned int fvi_count,
    unsigned long long elapsed_us,
    const route_snapshot_summary *snapshot,
    int game,
    int level,
    unsigned int hints)
{
	if (!summary)
		return;
	summary->attempted = 1;
	summary->primary_valid = primary_valid != 0;
	summary->shadow_valid = shadow_valid != 0;
	summary->attempts++;
	summary->last_fvi_count = fvi_count;
	summary->total_us += elapsed_us;
	if (elapsed_us > summary->max_us)
		summary->max_us = elapsed_us;
	if (primary)
		summary->primary = *primary;
	else
		guidebot_route_decision_clear(&summary->primary);
	if (shadow)
		summary->shadow = *shadow;
	else
		guidebot_route_decision_clear(&summary->shadow);
	if (summary->primary_valid != summary->shadow_valid) {
		summary->mismatch_kind =
		    GUIDEBOT_ROUTE_SHADOW_AVAILABILITY_MISMATCH;
		summary->availability_mismatches++;
	} else if (summary->primary_valid &&
	           !guidebot_route_decision_semantic_equal(
	               &summary->primary, &summary->shadow)) {
		summary->mismatch_kind = GUIDEBOT_ROUTE_SHADOW_SEMANTIC_MISMATCH;
		summary->semantic_mismatches++;
	} else {
		summary->semantic_matches++;
		if (summary->primary_valid &&
		    !guidebot_route_decision_guidance_equal(
		        &summary->primary, &summary->shadow)) {
			summary->mismatch_kind =
			    GUIDEBOT_ROUTE_SHADOW_GUIDANCE_MISMATCH;
			summary->guidance_mismatches++;
		} else if (summary->primary_valid &&
		           (summary->primary.path_terminal_segment !=
		                summary->shadow.path_terminal_segment ||
		            summary->primary.partial_frontier_segment !=
		                summary->shadow.partial_frontier_segment ||
		            summary->primary.certificate.frontier_segment !=
		                summary->shadow.certificate.frontier_segment)) {
			summary->mismatch_kind =
			    GUIDEBOT_ROUTE_SHADOW_PROOF_MISMATCH;
			summary->proof_mismatches++;
			summary->guidance_matches++;
		} else {
			summary->mismatch_kind = GUIDEBOT_ROUTE_SHADOW_MATCH;
			summary->guidance_matches++;
		}
	}
	memset(&summary->fixture, 0, sizeof(summary->fixture));
	summary->fixture.version = GUIDEBOT_ROUTE_SHADOW_FIXTURE_VERSION;
	summary->fixture.hints = hints;
	summary->fixture.game = game;
	summary->fixture.level = level;
	summary->fixture.mismatch_kind = summary->mismatch_kind;
	summary->fixture.reason_kind =
	    guidebot_route_shadow_reason(summary, hints);
	if (snapshot)
		summary->fixture.snapshot = *snapshot;
}
