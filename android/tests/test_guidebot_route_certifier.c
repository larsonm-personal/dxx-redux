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

#define TEST_SEGMENTS 4
#define TEST_WALLS    3
#define TEST_TRIGGERS 2

typedef struct certifier_fixture {
	int child[TEST_SEGMENTS][LEVEL_METADATA_MAX_SIDES];
	int wall[TEST_SEGMENTS][LEVEL_METADATA_MAX_SIDES];
	int wall_open[TEST_WALLS];
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

static int wall_num(void *user, int segment, int side)
{
	certifier_fixture *fixture = (certifier_fixture *) user;
	return fixture->wall[segment][side];
}

static int wall_type(void *user, int wall)
{
	(void) user;
	(void) wall;
	return 1;
}

static int wall_flags(void *user, int wall)
{
	certifier_fixture *fixture = (certifier_fixture *) user;
	return fixture->wall_open[wall] ? 2 : 0;
}

static int wall_keys(void *user, int wall)
{
	(void) user;
	(void) wall;
	return 0;
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
	view.wall_num = wall_num;
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
	for (segment = 0; segment < TEST_WALLS; ++segment)
		fixture->wall_shootable[segment] = 1;
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
	assert(certify(
	    &view, &prepared_plan, &first_plan, &first_certificate, &summary));
	first_state = Live;
	fixture.wall_open[0] = 1;
	fixture.wall_open[0] = 0;
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
	fixture.trigger_flags[1] = view.trigger_flag_disabled;
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

	assert(certify(
	    &view, &prepared_plan, &live_plan, &certificate, &summary));
	assert(live_plan.first_pending_step == 2);
	assert(certificate.source_trigger == 1);

	Prepared.route_steps[1].activation_kind =
	    LEVEL_METADATA_ROUTE_ACTIVATION_PASS_THROUGH_TRIGGER;
	assert(certify(
	    &view, &prepared_plan, &live_plan, &certificate, &summary));
	assert(live_plan.first_pending_step == 1);
	assert(certificate.source_trigger == 0);
}

int main(void)
{
	test_current_start_and_accessibility_select_action();
	test_identical_state_is_history_independent();
	test_disabled_action_uses_reachable_prepared_alternative();
	test_destroyed_switch_stays_complete_when_link_recloses();
	printf("guidebot route certifier tests passed\n");
	return 0;
}
