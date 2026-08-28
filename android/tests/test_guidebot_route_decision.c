#include "guidebot_route_decision.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef NDEBUG
#undef assert
#define assert(condition)                                                 \
	do {                                                                  \
		if (!(condition)) {                                               \
			fprintf(stderr, "assertion failed: %s (%s:%d)\n", #condition, \
			        __FILE__, __LINE__);                                  \
			abort();                                                      \
		}                                                                 \
	} while (0)
#endif

static void initialize_snapshot(route_snapshot_summary *snapshot)
{
	memset(snapshot, 0, sizeof(*snapshot));
	snapshot->topology_hash = 10;
	snapshot->state_hash = 20;
	snapshot->start_hash = 30;
	snapshot->progression_hash = 40;
	snapshot->navigation_hash = 50;
	snapshot->trigger_hash = 60;
	snapshot->object_hash = 70;
	snapshot->automap_hash = 80;
	snapshot->actor_hash = 90;
}

static void initialize_trigger_plan(
    level_metadata_state *state,
    route_planner_plan_summary *plan)
{
	level_metadata_route_step *step;

	memset(state, 0, sizeof(*state));
	memset(plan, 0, sizeof(*plan));
	state->route_status = LEVEL_METADATA_ROUTE_OK;
	state->route_step_count = 2;
	step = &state->route_steps[1];
	step->kind = LEVEL_METADATA_ROUTE_TRIGGER;
	step->activation_kind = LEVEL_METADATA_ROUTE_ACTIVATION_SHOOT_SWITCH;
	step->trigger_num = 12;
	step->wall_num = 44;
	step->key_index = -1;
	step->key_carrier_objnum = -1;
	step->seg = 137;
	step->side = 2;
	step->activation_pos_valid = 1;
	step->activation_pos[0] = 100;
	step->activation_pos[1] = 200;
	step->activation_pos[2] = 300;
	plan->endpoint_kind = ROUTE_PLANNER_ENDPOINT_END_OF_LEVEL;
	plan->route_step_count = 2;
	plan->first_pending_step = 1;
	plan->first_pending_path_terminal_segment = 137;
	plan->partial_frontier_segment = -1;
}

static void test_equivalent_inputs_are_equal(void)
{
	level_metadata_state state;
	route_planner_plan_summary plan;
	route_snapshot_summary first_snapshot;
	route_snapshot_summary second_snapshot;
	guidebot_route_decision first;
	guidebot_route_decision second;

	initialize_trigger_plan(&state, &plan);
	initialize_snapshot(&first_snapshot);
	second_snapshot = first_snapshot;
	second_snapshot.topology_generation = 9;
	second_snapshot.trigger_generation = 17;
	assert(guidebot_route_decision_project(
	    &state, &plan, &first_snapshot, 2, -1, &first));
	assert(guidebot_route_decision_project(
	    &state, &plan, &second_snapshot, 2, -1, &second));
	assert(guidebot_route_decision_semantic_equal(&first, &second));
	assert(guidebot_route_decision_guidance_equal(&first, &second));
	assert(first.semantic_hash == second.semantic_hash);
	assert(first.guidance_hash == second.guidance_hash);
	assert(first.input_hash == second.input_hash);
	assert(first.decision_hash == second.decision_hash);
	assert(first.trigger_hash == 60);
	assert(first.certificate.status == GUIDEBOT_ROUTE_CERTIFICATE_UNCHECKED);
}

