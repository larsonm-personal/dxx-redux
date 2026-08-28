#include "guidebot_route_certifier.h"

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

#define TEST_SEGMENTS 5
#define TEST_WALLS    4
#define TEST_TRIGGERS 2

typedef struct certifier_fixture {
	int child[TEST_SEGMENTS][LEVEL_METADATA_MAX_SIDES];
	int wall[TEST_SEGMENTS][LEVEL_METADATA_MAX_SIDES];
	int wall_type[TEST_WALLS];
	int wall_open[TEST_WALLS];
	int wall_key[TEST_WALLS];
	int hard_blocked[TEST_WALLS];
	int control_center_link[TEST_WALLS];
	int wall_shootable[TEST_WALLS];
	int trigger_flags[TEST_TRIGGERS];
	int object_dead;
} certifier_fixture;

static guidebot_route_certifier_workspace Workspace;
static level_metadata_state Prepared;
static level_metadata_state Live;

static int segment_child(void *user, int segment, int side)
{
	certifier_fixture *fixture = (certifier_fixture *) user;
	return fixture->child[segment][side];
}

static int reverse_side(void *user, int segment, int child)
{
	certifier_fixture *fixture = (certifier_fixture *) user;
	int side;

	for (side = 0; side < LEVEL_METADATA_MAX_SIDES; ++side)
		if (fixture->child[child][side] == segment)
			return side;
	return -1;
}

static int side_is_flyable(void *user, int segment, int side)
{
	certifier_fixture *fixture = (certifier_fixture *) user;
	int wall = fixture->wall[segment][side];
	return wall < 0 || fixture->wall_open[wall];
}

static int side_clearance(void *user, int segment, int side)
{
	(void) user;
	(void) segment;
	(void) side;
	return 100;
}

static int side_is_hard_blocked(void *user, int segment, int side)
{
	certifier_fixture *fixture = (certifier_fixture *) user;
	int wall = fixture->wall[segment][side];

	return wall >= 0 && fixture->hard_blocked[wall];
}

static int side_is_control_center_link(void *user, int segment, int side)
{
	certifier_fixture *fixture = (certifier_fixture *) user;
	int wall = fixture->wall[segment][side];

	return wall >= 0 && fixture->control_center_link[wall];
}

static int wall_num(void *user, int segment, int side)
{
	certifier_fixture *fixture = (certifier_fixture *) user;
	return fixture->wall[segment][side];
}

static int wall_segment(void *user, int wall)
{
	(void) user;
	return wall;
}

static int wall_side(void *user, int wall)
{
	(void) user;
	(void) wall;
	return 0;
}

static int wall_type(void *user, int wall)
{
	certifier_fixture *fixture = (certifier_fixture *) user;
	return fixture->wall_type[wall];
}

static int wall_flags(void *user, int wall)
{
	certifier_fixture *fixture = (certifier_fixture *) user;
	return fixture->wall_open[wall] ? 2 : 0;
}

static int wall_keys(void *user, int wall)
{
	certifier_fixture *fixture = (certifier_fixture *) user;
	return fixture->wall_key[wall];
}

static int wall_clip_flags(void *user, int wall)
{
	(void) user;
	(void) wall;
	return 0;
}

static int wall_is_shootable_trigger(void *user, int wall)
{
	certifier_fixture *fixture = (certifier_fixture *) user;
	return fixture->wall_shootable[wall];
}

static int trigger_flags(void *user, int trigger)
{
	certifier_fixture *fixture = (certifier_fixture *) user;
	return fixture->trigger_flags[trigger];
}

static int triggered_side_opener_count(void *user, int segment, int side)
{
	certifier_fixture *fixture = (certifier_fixture *) user;
	int wall = fixture->wall[segment][side];

	return wall >= 0 && wall < 2 ? 1 : 0;
}

static int object_count(void *user)
{
	(void) user;
	return 1;
}

static int object_type(void *user, int object)
{
	(void) user;
	(void) object;
	return 3;
}

static int object_flags(void *user, int object)
{
	certifier_fixture *fixture = (certifier_fixture *) user;
	(void) object;
	return fixture->object_dead ? 1 : 0;
}

static int object_segment(void *user, int object)
{
	(void) user;
	(void) object;
	return 2;
}

static int object_position(void *user, int object, int xyz[3])
{
	(void) user;
	(void) object;
	xyz[0] = 20;
	xyz[1] = 0;
	xyz[2] = 0;
	return 1;
}

static int segment_center(void *user, int segment, int xyz[3])
{
	(void) user;
	xyz[0] = 100 + segment;
	xyz[1] = 0;
	xyz[2] = 0;
	return 1;
}

static int wall_shootable_from_position(
    void *user, int segment, const int from_pos[3], int wall)
{
	certifier_fixture *fixture = (certifier_fixture *) user;
	(void) segment;
	(void) from_pos;
	return fixture->wall_shootable[wall];
}

static level_metadata_scan_view make_view(certifier_fixture *fixture)
{
	level_metadata_scan_view view;

	memset(&view, 0, sizeof(view));
	view.user = fixture;
	view.num_segments = TEST_SEGMENTS;
	view.num_walls = TEST_WALLS;
	view.num_triggers = TEST_TRIGGERS;
	view.navigator_radius = 10;
	view.wall_type_door = 1;
	view.wall_type_illusion = 2;
	view.wall_type_open = 3;
	view.wall_flag_door_opened = 2;
	view.wall_flag_door_locked = 4;
	view.wall_clip_hidden = 8;
	view.wall_key_none = 0;
	view.wall_key_blue = 1;
	view.wall_key_red = 2;
	view.wall_key_gold = 3;
	view.obj_type_powerup = 2;
	view.obj_type_control_center = 3;
	view.obj_flag_should_be_dead = 1;
	view.trigger_flag_disabled = 1;
	view.trigger_type_unlock_door = 10;
	view.segment_child = segment_child;
	view.reverse_side = reverse_side;
	view.side_is_flyable = side_is_flyable;
	view.side_clearance_radius = side_clearance;
	view.side_is_hard_blocked = side_is_hard_blocked;
	view.side_is_control_center_link = side_is_control_center_link;
	view.wall_num = wall_num;
	view.wall_segment = wall_segment;
	view.wall_side = wall_side;
	view.wall_type = wall_type;
	view.wall_flags = wall_flags;
	view.wall_keys = wall_keys;
	view.wall_clip_flags = wall_clip_flags;
	view.wall_is_shootable_trigger = wall_is_shootable_trigger;
	view.trigger_flags = trigger_flags;
	view.triggered_side_opener_count = triggered_side_opener_count;
	view.object_count = object_count;
	view.object_type = object_type;
	view.object_flags = object_flags;
	view.object_segment = object_segment;
	view.object_position = object_position;
	view.segment_center = segment_center;
	view.wall_shootable_from_position = wall_shootable_from_position;
	return view;
}

static void initialize_fixture(certifier_fixture *fixture)
{
	int segment;
	int side;

	memset(fixture, 0, sizeof(*fixture));
	for (segment = 0; segment < TEST_SEGMENTS; ++segment)
		for (side = 0; side < LEVEL_METADATA_MAX_SIDES; ++side) {
			fixture->child[segment][side] = -1;
			fixture->wall[segment][side] = -1;
		}
	for (segment = 0; segment + 1 < TEST_SEGMENTS; ++segment) {
		fixture->child[segment][0] = segment + 1;
		fixture->wall[segment][0] = segment;
		fixture->child[segment + 1][1] = segment;
		fixture->wall[segment + 1][1] = segment;
	}
	for (segment = 0; segment < TEST_WALLS; ++segment) {
		fixture->wall_type[segment] = 1;
		fixture->wall_shootable[segment] = 1;
	}
}