static void test_objective_identity_ties_are_total_and_stable(void)
{
	guidebot_route_objective_identity first = {
		LEVEL_METADATA_ROUTE_TRIGGER, 4, 9, -1, 12, 3
	};
	guidebot_route_objective_identity second = first;

	assert(guidebot_route_objective_identity_compare(&first, &first) == 0);
	second.kind = LEVEL_METADATA_ROUTE_REACTOR;
	assert(guidebot_route_objective_identity_compare(&first, &second) < 0);
	second = first;
	second.trigger++;
	assert(guidebot_route_objective_identity_compare(&first, &second) < 0);
	second = first;
	second.wall++;
	assert(guidebot_route_objective_identity_compare(&first, &second) < 0);
	second = first;
	first.object = 2;
	second.object = 5;
	assert(guidebot_route_objective_identity_compare(&first, &second) < 0);
	second = first;
	second.segment++;
	assert(guidebot_route_objective_identity_compare(&first, &second) < 0);
	second = first;
	second.side++;
	assert(guidebot_route_objective_identity_compare(&first, &second) < 0);
	first.side = -1;
	second.side = 0;
	assert(guidebot_route_objective_identity_compare(&first, &second) > 0);
	assert(guidebot_route_objective_identity_compare(NULL, &second) > 0);
	assert(guidebot_route_objective_identity_compare(&second, NULL) < 0);
}

static void test_unrelated_object_state_does_not_invalidate_trigger(void)
{
	level_metadata_state state;
	route_planner_plan_summary plan;
	route_snapshot_summary first_snapshot;
	route_snapshot_summary second_snapshot;
	guidebot_route_decision first;
	guidebot_route_decision second;

	initialize_trigger_plan(&state, &plan);
	initialize_snapshot(&first_snapshot);
	second_snapshot = first_snapshot;
	second_snapshot.state_hash++;
	second_snapshot.object_hash++;
	assert(guidebot_route_decision_project(
	    &state, &plan, &first_snapshot, 2, -1, &first));
	assert(guidebot_route_decision_project(
	    &state, &plan, &second_snapshot, 2, -1, &second));
	assert(guidebot_route_decision_semantic_equal(&first, &second));
	assert(first.state_hash != second.state_hash);
	assert(first.object_hash != second.object_hash);
	assert((first.dependency_mask & GUIDEBOT_ROUTE_DEPENDENCY_TRIGGERS) != 0);
	assert((first.dependency_mask & GUIDEBOT_ROUTE_DEPENDENCY_OBJECTS) == 0);
	assert((first.dependency_mask & GUIDEBOT_ROUTE_DEPENDENCY_AUTOMAP) == 0);
	assert(first.input_hash == second.input_hash);
	assert(first.decision_hash == second.decision_hash);
}

static void test_object_and_automap_dependencies_follow_objective(void)
{
	level_metadata_state state;
	route_planner_plan_summary plan;
	route_snapshot_summary snapshot;
	guidebot_route_decision decision;

	initialize_trigger_plan(&state, &plan);
	initialize_snapshot(&snapshot);
	state.route_steps[1].kind = LEVEL_METADATA_ROUTE_KEY;
	state.route_steps[1].key_index = 1;
	state.route_steps[1].trigger_num = -1;
	assert(guidebot_route_decision_project(
	    &state, &plan, &snapshot, 2, -1, &decision));
	assert((decision.dependency_mask & GUIDEBOT_ROUTE_DEPENDENCY_OBJECTS) != 0);
	assert((decision.dependency_mask & GUIDEBOT_ROUTE_DEPENDENCY_TRIGGERS) == 0);
	assert((decision.dependency_mask & GUIDEBOT_ROUTE_DEPENDENCY_AUTOMAP) == 0);

	plan.endpoint_kind = ROUTE_PLANNER_ENDPOINT_UNEXPLORED;
	assert(guidebot_route_decision_project(
	    &state, &plan, &snapshot, 2, 99, &decision));
	assert((decision.dependency_mask & GUIDEBOT_ROUTE_DEPENDENCY_OBJECTS) != 0);
	assert((decision.dependency_mask & GUIDEBOT_ROUTE_DEPENDENCY_AUTOMAP) != 0);
}

static void test_actor_profile_changes_only_input_identity(void)
{
	level_metadata_state state;
	route_planner_plan_summary plan;
	route_snapshot_summary first_snapshot;
	route_snapshot_summary second_snapshot;
	guidebot_route_decision first;
	guidebot_route_decision second;

	initialize_trigger_plan(&state, &plan);
	initialize_snapshot(&first_snapshot);
	second_snapshot = first_snapshot;
	second_snapshot.actor_hash++;
	assert(guidebot_route_decision_project(
	    &state, &plan, &first_snapshot, 2, -1, &first));
	assert(guidebot_route_decision_project(
	    &state, &plan, &second_snapshot, 2, -1, &second));
	assert((first.dependency_mask & GUIDEBOT_ROUTE_DEPENDENCY_ACTOR) != 0);
	assert(guidebot_route_decision_semantic_equal(&first, &second));
	assert(guidebot_route_decision_guidance_equal(&first, &second));
	assert(first.input_hash != second.input_hash);
	assert(first.decision_hash != second.decision_hash);
}