static void initialize_plan(route_planner_plan_summary *plan)
{
	level_metadata_route_step *step;

	memset(&Prepared, 0, sizeof(Prepared));
	memset(plan, 0, sizeof(*plan));
	Prepared.route_status = LEVEL_METADATA_ROUTE_OK;
	Prepared.route_step_count = 5;
	Prepared.route_steps[0].kind = LEVEL_METADATA_ROUTE_START;
	step = &Prepared.route_steps[1];
	step->kind = LEVEL_METADATA_ROUTE_TRIGGER;
	step->activation_kind = LEVEL_METADATA_ROUTE_ACTIVATION_SHOOT_SWITCH;
	step->trigger_num = 0;
	step->wall_num = 0;
	step->seg = 0;
	step->path_segment_count = 1;
	step->path_terminal_segment = 0;
	step->activation_pos_valid = 1;
	step->activation_pos[0] = 10;
	step->aim_pos_valid = 1;
	step->aim_pos[0] = 20;
	step->opened_link_count = 1;
	step->opened_link_wall[0] = 0;
	step = &Prepared.route_steps[2];
	step->kind = LEVEL_METADATA_ROUTE_TRIGGER;
	step->activation_kind = LEVEL_METADATA_ROUTE_ACTIVATION_SHOOT_SWITCH;
	step->trigger_num = 1;
	step->wall_num = 1;
	step->seg = 1;
	step->path_segment_count = 1;
	step->path_terminal_segment = 1;
	step->activation_pos_valid = 1;
	step->activation_pos[0] = 11;
	step->aim_pos_valid = 1;
	step->aim_pos[0] = 21;
	step->opened_link_count = 1;
	step->opened_link_wall[0] = 1;
	step = &Prepared.route_steps[3];
	step->kind = LEVEL_METADATA_ROUTE_REACTOR;
	step->activation_kind = LEVEL_METADATA_ROUTE_ACTIVATION_DESTROY_REACTOR;
	step->seg = 2;
	step->path_segment_count = 1;
	step->path_terminal_segment = 2;
	step = &Prepared.route_steps[4];
	step->kind = LEVEL_METADATA_ROUTE_EXIT;
	step->activation_kind = LEVEL_METADATA_ROUTE_ACTIVATION_ENTER_EXIT;
	step->seg = 3;
	step->path_segment_count = 1;
	step->path_terminal_segment = 3;
	plan->endpoint_kind = ROUTE_PLANNER_ENDPOINT_END_OF_LEVEL;
	plan->route_step_count = Prepared.route_step_count;
	plan->first_pending_step = 1;
	plan->first_pending_path_segment_count = 1;
	plan->first_pending_path_terminal_segment = 0;
	plan->partial_frontier_segment = -1;
}

static int certify(
    level_metadata_scan_view *view,
    const route_planner_plan_summary *prepared_plan,
    route_planner_plan_summary *live_plan,
    guidebot_route_validity_certificate *certificate,
    guidebot_route_certifier_summary *summary)
{
	return guidebot_route_certify_current_state(
	    view, &Prepared, prepared_plan, &Workspace, &Live, live_plan,
	    certificate, summary);
}

static void test_current_start_and_accessibility_select_action(void)
{
	certifier_fixture fixture;
	level_metadata_scan_view view;
	route_planner_plan_summary prepared_plan;
	route_planner_plan_summary live_plan;
	guidebot_route_validity_certificate certificate;
	guidebot_route_certifier_summary summary;

	initialize_fixture(&fixture);
	initialize_plan(&prepared_plan);
	view = make_view(&fixture);
	view.start_segment = 0;
	if (!certify(
	        &view, &prepared_plan, &live_plan, &certificate, &summary))
		fprintf(
		    stderr, "initial certification failed visited=%u edges=%u actions=%u rejected=%u\n",
		    summary.visited_segments, summary.evaluated_edges,
		    summary.evaluated_actions, summary.rejected_actions);
	assert(summary.selected_step >= 0);
	assert(live_plan.first_pending_step == 1);
	assert(summary.selected_segment == 0);
	assert(certificate.status == GUIDEBOT_ROUTE_CERTIFICATE_VALID);

	fixture.wall_open[0] = 1;
	view.start_segment = 1;
	assert(certify(
	    &view, &prepared_plan, &live_plan, &certificate, &summary));
	assert(live_plan.first_pending_step == 2);
	assert(summary.selected_segment == 1);
	assert(certificate.source_trigger == 1);

	fixture.wall_open[1] = 1;
	assert(certify(
	    &view, &prepared_plan, &live_plan, &certificate, &summary));
	assert(live_plan.first_pending_step == 3);
	assert(summary.selected_segment == 2);

	view.initial_control_center_destroyed = 1;
	fixture.object_dead = 1;
	fixture.wall_open[2] = 1;
	assert(certify(
	    &view, &prepared_plan, &live_plan, &certificate, &summary));
	assert(live_plan.first_pending_step == 4);
	assert(summary.selected_segment == 3);
}

static void test_first_reachable_required_action_preserves_order(void)
{
	certifier_fixture fixture;
	level_metadata_scan_view view;
	route_planner_plan_summary prepared_plan;
	route_planner_plan_summary live_plan;
	guidebot_route_validity_certificate certificate;
	guidebot_route_certifier_summary summary;

	initialize_fixture(&fixture);
	initialize_plan(&prepared_plan);
	view = make_view(&fixture);
	view.start_segment = 0;
	Prepared.route_steps[2].seg = 0;
	Prepared.route_steps[2].path_terminal_segment = 0;

	assert(certify(
	    &view, &prepared_plan, &live_plan, &certificate, &summary));
	assert(summary.selected_step == 1);
	assert(summary.selected_segment == 0);
	assert(live_plan.first_pending_step == 1);
	assert(certificate.source_trigger == 0);
}

static void test_identical_state_is_history_independent(void)
{
	certifier_fixture fixture;
	level_metadata_scan_view view;
	route_planner_plan_summary prepared_plan;
	route_planner_plan_summary first_plan;
	route_planner_plan_summary second_plan;
	guidebot_route_validity_certificate first_certificate;
	guidebot_route_validity_certificate second_certificate;
	guidebot_route_certifier_summary summary;
	level_metadata_state first_state;

	initialize_fixture(&fixture);
	initialize_plan(&prepared_plan);
	view = make_view(&fixture);
	view.start_segment = 1;
	fixture.wall_open[0] = 1;
	assert(certify(
	    &view, &prepared_plan, &first_plan, &first_certificate, &summary));
	first_state = Live;
	fixture.wall_open[2] = 1;
	fixture.wall_open[2] = 0;
	assert(certify(
	    &view, &prepared_plan, &second_plan, &second_certificate, &summary));
	assert(memcmp(&first_plan, &second_plan, sizeof(first_plan)) == 0);
	assert(memcmp(
	           &first_certificate, &second_certificate,
	           sizeof(first_certificate)) == 0);
	assert(memcmp(&first_state, &Live, sizeof(first_state)) == 0);
}

static void test_disabled_action_uses_reachable_prepared_alternative(void)
{
	certifier_fixture fixture;
	level_metadata_scan_view view;
	route_planner_plan_summary prepared_plan;
	route_planner_plan_summary live_plan;
	guidebot_route_validity_certificate certificate;
	guidebot_route_certifier_summary summary;

	initialize_fixture(&fixture);
	initialize_plan(&prepared_plan);
	view = make_view(&fixture);
	view.start_segment = 1;
	fixture.wall_open[0] = 1;
	fixture.trigger_flags[1] = view.trigger_flag_disabled;
	Prepared.route_steps[3].kind = LEVEL_METADATA_ROUTE_HIDDEN_DOOR;
	Prepared.route_steps[3].activation_kind =
	    LEVEL_METADATA_ROUTE_ACTIVATION_OPEN_HIDDEN_DOOR;
	Prepared.route_steps[3].wall_num = 2;
	Prepared.route_steps[3].seg = 1;
	Prepared.route_steps[3].path_terminal_segment = 1;

	assert(certify(
	    &view, &prepared_plan, &live_plan, &certificate, &summary));
	assert(summary.rejected_actions > 0);
	assert(summary.selected_step == 3);
	assert(summary.selected_segment == 1);
	assert(live_plan.first_pending_step == 3);
	assert(certificate.status == GUIDEBOT_ROUTE_CERTIFICATE_VALID);
}