static void test_semantic_and_guidance_changes_are_distinct(void)
{
	level_metadata_state state;
	route_planner_plan_summary plan;
	route_snapshot_summary snapshot;
	guidebot_route_decision original;
	guidebot_route_decision changed;

	initialize_trigger_plan(&state, &plan);
	initialize_snapshot(&snapshot);
	assert(guidebot_route_decision_project(
	    &state, &plan, &snapshot, 2, -1, &original));
	state.route_steps[1].activation_pos[0]++;
	assert(guidebot_route_decision_project(
	    &state, &plan, &snapshot, 2, -1, &changed));
	assert(guidebot_route_decision_semantic_equal(&original, &changed));
	assert(!guidebot_route_decision_guidance_equal(&original, &changed));
	state.route_steps[1].trigger_num++;
	assert(guidebot_route_decision_project(
	    &state, &plan, &snapshot, 2, -1, &changed));
	assert(!guidebot_route_decision_semantic_equal(&original, &changed));
}

static void test_terminal_statuses(void)
{
	level_metadata_state state;
	route_planner_plan_summary plan;
	guidebot_route_decision decision;

	memset(&state, 0, sizeof(state));
	memset(&plan, 0, sizeof(plan));
	state.route_status = LEVEL_METADATA_ROUTE_OK;
	plan.endpoint_kind = ROUTE_PLANNER_ENDPOINT_END_OF_LEVEL;
	plan.first_pending_step = -1;
	plan.first_pending_path_terminal_segment = -1;
	plan.partial_frontier_segment = -1;
	assert(guidebot_route_decision_project(
	    &state, &plan, NULL, 2, -1, &decision));
	assert(decision.status == GUIDEBOT_ROUTE_DECISION_COMPLETE);
	assert(decision.objective_kind == -1);
	assert(guidebot_route_decision_project(
	    NULL, NULL, NULL, 0, -1, &decision));
	assert(decision.status == GUIDEBOT_ROUTE_DECISION_CALCULATING);
}

static void test_path_adoption_retains_current_equivalent_guidance(void)
{
	level_metadata_state state;
	route_planner_plan_summary plan;
	route_snapshot_summary snapshot;
	guidebot_route_decision previous;
	guidebot_route_decision next;

	initialize_trigger_plan(&state, &plan);
	initialize_snapshot(&snapshot);
	assert(guidebot_route_decision_project(
	    &state, &plan, &snapshot, 2, -1, &previous));
	next = previous;
	assert(guidebot_route_decision_adoption_action(
	           1, &previous, 1, 1, &next) ==
	       GUIDEBOT_ROUTE_ADOPTION_RETAIN_PATH);
	assert(guidebot_route_decision_adoption_action(
	           1, &previous, 0, 1, &next) ==
	       GUIDEBOT_ROUTE_ADOPTION_RETAIN_PATH);
	next.objective_segment++;
	assert(guidebot_route_decision_adoption_action(
	           1, &previous, 0, 1, &next) ==
	       GUIDEBOT_ROUTE_ADOPTION_REPLACE_PATH);
	next = previous;
	next.path_terminal_segment++;
	assert(guidebot_route_decision_adoption_action(
	           1, &previous, 1, 1, &next) ==
	       GUIDEBOT_ROUTE_ADOPTION_RETAIN_PATH);
	next = previous;
	next.status = GUIDEBOT_ROUTE_DECISION_CALCULATING;
	assert(guidebot_route_decision_adoption_action(
	           1, &previous, 1, 1, &next) ==
	       GUIDEBOT_ROUTE_ADOPTION_RETAIN_PATH);
	assert(guidebot_route_decision_adoption_action(
	           1, &previous, 0, 1, &next) ==
	       GUIDEBOT_ROUTE_ADOPTION_STOP);
	assert(guidebot_route_decision_adoption_action(
	           1, &previous, 1, 0, NULL) ==
	       GUIDEBOT_ROUTE_ADOPTION_STOP);
}