static void test_destroyed_switch_stays_complete_when_link_recloses(void)
{
	certifier_fixture fixture;
	level_metadata_scan_view view;
	route_planner_plan_summary prepared_plan;
	route_planner_plan_summary live_plan;
	guidebot_route_validity_certificate certificate;
	guidebot_route_certifier_summary summary;

	initialize_fixture(&fixture);
	initialize_plan(&prepared_plan);
	view = make_view(&fixture);
	view.start_segment = 0;
	fixture.wall_shootable[0] = 0;
	assert(!fixture.wall_open[0]);

	assert(!certify(
	    &view, &prepared_plan, &live_plan, &certificate, &summary));
	assert(summary.selected_step == -1);

	Prepared.route_steps[1].activation_kind =
	    LEVEL_METADATA_ROUTE_ACTIVATION_PASS_THROUGH_TRIGGER;
	assert(certify(
	    &view, &prepared_plan, &live_plan, &certificate, &summary));
	assert(live_plan.first_pending_step == 1);
	assert(certificate.source_trigger == 0);
}

static void test_equal_switch_positions_use_segment_center(void)
{
	certifier_fixture fixture;
	level_metadata_scan_view view;
	route_planner_plan_summary prepared_plan;
	route_planner_plan_summary live_plan;
	guidebot_route_validity_certificate certificate;
	guidebot_route_certifier_summary summary;

	initialize_fixture(&fixture);
	initialize_plan(&prepared_plan);
	view = make_view(&fixture);
	view.start_segment = 0;
	memcpy(
	    Prepared.route_steps[1].aim_pos,
	    Prepared.route_steps[1].activation_pos,
	    sizeof(Prepared.route_steps[1].aim_pos));

	assert(certify(
	    &view, &prepared_plan, &live_plan, &certificate, &summary));
	assert(live_plan.first_pending_step == 1);
	assert(Live.route_steps[1].activation_pos[0] == 100);
	assert(Live.route_steps[1].activation_pos[0] !=
	       Live.route_steps[1].aim_pos[0]);
}

static void test_solid_illusion_wall_is_not_passable(void)
{
	certifier_fixture fixture;
	level_metadata_scan_view view;

	initialize_fixture(&fixture);
	view = make_view(&fixture);
	fixture.wall_type[0] = view.wall_type_illusion;
	assert(!fixture.wall_open[0]);
	assert(!guidebot_route_side_passable_current(&view, 0, 0));
	fixture.wall_open[0] = 1;
	assert(guidebot_route_side_passable_current(&view, 0, 0));
}

static void test_keyed_buddy_proof_door_keeps_objective_reachable(void)
{
	certifier_fixture fixture;
	level_metadata_scan_view view;
	route_planner_plan_summary prepared_plan;
	route_planner_plan_summary live_plan;
	guidebot_route_validity_certificate certificate;
	guidebot_route_certifier_summary summary;

	initialize_fixture(&fixture);
	initialize_plan(&prepared_plan);
	view = make_view(&fixture);
	view.start_segment = 0;
	view.initial_key_mask = LEVEL_METADATA_KEY_MASK_BLUE;
	fixture.wall_key[0] = view.wall_key_blue;
	fixture.hard_blocked[0] = 1;
	Prepared.route_steps[1].seg = 1;
	Prepared.route_steps[1].path_terminal_segment = 1;

	assert(!guidebot_route_side_passable_current(&view, 0, 0));
	assert(guidebot_route_side_progress_reachable_current(&view, 0, 0));
	assert(certify(
	    &view, &prepared_plan, &live_plan, &certificate, &summary));
	assert(summary.selected_step == 1);
	assert(summary.selected_segment == 1);

	view.initial_key_mask = 0;
	assert(!guidebot_route_side_progress_reachable_current(&view, 0, 0));
	assert(!certify(
	    &view, &prepared_plan, &live_plan, &certificate, &summary));
	assert(summary.blocking_step == 1);
}