static void test_passive_cache_publication_preserves_incumbent_waypoint(void)
{
	level_metadata_state state;
	route_planner_plan_summary plan;
	route_snapshot_summary snapshot;
	guidebot_route_decision incumbent;
	guidebot_route_decision cached;

	initialize_trigger_plan(&state, &plan);
	initialize_snapshot(&snapshot);
	assert(guidebot_route_decision_project(
	    &state, &plan, &snapshot, 2, -1, &incumbent));
	state.route_steps[1].activation_pos[0] += 1000;
	plan.first_pending_path_terminal_segment = 320;
	assert(guidebot_route_decision_project(
	    &state, &plan, &snapshot, 2, -1, &cached));
	assert(guidebot_route_decision_semantic_equal(&incumbent, &cached));
	assert(!guidebot_route_decision_guidance_equal(&incumbent, &cached));
	assert(guidebot_route_passive_adoption_action(
	           1, 1,
	           guidebot_route_decision_semantic_equal(&incumbent, &cached)) ==
	       GUIDEBOT_ROUTE_ADOPTION_RETAIN_PATH);
	assert(guidebot_route_decision_adoption_action(
	           1, &incumbent, 0, 1, &cached) ==
	       GUIDEBOT_ROUTE_ADOPTION_REPLACE_PATH);
	assert(guidebot_route_passive_adoption_action(1, 0, 0) ==
	       GUIDEBOT_ROUTE_ADOPTION_RETAIN_PATH);
	assert(guidebot_route_passive_adoption_action(1, 1, 0) ==
	       GUIDEBOT_ROUTE_ADOPTION_REPLACE_PATH);
	assert(guidebot_route_passive_adoption_action(0, 1, 0) ==
	       GUIDEBOT_ROUTE_ADOPTION_REPLACE_PATH);
	assert(guidebot_route_passive_adoption_action(0, 0, 0) ==
	       GUIDEBOT_ROUTE_ADOPTION_STOP);
}