static void test_physical_frontier_follows_strategic_route(void)
{
	certifier_fixture fixture;
	level_metadata_scan_view view;

	initialize_fixture(&fixture);
	view = make_view(&fixture);
	view.initial_key_mask = LEVEL_METADATA_KEY_MASK_BLUE;
	fixture.wall_key[1] = view.wall_key_blue;
	fixture.hard_blocked[1] = 1;
	fixture.wall_open[0] = 1;
	fixture.wall_open[2] = 1;
	fixture.child[3][0] = -1;
	fixture.wall[3][0] = -1;
	fixture.child[4][1] = -1;
	fixture.wall[4][1] = -1;
	fixture.child[0][2] = 4;
	fixture.wall[0][2] = 3;
	fixture.child[4][3] = 0;
	fixture.wall[4][3] = 3;
	fixture.wall_open[3] = 1;

	assert(
	    guidebot_route_best_physical_frontier(
	        &view, 0, 3, 200, -1, -1, -1, -1, &Workspace) == 1);
	fixture.wall_open[1] = 1;
	assert(
	    guidebot_route_best_physical_frontier(
	        &view, 0, 3, 200, -1, -1, -1, -1, &Workspace) == 3);
	assert(
	    guidebot_route_best_physical_frontier(
	        &view, 0, 3, 200, 1, 2, -1, -1, &Workspace) == 1);
	fixture.wall_open[1] = 0;
	view.initial_key_mask = 0;
	assert(
	    guidebot_route_best_physical_frontier(
	        &view, 0, 3, 200, -1, -1, -1, -1, &Workspace) == -1);
}

static void test_exit_projection_skips_only_countdown_steps(void)
{
	certifier_fixture fixture;
	level_metadata_scan_view view;
	level_metadata_route_step selected;
	int selected_index;
	int selected_segment;

	initialize_fixture(&fixture);
	view = make_view(&fixture);
	fixture.wall_open[0] = 1;
	Prepared.route_steps[2].kind = LEVEL_METADATA_ROUTE_REACTOR;
	Prepared.route_steps[2].activation_kind =
	    LEVEL_METADATA_ROUTE_ACTIVATION_DESTROY_REACTOR;
	Prepared.route_steps[3].kind = LEVEL_METADATA_ROUTE_TRIGGER;
	Prepared.route_steps[3].activation_kind =
	    LEVEL_METADATA_ROUTE_ACTIVATION_SHOOT_SWITCH;
	Prepared.route_steps[3].trigger_num = 1;
	Prepared.route_steps[3].wall_num = 1;
	Prepared.route_steps[3].seg = 1;
	Prepared.route_steps[3].path_terminal_segment = 1;
	Prepared.route_steps[3].opened_link_count = 1;
	Prepared.route_steps[3].opened_link_wall[0] = 1;

	assert(guidebot_route_select_exit_step_current_state(
	    &view, &Prepared, &selected, &selected_index, &selected_segment));
	assert(selected_index == 3);
	assert(selected.kind == LEVEL_METADATA_ROUTE_TRIGGER);
	assert(selected_segment == 1);

	fixture.wall_open[1] = 1;
	assert(guidebot_route_select_exit_step_current_state(
	    &view, &Prepared, &selected, &selected_index, &selected_segment));
	assert(selected_index == 4);
	assert(selected.kind == LEVEL_METADATA_ROUTE_EXIT);
	assert(selected_segment == 3);

	Prepared.route_steps[2].kind = LEVEL_METADATA_ROUTE_BOSS;
	Prepared.route_steps[2].activation_kind =
	    LEVEL_METADATA_ROUTE_ACTIVATION_DESTROY_BOSS;
	assert(guidebot_route_select_exit_step_current_state(
	    &view, &Prepared, &selected, &selected_index, &selected_segment));
	assert(selected_index == 4);
}

static void test_deferred_countdown_frontier_stops_before_closed_link(void)
{
	certifier_fixture fixture;
	level_metadata_scan_view view;

	initialize_fixture(&fixture);
	view = make_view(&fixture);
	fixture.wall_open[0] = 1;
	fixture.control_center_link[1] = 1;
	fixture.wall_open[2] = 1;

	assert(
	    guidebot_route_best_physical_frontier(
	        &view, 0, 3, 200, -1, -1, -1, -1, &Workspace) == -1);
	assert(
	    guidebot_route_best_deferred_countdown_frontier(
	        &view, 0, 3, 200, -1, -1, -1, -1, &Workspace) == 1);
	view.initial_control_center_destroyed = 1;
	assert(
	    guidebot_route_best_physical_frontier(
	        &view, 0, 3, 200, -1, -1, -1, -1, &Workspace) == 3);
}

static void test_unreachable_prepared_target_requires_replan(void)
{
	certifier_fixture fixture;
	level_metadata_scan_view view;
	route_planner_plan_summary prepared_plan;
	route_planner_plan_summary live_plan;
	guidebot_route_validity_certificate certificate;
	guidebot_route_certifier_summary summary;

	initialize_fixture(&fixture);
	initialize_plan(&prepared_plan);
	view = make_view(&fixture);
	view.start_segment = 0;
	Prepared.route_steps[1].seg = 1;
	Prepared.route_steps[1].path_terminal_segment = 1;
	Prepared.route_steps[3].kind = LEVEL_METADATA_ROUTE_HIDDEN_DOOR;
	Prepared.route_steps[3].activation_kind =
	    LEVEL_METADATA_ROUTE_ACTIVATION_OPEN_HIDDEN_DOOR;
	Prepared.route_steps[3].wall_num = 2;
	Prepared.route_steps[3].seg = 0;
	Prepared.route_steps[3].path_terminal_segment = 0;
	assert(!certify(
	    &view, &prepared_plan, &live_plan, &certificate, &summary));
	assert(summary.selected_step == -1);
	assert(summary.rejected_actions > 0);
	assert(summary.blocking_step == 1);
	assert(summary.blocking_segment == 1);
	assert(
	    summary.blocking_reason ==
	    GUIDEBOT_ROUTE_CERTIFIER_REJECTION_UNREACHABLE_TARGET);
}

static void test_reclosed_hidden_door_behind_start_does_not_regress_route(void)
{
	certifier_fixture fixture;
	level_metadata_scan_view view;
	route_planner_plan_summary prepared_plan;
	route_planner_plan_summary live_plan;
	guidebot_route_validity_certificate certificate;
	guidebot_route_certifier_summary summary;

	initialize_fixture(&fixture);
	initialize_plan(&prepared_plan);
	view = make_view(&fixture);
	view.start_segment = 2;
	Prepared.route_steps[1].kind = LEVEL_METADATA_ROUTE_HIDDEN_DOOR;
	Prepared.route_steps[1].activation_kind =
	    LEVEL_METADATA_ROUTE_ACTIVATION_OPEN_HIDDEN_DOOR;
	Prepared.route_steps[1].wall_num = 1;
	Prepared.route_steps[1].seg = 1;
	Prepared.route_steps[1].path_terminal_segment = 1;
	Prepared.route_steps[2].wall_num = 2;
	Prepared.route_steps[2].opened_link_wall[0] = 2;
	Prepared.route_steps[2].seg = 2;
	Prepared.route_steps[2].path_terminal_segment = 2;

	assert(certify(
	    &view, &prepared_plan, &live_plan, &certificate, &summary));
	assert(live_plan.first_pending_step == 2);
	assert(summary.selected_segment == 2);
	assert(certificate.source_trigger == 1);
}

int main(void)
{
	test_current_start_and_accessibility_select_action();
	test_first_reachable_required_action_preserves_order();
	test_identical_state_is_history_independent();
	test_disabled_action_uses_reachable_prepared_alternative();
	test_destroyed_switch_stays_complete_when_link_recloses();
	test_equal_switch_positions_use_segment_center();
	test_solid_illusion_wall_is_not_passable();
	test_keyed_buddy_proof_door_keeps_objective_reachable();
	test_physical_frontier_follows_strategic_route();
	test_exit_projection_skips_only_countdown_steps();
	test_deferred_countdown_frontier_stops_before_closed_link();
	test_unreachable_prepared_target_requires_replan();
	test_reclosed_hidden_door_behind_start_does_not_regress_route();
	printf("guidebot route certifier tests passed\n");
	return 0;
}