static void test_shadow_classifies_mismatches(void)
{
	level_metadata_state state;
	route_planner_plan_summary plan;
	route_snapshot_summary snapshot;
	guidebot_route_decision primary;
	guidebot_route_decision shadow;
	guidebot_route_shadow_summary summary;

	memset(&summary, 0, sizeof(summary));
	initialize_trigger_plan(&state, &plan);
	initialize_snapshot(&snapshot);
	assert(guidebot_route_decision_project(
	    &state, &plan, &snapshot, 2, -1, &primary));
	shadow = primary;
	guidebot_route_shadow_record(
	    &summary, 1, &primary, 1, &shadow, 4, 10, &snapshot, 2, 20, 0);
	assert(summary.mismatch_kind == GUIDEBOT_ROUTE_SHADOW_MATCH);
	assert(summary.fixture.reason_kind == GUIDEBOT_ROUTE_SHADOW_REASON_NONE);
	assert(summary.fixture.version == GUIDEBOT_ROUTE_SHADOW_FIXTURE_VERSION);
	assert(summary.fixture.game == 2);
	assert(summary.fixture.level == 20);
	assert(summary.fixture.snapshot.state_hash == snapshot.state_hash);
	assert(summary.semantic_matches == 1);
	assert(summary.guidance_matches == 1);
	shadow.guidance_position[0]++;
	shadow.guidance_hash++;
	guidebot_route_shadow_record(
	    &summary, 1, &primary, 1, &shadow, 5, 20, &snapshot, 2, 20, 0);
	assert(summary.mismatch_kind == GUIDEBOT_ROUTE_SHADOW_GUIDANCE_MISMATCH);
	assert(summary.fixture.reason_kind ==
	       GUIDEBOT_ROUTE_SHADOW_REASON_ACTIVATION_VISIBILITY);
	assert(summary.guidance_mismatches == 1);
	shadow = primary;
	shadow.objective_trigger++;
	shadow.semantic_hash++;
	guidebot_route_shadow_record(
	    &summary, 1, &primary, 1, &shadow, 6, 30, &snapshot, 2, 20, 0);
	assert(summary.mismatch_kind == GUIDEBOT_ROUTE_SHADOW_SEMANTIC_MISMATCH);
	assert(summary.fixture.reason_kind ==
	       GUIDEBOT_ROUTE_SHADOW_REASON_TARGET_RANKING);
	assert(summary.semantic_mismatches == 1);
	shadow.objective_kind++;
	guidebot_route_shadow_record(
	    &summary, 1, &primary, 1, &shadow, 6, 30, &snapshot, 2, 20, 0);
	assert(summary.fixture.reason_kind ==
	       GUIDEBOT_ROUTE_SHADOW_REASON_OUT_OF_ORDER_ACTION);
	guidebot_route_shadow_record(
	    &summary, 1, &primary, 0, NULL, 7, 40, &snapshot, 2, 20, 0);
	assert(summary.mismatch_kind ==
	       GUIDEBOT_ROUTE_SHADOW_AVAILABILITY_MISMATCH);
	assert(summary.fixture.reason_kind ==
	       GUIDEBOT_ROUTE_SHADOW_REASON_CACHE_READINESS);
	assert(summary.availability_mismatches == 1);
	shadow = primary;
	shadow.path_terminal_segment++;
	guidebot_route_shadow_record(
	    &summary, 1, &primary, 1, &shadow, 8, 50, &snapshot, 2, 20, 0);
	assert(summary.mismatch_kind == GUIDEBOT_ROUTE_SHADOW_PROOF_MISMATCH);
	assert(summary.fixture.reason_kind ==
	       GUIDEBOT_ROUTE_SHADOW_REASON_ACTOR_ACCESSIBILITY);
	assert(summary.proof_mismatches == 1);
	guidebot_route_shadow_record(
	    &summary, 1, &primary, 1, &shadow, 9, 60, &snapshot, 2, 20,
	    GUIDEBOT_ROUTE_SHADOW_HINT_REVERSIBLE_WALL_STATE);
	assert(summary.fixture.reason_kind ==
	       GUIDEBOT_ROUTE_SHADOW_REASON_REVERSIBLE_WALL_STATE);
	shadow = primary;
	shadow.guidance_hash++;
	guidebot_route_shadow_record(
	    &summary, 1, &primary, 1, &shadow, 10, 70, &snapshot, 2, 20, 0);
	assert(summary.fixture.reason_kind ==
	       GUIDEBOT_ROUTE_SHADOW_REASON_PLANNER_DEFECT);
	guidebot_route_shadow_record(
	    &summary, 1, &primary, 0, NULL, 11, 80, &snapshot, 2, 20,
	    GUIDEBOT_ROUTE_SHADOW_HINT_STALE_COMPLETION_HISTORY);
	assert(summary.fixture.reason_kind ==
	       GUIDEBOT_ROUTE_SHADOW_REASON_STALE_COMPLETION_HISTORY);
	assert(strcmp(
	           guidebot_route_shadow_reason_name(
	               summary.fixture.reason_kind),
	           "stale_completion_history") == 0);
	assert(summary.attempts == 9);
	assert(summary.total_us == 390);
	assert(summary.max_us == 80);
	assert(summary.last_fvi_count == 11);
}

int main(void)
{
	test_equivalent_inputs_are_equal();
	test_objective_identity_ties_are_total_and_stable();
	test_unrelated_object_state_does_not_invalidate_trigger();
	test_object_and_automap_dependencies_follow_objective();
	test_actor_profile_changes_only_input_identity();
	test_semantic_and_guidance_changes_are_distinct();
	test_terminal_statuses();
	test_path_adoption_retains_current_equivalent_guidance();
	test_passive_cache_publication_preserves_incumbent_waypoint();
	test_shadow_classifies_mismatches();
	return 0;
}
