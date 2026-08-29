#include "guidebot_route_certifier.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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

#define TEST_SEGMENTS      5
#define BENCHMARK_SEGMENTS 900
#define FIXTURE_SEGMENTS   BENCHMARK_SEGMENTS
#define TEST_WALLS         4
#define TEST_TRIGGERS      2

typedef struct certifier_fixture {
	int num_segments;
	int child[FIXTURE_SEGMENTS][LEVEL_METADATA_MAX_SIDES];
	int wall[FIXTURE_SEGMENTS][LEVEL_METADATA_MAX_SIDES];
	int wall_type[TEST_WALLS];
	int wall_open[TEST_WALLS];
	int wall_extra_flags[TEST_WALLS];
	int wall_clip[TEST_WALLS];
	int wall_key[TEST_WALLS];
	int hard_blocked[TEST_WALLS];
	int control_center_link[TEST_WALLS];
	int wall_shootable[TEST_WALLS];
	int position_sensitive_wall;
	int shootable_segment[FIXTURE_SEGMENTS];
	int potentially_shootable_segment[FIXTURE_SEGMENTS];
	int incidence_cosine[FIXTURE_SEGMENTS];
	int center_x[FIXTURE_SEGMENTS];
	int explored[FIXTURE_SEGMENTS];
	int detailed_geometry;
	int detailed_shootable_segment;
	int wall_shootable_calls;
	unsigned int segment_child_calls;
	unsigned int segment_center_calls;
	int trigger_flags[TEST_TRIGGERS];
	int object_dead;
	int object_type;
	int object_id;
	int object_segment;
} certifier_fixture;

static guidebot_route_certifier_workspace Workspace;
static level_metadata_state Prepared;
static level_metadata_state Live;

static int segment_child(void *user, int segment, int side)
{
	certifier_fixture *fixture = (certifier_fixture *) user;
	fixture->segment_child_calls++;
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
	return (fixture->wall_open[wall] ? 2 : 0) |
	       fixture->wall_extra_flags[wall];
}

static int wall_keys(void *user, int wall)
{
	certifier_fixture *fixture = (certifier_fixture *) user;
	return fixture->wall_key[wall];
}

static int wall_clip_flags(void *user, int wall)
{
	certifier_fixture *fixture = (certifier_fixture *) user;
	return fixture->wall_clip[wall];
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
	certifier_fixture *fixture = (certifier_fixture *) user;
	(void) object;
	return fixture->object_type;
}

static int object_id(void *user, int object)
{
	certifier_fixture *fixture = (certifier_fixture *) user;
	(void) object;
	return fixture->object_id;
}

static int object_flags(void *user, int object)
{
	certifier_fixture *fixture = (certifier_fixture *) user;
	(void) object;
	return fixture->object_dead ? 1 : 0;
}

static int object_segment(void *user, int object)
{
	certifier_fixture *fixture = (certifier_fixture *) user;
	(void) object;
	return fixture->object_segment;
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
	certifier_fixture *fixture = (certifier_fixture *) user;
	fixture->segment_center_calls++;
	xyz[0] = fixture->center_x[segment];
	xyz[1] = 0;
	xyz[2] = 0;
	return 1;
}

static int side_center(void *user, int segment, int side, int xyz[3])
{
	certifier_fixture *fixture = (certifier_fixture *) user;

	if (!fixture->detailed_geometry)
		return 0;
	xyz[0] = fixture->center_x[segment];
	xyz[1] = (side + 1) * 10;
	xyz[2] = 0;
	return 1;
}

static int segment_vertex(void *user, int segment, int vertex, int xyz[3])
{
	certifier_fixture *fixture = (certifier_fixture *) user;

	if (!fixture->detailed_geometry)
		return 0;
	xyz[0] = fixture->center_x[segment];
	xyz[1] = (vertex & 1) ? 10 : -10;
	xyz[2] = (vertex & 2) ? 10 : -10;
	return 1;
}

static int segment_is_explored(void *user, int segment)
{
	certifier_fixture *fixture = (certifier_fixture *) user;

	return fixture->explored[segment];
}

static int wall_shootable_from_position(
    void *user, int segment, const int from_pos[3], int wall)
{
	certifier_fixture *fixture = (certifier_fixture *) user;
	(void) from_pos;
	fixture->wall_shootable_calls++;
	if (wall == fixture->position_sensitive_wall) {
		if (segment == fixture->detailed_shootable_segment &&
		    from_pos[1] != 0)
			return 1;
		return fixture->shootable_segment[segment];
	}
	return fixture->wall_shootable[wall];
}

static int wall_potentially_shootable_from_position(
    void *user, int segment, const int from_pos[3], int wall)
{
	certifier_fixture *fixture = (certifier_fixture *) user;
	(void) from_pos;
	fixture->wall_shootable_calls++;
	if (wall == fixture->position_sensitive_wall)
		return fixture->potentially_shootable_segment[segment];
	return fixture->wall_shootable[wall];
}

static int wall_shot_incidence_cosine(
    void *user, const int from_pos[3], int wall)
{
	certifier_fixture *fixture = (certifier_fixture *) user;
	int segment;
	(void) wall;

	for (segment = 0; segment < fixture->num_segments; ++segment)
		if (fixture->center_x[segment] == from_pos[0])
			return fixture->incidence_cosine[segment];
	return LEVEL_METADATA_SHOT_COSINE_ONE;
}

static level_metadata_scan_view make_view(certifier_fixture *fixture)
{
	level_metadata_scan_view view;

	memset(&view, 0, sizeof(view));
	view.user = fixture;
	view.num_segments = fixture->num_segments;
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
	view.powerup_key_blue = 10;
	view.powerup_key_red = 20;
	view.powerup_key_gold = 30;
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
	view.object_id = object_id;
	view.object_flags = object_flags;
	view.object_segment = object_segment;
	view.object_position = object_position;
	view.segment_center = segment_center;
	view.side_center = side_center;
	view.segment_vertex = segment_vertex;
	view.segment_is_explored = segment_is_explored;
	view.wall_shootable_from_position = wall_shootable_from_position;
	view.wall_potentially_shootable_from_position =
	    wall_potentially_shootable_from_position;
	view.wall_shot_incidence_cosine = wall_shot_incidence_cosine;
	return view;
}

static void initialize_fixture(certifier_fixture *fixture)
{
	int segment;
	int side;

	memset(fixture, 0, sizeof(*fixture));
	fixture->num_segments = TEST_SEGMENTS;
	fixture->object_type = 3;
	fixture->object_segment = 2;
	fixture->position_sensitive_wall = -1;
	fixture->detailed_shootable_segment = -1;
	for (segment = 0; segment < TEST_SEGMENTS; ++segment) {
		fixture->center_x[segment] = 100 + segment;
		fixture->incidence_cosine[segment] =
		    LEVEL_METADATA_SHOT_COSINE_ONE;
	}
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
	memset(&Workspace, 0, sizeof(Workspace));
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

static int select_compiled(
    level_metadata_scan_view *view,
    const route_planner_plan_summary *compiled_plan,
    route_planner_plan_summary *live_plan,
    guidebot_route_validity_certificate *certificate,
    guidebot_route_certifier_summary *summary)
{
	return guidebot_route_select_compiled_current_state(
	    view, &Prepared, compiled_plan, &Live, live_plan, certificate,
	    summary);
}

static void test_compiled_selector_never_restores_collected_key(void)
{
	certifier_fixture fixture;
	level_metadata_scan_view view;
	route_planner_plan_summary compiled_plan;
	route_planner_plan_summary live_plan;
	guidebot_route_validity_certificate certificate;
	guidebot_route_certifier_summary summary;

	initialize_fixture(&fixture);
	initialize_plan(&compiled_plan);
	view = make_view(&fixture);
	view.initial_key_mask = LEVEL_METADATA_KEY_MASK_GOLD;
	Prepared.route_steps[1].kind = LEVEL_METADATA_ROUTE_KEY;
	Prepared.route_steps[1].activation_kind =
	    LEVEL_METADATA_ROUTE_ACTIVATION_PICKUP_KEY;
	Prepared.route_steps[1].key_index = 2;
	Prepared.route_steps[1].seg = 1;
	Prepared.route_steps[1].path_terminal_segment = 1;
	Prepared.route_steps[2].seg = 4;
	Prepared.route_steps[2].path_terminal_segment = 4;
	fixture.segment_child_calls = 0;

	assert(select_compiled(
	    &view, &compiled_plan, &live_plan, &certificate, &summary));
	assert(summary.selected_step == 2);
	assert(summary.selected_segment == 4);
	assert(live_plan.first_pending_step == 2);
	assert(certificate.source_trigger == 1);
	assert(fixture.segment_child_calls == 0);
}

static void test_compiled_selector_rebinds_moving_key_object(void)
{
	certifier_fixture fixture;
	level_metadata_scan_view view;
	route_planner_plan_summary compiled_plan;
	route_planner_plan_summary live_plan;
	guidebot_route_validity_certificate certificate;
	guidebot_route_certifier_summary summary;

	initialize_fixture(&fixture);
	initialize_plan(&compiled_plan);
	view = make_view(&fixture);
	fixture.object_type = view.obj_type_powerup;
	fixture.object_id = view.powerup_key_gold;
	fixture.object_segment = 4;
	Prepared.route_steps[1].kind = LEVEL_METADATA_ROUTE_KEY;
	Prepared.route_steps[1].activation_kind =
	    LEVEL_METADATA_ROUTE_ACTIVATION_PICKUP_KEY;
	Prepared.route_steps[1].key_index = 2;
	Prepared.route_steps[1].seg = 1;
	Prepared.route_steps[1].path_terminal_segment = 1;

	assert(select_compiled(
	    &view, &compiled_plan, &live_plan, &certificate, &summary));
	assert(summary.selected_step == 1);
	assert(summary.selected_segment == 4);
	assert(Live.route_steps[1].seg == 4);
	assert(live_plan.first_pending_path_terminal_segment == 4);
}

static void test_compiled_selector_chooses_reachable_switch_guidance(void)
{
	certifier_fixture fixture;
	level_metadata_scan_view view;
	route_planner_plan_summary compiled_plan;
	route_planner_plan_summary live_plan;
	guidebot_route_validity_certificate certificate;
	guidebot_route_certifier_summary summary;
	level_metadata_route_step *step;
	int wall;

	initialize_fixture(&fixture);
	initialize_plan(&compiled_plan);
	view = make_view(&fixture);
	view.start_segment = 0;
	for (wall = 0; wall < TEST_WALLS; ++wall)
		fixture.wall_open[wall] = 1;
	step = &Prepared.route_steps[1];
	step->opened_link_count = 0;
	step->switch_guidance_candidate_count = 3;
	step->switch_guidance_candidate_seg[0] = 4;
	step->switch_guidance_candidate_quality[0] =
	    LEVEL_METADATA_SWITCH_SHOT_CONFIRMED_STEEP;
	step->switch_guidance_candidate_incidence[0] = 10000;
	step->switch_guidance_candidate_pos[0][0] = 40;
	step->switch_guidance_candidate_seg[1] = 2;
	step->switch_guidance_candidate_quality[1] =
	    LEVEL_METADATA_SWITCH_SHOT_CONFIRMED;
	step->switch_guidance_candidate_incidence[1] =
	    LEVEL_METADATA_SHOT_COSINE_ONE;
	step->switch_guidance_candidate_pos[1][0] = 20;
	step->switch_guidance_candidate_seg[2] = 99;
	step->switch_guidance_candidate_quality[2] =
	    LEVEL_METADATA_SWITCH_SHOT_CONFIRMED;
	step->switch_guidance_candidate_incidence[2] =
	    LEVEL_METADATA_SHOT_COSINE_ONE;

	assert(select_compiled(
	    &view, &compiled_plan, &live_plan, &certificate, &summary));
	assert(summary.selected_step == 1);
	assert(summary.selected_segment == 2);
	assert(summary.reranked_firing_position);
	assert(summary.visited_segments == TEST_SEGMENTS);
	assert(Live.route_steps[1].seg == 2);
	assert(Live.route_steps[1].activation_pos[0] == 20);
	assert(Live.route_steps[1].switch_shot_quality ==
	       LEVEL_METADATA_SWITCH_SHOT_CONFIRMED);
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
	assert(summary.rejected_actions == 0);
	assert(summary.evaluated_actions == 1);
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
	assert(summary.selected_step == 2);

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

static void test_remote_cached_switch_position_reranks_near_switch(void)
{
	certifier_fixture fixture;
	level_metadata_scan_view view;
	route_planner_plan_summary prepared_plan;
	route_planner_plan_summary live_plan;
	guidebot_route_validity_certificate certificate;
	guidebot_route_certifier_summary summary;
	int first_call_count;

	initialize_fixture(&fixture);
	initialize_plan(&prepared_plan);
	view = make_view(&fixture);
	view.start_segment = 0;
	fixture.position_sensitive_wall = 0;
	fixture.shootable_segment[0] = 1;
	fixture.shootable_segment[3] = 1;
	fixture.center_x[0] = 100;
	fixture.center_x[1] = 200;
	fixture.center_x[2] = 300;
	fixture.center_x[3] = 390;
	Prepared.route_steps[1].activation_pos[0] = 100;
	Prepared.route_steps[1].aim_pos[0] = 400;
	fixture.child[0][2] = 1;
	fixture.child[1][2] = 0;
	fixture.child[1][3] = 2;
	fixture.child[2][3] = 1;
	fixture.child[2][4] = 3;
	fixture.child[3][4] = 2;

	assert(certify(
	    &view, &prepared_plan, &live_plan, &certificate, &summary));
	assert(summary.selected_step == 1);
	assert(summary.selected_segment == 3);
	assert(summary.reranked_firing_position);
	assert(!summary.firing_cache_hit);
	assert(summary.evaluated_firing_positions > 0);
	assert(summary.evaluated_firing_positions <= TEST_SEGMENTS + 1);
	assert(Live.route_steps[1].seg == 3);
	assert(Live.route_steps[1].activation_pos[0] == 390);
	assert(live_plan.first_pending_path_terminal_segment == 3);
	first_call_count = fixture.wall_shootable_calls;

	assert(certify(
	    &view, &prepared_plan, &live_plan, &certificate, &summary));
	assert(summary.selected_segment == 3);
	assert(summary.firing_cache_hit);
	assert(summary.evaluated_firing_positions == 0);
	assert(fixture.wall_shootable_calls - first_call_count == 1);
}

static void test_budgeted_certification_resumes_without_repeating_candidates(void)
{
	certifier_fixture fixture;
	level_metadata_scan_view view;
	route_planner_plan_summary prepared_plan;
	route_planner_plan_summary live_plan;
	guidebot_route_validity_certificate certificate;
	guidebot_route_certifier_summary summary;
	guidebot_route_certifier_budget budget;
	int pending_ticks = 0;
	int result;

	initialize_fixture(&fixture);
	initialize_plan(&prepared_plan);
	view = make_view(&fixture);
	view.start_segment = 0;
	fixture.position_sensitive_wall = 0;
	fixture.shootable_segment[0] = 1;
	fixture.shootable_segment[3] = 1;
	fixture.center_x[0] = 100;
	fixture.center_x[1] = 200;
	fixture.center_x[2] = 300;
	fixture.center_x[3] = 390;
	Prepared.route_steps[1].activation_pos[0] = 100;
	Prepared.route_steps[1].aim_pos[0] = 400;
	fixture.child[0][2] = 1;
	fixture.child[1][2] = 0;
	fixture.child[1][3] = 2;
	fixture.child[2][3] = 1;
	fixture.child[2][4] = 3;
	fixture.child[3][4] = 2;
	memset(&budget, 0, sizeof(budget));
	budget.work_limit = 1;
	guidebot_route_certifier_reset_job(&Workspace);
	do {
		int calls_before = fixture.wall_shootable_calls;

		result = guidebot_route_certify_current_state_budgeted(
		    &view, &Prepared, &prepared_plan, &Workspace, &Live,
		    &live_plan, &certificate, &summary, &budget);
		assert(fixture.wall_shootable_calls - calls_before <= 1);
		if (result == GUIDEBOT_ROUTE_CERTIFIER_PENDING)
			pending_ticks++;
	} while (result == GUIDEBOT_ROUTE_CERTIFIER_PENDING &&
	         pending_ticks < 700);
	assert(result == GUIDEBOT_ROUTE_CERTIFIER_VALID);
	assert(pending_ticks > 1);
	assert(summary.selected_segment == 3);
	assert(summary.evaluated_firing_positions > 0);
	assert(summary.evaluated_firing_positions <= TEST_SEGMENTS + 1);
}

static void test_budgeted_certification_keeps_start_while_companion_moves(void)
{
	certifier_fixture fixture;
	guidebot_route_certifier_budget budget;
	guidebot_route_validity_certificate certificate;
	guidebot_route_certifier_summary summary;
	level_metadata_scan_view view;
	route_planner_plan_summary live_plan;
	route_planner_plan_summary prepared_plan;
	int pending_ticks = 0;
	int result;

	initialize_fixture(&fixture);
	initialize_plan(&prepared_plan);
	view = make_view(&fixture);
	view.start_segment = 0;
	memset(&budget, 0, sizeof(budget));
	budget.work_limit = 1;
	guidebot_route_certifier_reset_job(&Workspace);
	do {
		result = guidebot_route_certify_current_state_budgeted(
		    &view, &Prepared, &prepared_plan, &Workspace, &Live,
		    &live_plan, &certificate, &summary, &budget);
		if (result == GUIDEBOT_ROUTE_CERTIFIER_PENDING) {
			pending_ticks++;
			view.start_segment = view.start_segment ? 0 : 1;
			assert(Workspace.job_start_segment == 0);
		}
	} while (result == GUIDEBOT_ROUTE_CERTIFIER_PENDING &&
	         pending_ticks < 300);
	assert(result == GUIDEBOT_ROUTE_CERTIFIER_VALID);
	assert(pending_ticks > 1);
}

static void test_budgeted_certification_skips_trigger_completed_mid_scan(void)
{
	certifier_fixture fixture;
	guidebot_route_certifier_budget budget;
	guidebot_route_validity_certificate certificate;
	guidebot_route_certifier_summary summary;
	level_metadata_scan_view view;
	route_planner_plan_summary live_plan;
	route_planner_plan_summary prepared_plan;
	int pending_ticks = 0;
	int result;

	initialize_fixture(&fixture);
	initialize_plan(&prepared_plan);
	view = make_view(&fixture);
	view.start_segment = 0;
	fixture.position_sensitive_wall = 0;
	fixture.shootable_segment[0] = 1;
	memset(&budget, 0, sizeof(budget));
	budget.work_limit = 1;
	guidebot_route_certifier_reset_job(&Workspace);
	result = guidebot_route_certify_current_state_budgeted(
	    &view, &Prepared, &prepared_plan, &Workspace, &Live, &live_plan,
	    &certificate, &summary, &budget);
	assert(result == GUIDEBOT_ROUTE_CERTIFIER_PENDING);
	fixture.wall_open[0] = 1;
	do {
		result = guidebot_route_certify_current_state_budgeted(
		    &view, &Prepared, &prepared_plan, &Workspace, &Live,
		    &live_plan, &certificate, &summary, &budget);
		if (result == GUIDEBOT_ROUTE_CERTIFIER_PENDING)
			pending_ticks++;
	} while (result == GUIDEBOT_ROUTE_CERTIFIER_PENDING &&
	         pending_ticks < 300);
	assert(result == GUIDEBOT_ROUTE_CERTIFIER_VALID);
	assert(summary.selected_step == 2);
	assert(live_plan.first_pending_step == 2);
}

static void test_budgeted_unexplored_scan_publishes_complete_component(void)
{
	certifier_fixture fixture;
	guidebot_route_certifier_budget budget;
	level_metadata_scan_view view;
	level_metadata_unexplored_route result;
	int pending_ticks = 0;
	int scan_result;
	int wall;

	initialize_fixture(&fixture);
	view = make_view(&fixture);
	view.start_segment = 0;
	fixture.explored[0] = 1;
	fixture.child[2][0] = -1;
	fixture.child[3][1] = -1;
	for (wall = 0; wall < TEST_WALLS; ++wall)
		fixture.wall_open[wall] = 1;
	memset(&budget, 0, sizeof(budget));
	budget.work_limit = 1;
	memset(&result, 0, sizeof(result));
	result.target_seg = -1;
	result.waypoint_seg = -1;
	guidebot_route_certifier_reset_job(&Workspace);
	do {
		scan_result = guidebot_route_find_unexplored_budgeted(
		    &view, &Workspace, &result, &budget);
		if (scan_result == GUIDEBOT_ROUTE_CERTIFIER_PENDING) {
			assert(result.target_seg == -1);
			pending_ticks++;
		}
	} while (scan_result == GUIDEBOT_ROUTE_CERTIFIER_PENDING &&
	         pending_ticks < 200);
	assert(scan_result == GUIDEBOT_ROUTE_CERTIFIER_VALID);
	assert(pending_ticks > 1);
	assert(result.component_size == 2);
	assert(result.target_seg == 1);
	assert(result.waypoint_seg == 1);
	assert(result.direct_reachable);
}

static void test_unexplored_scan_prefers_larger_locked_component(void)
{
	certifier_fixture fixture;
	guidebot_route_certifier_budget budget;
	level_metadata_scan_view view;
	level_metadata_unexplored_route result;
	int scan_result;

	initialize_fixture(&fixture);
	view = make_view(&fixture);
	view.start_segment = 0;
	fixture.explored[0] = 1;
	fixture.child[1][0] = -1;
	fixture.child[2][1] = -1;
	fixture.wall_open[0] = 1;
	memset(&budget, 0, sizeof(budget));
	budget.work_limit = 2;
	memset(&result, 0, sizeof(result));
	result.target_seg = -1;
	result.waypoint_seg = -1;
	guidebot_route_certifier_reset_job(&Workspace);
	do {
		scan_result = guidebot_route_find_unexplored_budgeted(
		    &view, &Workspace, &result, &budget);
	} while (scan_result == GUIDEBOT_ROUTE_CERTIFIER_PENDING);
	assert(scan_result == GUIDEBOT_ROUTE_CERTIFIER_VALID);
	assert(result.component_size == 3);
	assert(result.target_seg == 2);
	assert(result.waypoint_seg == 2);
	assert(!result.direct_reachable);
}

static void test_rejected_confirmed_switch_degrades_to_approximate_warning(void)
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
	fixture.position_sensitive_wall = 0;
	memset(fixture.shootable_segment, 0, sizeof(fixture.shootable_segment));
	fixture.potentially_shootable_segment[0] = 1;
	fixture.potentially_shootable_segment[3] = 1;
	fixture.center_x[0] = 100;
	fixture.center_x[1] = 200;
	fixture.center_x[2] = 300;
	fixture.center_x[3] = 390;
	Prepared.route_steps[1].activation_pos[0] = 100;
	Prepared.route_steps[1].aim_pos[0] = 400;
	Prepared.route_steps[1].switch_shot_quality =
	    LEVEL_METADATA_SWITCH_SHOT_CONFIRMED;
	fixture.child[0][2] = 1;
	fixture.child[1][2] = 0;
	fixture.child[1][3] = 2;
	fixture.child[2][3] = 1;
	fixture.child[2][4] = 3;
	fixture.child[3][4] = 2;

	assert(certify(
	    &view, &prepared_plan, &live_plan, &certificate, &summary));
	assert(summary.selected_segment == 3);
	assert(summary.approximate_firing_position);
	assert(!summary.steep_firing_position);
	assert(Live.route_steps[1].switch_shot_quality ==
	       LEVEL_METADATA_SWITCH_SHOT_APPROXIMATE);
	assert(Live.route_steps[1].activation_pos[0] == 390);
}

static void test_steep_switch_position_loses_to_square_shot(void)
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
	fixture.position_sensitive_wall = 0;
	fixture.shootable_segment[0] = 1;
	fixture.shootable_segment[3] = 1;
	fixture.center_x[0] = 250;
	fixture.center_x[1] = 200;
	fixture.center_x[2] = 300;
	fixture.center_x[3] = 390;
	fixture.incidence_cosine[3] = 1000;
	Prepared.route_steps[1].activation_pos[0] = 390;
	Prepared.route_steps[1].seg = 3;
	Prepared.route_steps[1].path_terminal_segment = 3;
	Prepared.route_steps[1].aim_pos[0] = 400;
	fixture.child[0][2] = 1;
	fixture.child[1][2] = 0;
	fixture.child[1][3] = 2;
	fixture.child[2][3] = 1;
	fixture.child[2][4] = 3;
	fixture.child[3][4] = 2;

	assert(certify(
	    &view, &prepared_plan, &live_plan, &certificate, &summary));
	assert(summary.selected_segment == 0);
	assert(!summary.steep_firing_position);
	assert(Live.route_steps[1].switch_shot_quality ==
	       LEVEL_METADATA_SWITCH_SHOT_CONFIRMED);
}

static void test_nearby_detailed_switch_pose_avoids_minewide_scan(void)
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
	fixture.position_sensitive_wall = 0;
	fixture.detailed_geometry = 1;
	fixture.detailed_shootable_segment = 3;
	fixture.shootable_segment[0] = 1;
	fixture.center_x[0] = 100;
	fixture.center_x[1] = 200;
	fixture.center_x[2] = 300;
	fixture.center_x[3] = 390;
	Prepared.route_steps[1].aim_pos[0] = 400;
	fixture.child[0][2] = 1;
	fixture.child[1][2] = 0;
	fixture.child[1][3] = 2;
	fixture.child[2][3] = 1;
	fixture.child[2][4] = 3;
	fixture.child[3][4] = 2;

	assert(certify(
	    &view, &prepared_plan, &live_plan, &certificate, &summary));
	assert(summary.selected_segment == 3);
	assert(summary.evaluated_firing_positions == 2);
	assert(fixture.wall_shootable_calls == 2);
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

static void test_visible_unlocked_triggered_door_is_physically_passable(void)
{
	certifier_fixture fixture;
	level_metadata_scan_view view;

	initialize_fixture(&fixture);
	view = make_view(&fixture);
	assert(fixture.wall_type[0] == view.wall_type_door);
	assert(fixture.wall_key[0] == view.wall_key_none);
	assert(triggered_side_opener_count(&fixture, 0, 0) > 0);
	assert(guidebot_route_side_passable_current(&view, 0, 0));
	fixture.wall_extra_flags[0] = view.wall_flag_door_locked;
	assert(!guidebot_route_side_passable_current(&view, 0, 0));
	fixture.wall_extra_flags[0] = 0;
	fixture.wall_clip[0] = view.wall_clip_hidden;
	assert(!guidebot_route_side_passable_current(&view, 0, 0));
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
	fixture.position_sensitive_wall = 0;
	fixture.shootable_segment[1] = 1;
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

static void test_keyed_door_blocks_objective_route_but_not_player_progress(void)
{
	certifier_fixture fixture;
	level_metadata_scan_view view;

	initialize_fixture(&fixture);
	view = make_view(&fixture);
	view.initial_key_mask = LEVEL_METADATA_KEY_MASK_BLUE;
	fixture.wall_key[0] = view.wall_key_blue;
	assert(!fixture.hard_blocked[0]);
	assert(!guidebot_route_side_passable_current(&view, 0, 0));
	assert(guidebot_route_side_progress_reachable_current(&view, 0, 0));
	assert(guidebot_route_segment_has_player_openable_keyed_door(&view, 0));
	fixture.wall_open[0] = 1;
	assert(guidebot_route_side_passable_current(&view, 0, 0));
}

static void test_reverse_side_keyed_buddy_proof_door_is_player_reachable(void)
{
	certifier_fixture fixture;
	level_metadata_scan_view view;

	initialize_fixture(&fixture);
	view = make_view(&fixture);
	view.initial_key_mask = LEVEL_METADATA_KEY_MASK_BLUE;
	fixture.wall[1][1] = 1;
	fixture.wall_key[1] = view.wall_key_blue;
	fixture.hard_blocked[1] = 1;

	assert(!guidebot_route_side_passable_current(&view, 0, 0));
	assert(guidebot_route_side_progress_reachable_current(&view, 0, 0));
	assert(guidebot_route_segment_has_player_openable_keyed_door(&view, 0));
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
	assert(guidebot_route_segment_has_player_openable_keyed_door(&view, 1));
	fixture.wall_open[1] = 1;
	assert(!guidebot_route_segment_has_player_openable_keyed_door(&view, 1));
	assert(
	    guidebot_route_best_physical_frontier(
	        &view, 0, 3, 200, -1, -1, -1, -1, &Workspace) == 3);
	assert(
	    guidebot_route_best_physical_frontier(
	        &view, 0, 3, 200, 1, 2, -1, -1, &Workspace) == 1);
	fixture.wall_open[1] = 0;
	view.initial_key_mask = 0;
	assert(!guidebot_route_segment_has_player_openable_keyed_door(&view, 1));
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

static void test_physical_frontier_can_plan_toward_triggered_link(void)
{
	certifier_fixture fixture;
	level_metadata_scan_view view;

	initialize_fixture(&fixture);
	view = make_view(&fixture);
	assert(
	    guidebot_route_best_physical_frontier(
	        &view, 0, 3, 200, -1, -1, -1, -1, &Workspace) == 3);
	fixture.wall_open[0] = 1;
	assert(
	    guidebot_route_best_physical_frontier(
	        &view, 0, 3, 200, -1, -1, -1, -1, &Workspace) == 3);
}

static void test_unreachable_switch_uses_physical_frontier(void)
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
	fixture.position_sensitive_wall = 0;
	fixture.shootable_segment[1] = 1;
	Prepared.route_steps[1].seg = 1;
	Prepared.route_steps[1].path_terminal_segment = 1;
	Prepared.route_steps[3].kind = LEVEL_METADATA_ROUTE_HIDDEN_DOOR;
	Prepared.route_steps[3].activation_kind =
	    LEVEL_METADATA_ROUTE_ACTIVATION_OPEN_HIDDEN_DOOR;
	Prepared.route_steps[3].wall_num = 2;
	Prepared.route_steps[3].seg = 0;
	Prepared.route_steps[3].path_terminal_segment = 0;
	assert(certify(
	    &view, &prepared_plan, &live_plan, &certificate, &summary));
	assert(summary.selected_step == 1);
	assert(summary.selected_segment == 0);
	assert(summary.approximate_firing_position);
	assert(Live.route_steps[1].seg == 0);
	assert(live_plan.first_pending_path_terminal_segment == 0);
}

static void test_switch_frontier_prefers_square_nearby_approach(void)
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
	fixture.position_sensitive_wall = 0;
	fixture.wall_open[0] = 1;
	fixture.wall_open[2] = 1;
	fixture.incidence_cosine[0] = LEVEL_METADATA_SHOT_COSINE_ONE;
	fixture.incidence_cosine[1] = 1000;
	Prepared.route_steps[1].opened_link_count = 0;
	Prepared.route_steps[1].seg = 3;
	Prepared.route_steps[1].path_terminal_segment = 3;
	assert(certify(
	    &view, &prepared_plan, &live_plan, &certificate, &summary));
	assert(summary.selected_step == 1);
	assert(summary.selected_segment == 0);
	assert(summary.approximate_firing_position);
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

typedef struct certifier_benchmark_result {
	unsigned int slices;
	unsigned int evaluated_edges;
	unsigned int evaluated_firing_positions;
	unsigned int wall_shootable_calls;
	unsigned int segment_child_calls;
	unsigned int segment_center_calls;
	unsigned int max_callbacks_per_slice;
	double wall_us;
} certifier_benchmark_result;

static double benchmark_wall_us(void)
{
	struct timespec now;

	timespec_get(&now, TIME_UTC);
	return (double) now.tv_sec * 1000000.0 +
	       (double) now.tv_nsec / 1000.0;
}

static void initialize_benchmark_fixture(certifier_fixture *fixture)
{
	int segment;
	int side;

	initialize_fixture(fixture);
	fixture->num_segments = BENCHMARK_SEGMENTS;
	fixture->detailed_geometry = 1;
	for (segment = 0; segment < BENCHMARK_SEGMENTS; ++segment) {
		fixture->center_x[segment] = segment * 100;
		fixture->incidence_cosine[segment] =
		    LEVEL_METADATA_SHOT_COSINE_ONE;
		fixture->explored[segment] = segment < BENCHMARK_SEGMENTS / 2;
		for (side = 0; side < LEVEL_METADATA_MAX_SIDES; ++side) {
			fixture->child[segment][side] = -1;
			fixture->wall[segment][side] = -1;
		}
		if (segment + 1 < BENCHMARK_SEGMENTS)
			fixture->child[segment][0] = segment + 1;
		if (segment > 0)
			fixture->child[segment][1] = segment - 1;
	}
}

static void initialize_benchmark_plan(
    route_planner_plan_summary *plan, int shoot_switch)
{
	initialize_plan(plan);
	Prepared.route_step_count = 2;
	Prepared.route_steps[1].seg = BENCHMARK_SEGMENTS - 1;
	Prepared.route_steps[1].path_terminal_segment = BENCHMARK_SEGMENTS - 1;
	Prepared.route_steps[1].path_segment_count = BENCHMARK_SEGMENTS;
	Prepared.route_steps[1].activation_pos[0] =
	    (BENCHMARK_SEGMENTS - 1) * 100;
	Prepared.route_steps[1].aim_pos[0] = BENCHMARK_SEGMENTS * 100;
	Prepared.route_steps[1].opened_link_count = 0;
	if (!shoot_switch) {
		Prepared.route_steps[1].kind = LEVEL_METADATA_ROUTE_REACTOR;
		Prepared.route_steps[1].activation_kind =
		    LEVEL_METADATA_ROUTE_ACTIVATION_DESTROY_REACTOR;
		Prepared.route_steps[1].wall_num = -1;
		Prepared.route_steps[1].trigger_num = -1;
	}
	plan->route_step_count = Prepared.route_step_count;
	plan->first_pending_path_segment_count = BENCHMARK_SEGMENTS;
	plan->first_pending_path_terminal_segment = BENCHMARK_SEGMENTS - 1;
}

static certifier_benchmark_result run_certifier_benchmark_once(
    int shoot_switch, int detailed_success, int blocked_frontier,
    unsigned int work_limit)
{
	certifier_fixture fixture;
	guidebot_route_certifier_budget budget;
	guidebot_route_validity_certificate certificate;
	guidebot_route_certifier_summary summary;
	level_metadata_scan_view view;
	route_planner_plan_summary live_plan;
	route_planner_plan_summary prepared_plan;
	certifier_benchmark_result metrics;
	double started;
	int result;

	memset(&metrics, 0, sizeof(metrics));
	initialize_benchmark_fixture(&fixture);
	initialize_benchmark_plan(&prepared_plan, shoot_switch);
	view = make_view(&fixture);
	view.start_segment = 0;
	fixture.position_sensitive_wall = 0;
	fixture.detailed_shootable_segment =
	    detailed_success ? BENCHMARK_SEGMENTS - 1 : -1;
	if (blocked_frontier) {
		const int segment = BENCHMARK_SEGMENTS / 2 - 1;
		fixture.wall[segment][0] = 0;
		fixture.wall[segment + 1][1] = 0;
	}
	memset(&budget, 0, sizeof(budget));
	budget.work_limit = work_limit;
	guidebot_route_certifier_reset_job(&Workspace);
	started = benchmark_wall_us();
	do {
		const unsigned int callbacks_before =
		    fixture.segment_child_calls + fixture.segment_center_calls +
		    (unsigned int) fixture.wall_shootable_calls;
		unsigned int callbacks;

		result = guidebot_route_certify_current_state_budgeted(
		    &view, &Prepared, &prepared_plan, &Workspace, &Live, &live_plan,
		    &certificate, &summary, work_limit ? &budget : NULL);
		metrics.slices++;
		callbacks = fixture.segment_child_calls +
		            fixture.segment_center_calls +
		            (unsigned int) fixture.wall_shootable_calls -
		            callbacks_before;
		if (callbacks > metrics.max_callbacks_per_slice)
			metrics.max_callbacks_per_slice = callbacks;
	} while (result == GUIDEBOT_ROUTE_CERTIFIER_PENDING &&
	         metrics.slices < 10000);
	metrics.wall_us = benchmark_wall_us() - started;
	assert(result == GUIDEBOT_ROUTE_CERTIFIER_VALID);
	metrics.evaluated_edges = summary.evaluated_edges;
	metrics.evaluated_firing_positions =
	    summary.evaluated_firing_positions;
	metrics.wall_shootable_calls = fixture.wall_shootable_calls;
	metrics.segment_child_calls = fixture.segment_child_calls;
	metrics.segment_center_calls = fixture.segment_center_calls;
	return metrics;
}

static certifier_benchmark_result run_certifier_benchmark(
    int shoot_switch, int detailed_success, int blocked_frontier,
    unsigned int work_limit)
{
	certifier_benchmark_result metrics;
	double total_us = 0.0;
	int iteration;

	for (iteration = 0; iteration < 100; ++iteration) {
		metrics = run_certifier_benchmark_once(
		    shoot_switch, detailed_success, blocked_frontier, work_limit);
		total_us += metrics.wall_us;
	}
	metrics.wall_us = total_us / 100.0;
	return metrics;
}

static certifier_benchmark_result run_cached_certifier_benchmark_once(void)
{
	certifier_fixture fixture;
	guidebot_route_certifier_budget budget;
	guidebot_route_validity_certificate certificate;
	guidebot_route_certifier_summary summary;
	level_metadata_scan_view view;
	route_planner_plan_summary live_plan;
	route_planner_plan_summary prepared_plan;
	certifier_benchmark_result metrics;
	double started;
	int result;

	memset(&metrics, 0, sizeof(metrics));
	initialize_benchmark_fixture(&fixture);
	initialize_benchmark_plan(&prepared_plan, 1);
	view = make_view(&fixture);
	view.start_segment = 0;
	fixture.position_sensitive_wall = 0;
	fixture.detailed_shootable_segment = BENCHMARK_SEGMENTS - 1;
	assert(certify(
	    &view, &prepared_plan, &live_plan, &certificate, &summary));
	fixture.wall_shootable_calls = 0;
	fixture.segment_child_calls = 0;
	fixture.segment_center_calls = 0;
	memset(&budget, 0, sizeof(budget));
	budget.work_limit = 512;
	started = benchmark_wall_us();
	do {
		const unsigned int callbacks_before =
		    fixture.segment_child_calls + fixture.segment_center_calls +
		    (unsigned int) fixture.wall_shootable_calls;
		unsigned int callbacks;

		result = guidebot_route_certify_current_state_budgeted(
		    &view, &Prepared, &prepared_plan, &Workspace, &Live, &live_plan,
		    &certificate, &summary, &budget);
		metrics.slices++;
		callbacks = fixture.segment_child_calls +
		            fixture.segment_center_calls +
		            (unsigned int) fixture.wall_shootable_calls -
		            callbacks_before;
		if (callbacks > metrics.max_callbacks_per_slice)
			metrics.max_callbacks_per_slice = callbacks;
	} while (result == GUIDEBOT_ROUTE_CERTIFIER_PENDING);
	metrics.wall_us = benchmark_wall_us() - started;
	assert(result == GUIDEBOT_ROUTE_CERTIFIER_VALID);
	assert(summary.firing_cache_hit);
	metrics.evaluated_edges = summary.evaluated_edges;
	metrics.evaluated_firing_positions =
	    summary.evaluated_firing_positions;
	metrics.wall_shootable_calls = fixture.wall_shootable_calls;
	metrics.segment_child_calls = fixture.segment_child_calls;
	metrics.segment_center_calls = fixture.segment_center_calls;
	return metrics;
}

static certifier_benchmark_result run_cached_certifier_benchmark(void)
{
	certifier_benchmark_result metrics;
	double total_us = 0.0;
	int iteration;

	for (iteration = 0; iteration < 100; ++iteration) {
		metrics = run_cached_certifier_benchmark_once();
		total_us += metrics.wall_us;
	}
	metrics.wall_us = total_us / 100.0;
	return metrics;
}

static certifier_benchmark_result run_unexplored_benchmark_once(
    unsigned int work_limit)
{
	certifier_fixture fixture;
	guidebot_route_certifier_budget budget;
	certifier_benchmark_result metrics;
	level_metadata_scan_view view;
	level_metadata_unexplored_route route;
	double started;
	int result;

	memset(&metrics, 0, sizeof(metrics));
	initialize_benchmark_fixture(&fixture);
	view = make_view(&fixture);
	view.start_segment = 0;
	memset(&budget, 0, sizeof(budget));
	budget.work_limit = work_limit;
	memset(&route, 0, sizeof(route));
	route.target_seg = -1;
	route.waypoint_seg = -1;
	guidebot_route_certifier_reset_job(&Workspace);
	started = benchmark_wall_us();
	do {
		const unsigned int callbacks_before = fixture.segment_child_calls;
		const unsigned int centers_before = fixture.segment_center_calls;
		unsigned int callbacks;

		result = guidebot_route_find_unexplored_budgeted(
		    &view, &Workspace, &route, &budget);
		metrics.slices++;
		callbacks = fixture.segment_child_calls - callbacks_before +
		            fixture.segment_center_calls - centers_before;
		if (callbacks > metrics.max_callbacks_per_slice)
			metrics.max_callbacks_per_slice = callbacks;
	} while (result == GUIDEBOT_ROUTE_CERTIFIER_PENDING &&
	         metrics.slices < 10000);
	metrics.wall_us = benchmark_wall_us() - started;
	assert(result == GUIDEBOT_ROUTE_CERTIFIER_VALID);
	assert(route.component_size == BENCHMARK_SEGMENTS / 2);
	metrics.segment_child_calls = fixture.segment_child_calls;
	metrics.segment_center_calls = fixture.segment_center_calls;
	return metrics;
}

static certifier_benchmark_result run_unexplored_benchmark(
    unsigned int work_limit)
{
	certifier_benchmark_result metrics;
	double total_us = 0.0;
	int iteration;

	for (iteration = 0; iteration < 100; ++iteration) {
		metrics = run_unexplored_benchmark_once(work_limit);
		total_us += metrics.wall_us;
	}
	metrics.wall_us = total_us / 100.0;
	return metrics;
}

static certifier_benchmark_result run_compiled_selector_benchmark(void)
{
	certifier_fixture fixture;
	level_metadata_scan_view view;
	route_planner_plan_summary compiled_plan;
	route_planner_plan_summary live_plan;
	guidebot_route_validity_certificate certificate;
	guidebot_route_certifier_summary summary;
	certifier_benchmark_result metrics;
	double started;
	int iteration;
	int step;

	initialize_fixture(&fixture);
	initialize_plan(&compiled_plan);
	view = make_view(&fixture);
	view.initial_key_mask = LEVEL_METADATA_KEY_MASK_GOLD;
	Prepared.route_step_count = LEVEL_METADATA_MAX_ROUTE_STEPS;
	compiled_plan.route_step_count = Prepared.route_step_count;
	for (step = 1; step + 1 < Prepared.route_step_count; ++step) {
		Prepared.route_steps[step].kind = LEVEL_METADATA_ROUTE_KEY;
		Prepared.route_steps[step].activation_kind =
		    LEVEL_METADATA_ROUTE_ACTIVATION_PICKUP_KEY;
		Prepared.route_steps[step].key_index = 2;
		Prepared.route_steps[step].seg = 1;
		Prepared.route_steps[step].path_terminal_segment = 1;
	}
	Prepared.route_steps[Prepared.route_step_count - 1].kind =
	    LEVEL_METADATA_ROUTE_EXIT;
	Prepared.route_steps[Prepared.route_step_count - 1].activation_kind =
	    LEVEL_METADATA_ROUTE_ACTIVATION_ENTER_EXIT;
	Prepared.route_steps[Prepared.route_step_count - 1].seg = 4;
	Prepared.route_steps[Prepared.route_step_count - 1]
	    .path_terminal_segment = 4;
	memset(&metrics, 0, sizeof(metrics));
	for (iteration = 0; iteration < 100; ++iteration)
		assert(select_compiled(
		    &view, &compiled_plan, &live_plan, &certificate, &summary));
	started = benchmark_wall_us();
	for (iteration = 0; iteration < 10000; ++iteration)
		assert(select_compiled(
		    &view, &compiled_plan, &live_plan, &certificate, &summary));
	metrics.wall_us = (benchmark_wall_us() - started) / 10000.0;
	metrics.segment_child_calls = fixture.segment_child_calls;
	metrics.segment_center_calls = fixture.segment_center_calls;
	metrics.wall_shootable_calls = fixture.wall_shootable_calls;
	return metrics;
}

static certifier_benchmark_result run_compiled_switch_benchmark(void)
{
	certifier_fixture fixture;
	level_metadata_scan_view view;
	route_planner_plan_summary compiled_plan;
	route_planner_plan_summary live_plan;
	guidebot_route_validity_certificate certificate;
	guidebot_route_certifier_summary summary;
	certifier_benchmark_result metrics;
	level_metadata_route_step *step;
	double started;
	int candidate;
	int iteration;

	initialize_benchmark_fixture(&fixture);
	initialize_benchmark_plan(&compiled_plan, 1);
	view = make_view(&fixture);
	view.start_segment = 0;
	step = &Prepared.route_steps[1];
	step->switch_guidance_candidate_count =
	    LEVEL_METADATA_MAX_SWITCH_GUIDANCE_CANDIDATES;
	for (candidate = 0;
	     candidate < LEVEL_METADATA_MAX_SWITCH_GUIDANCE_CANDIDATES;
	     ++candidate) {
		step->switch_guidance_candidate_seg[candidate] =
		    BENCHMARK_SEGMENTS - 1 - candidate * 10;
		step->switch_guidance_candidate_quality[candidate] =
		    LEVEL_METADATA_SWITCH_SHOT_CONFIRMED;
		step->switch_guidance_candidate_incidence[candidate] =
		    LEVEL_METADATA_SHOT_COSINE_ONE - candidate * 1000;
	}
	for (iteration = 0; iteration < 10; ++iteration)
		assert(select_compiled(
		    &view, &compiled_plan, &live_plan, &certificate, &summary));
	fixture.segment_child_calls = 0;
	memset(&metrics, 0, sizeof(metrics));
	started = benchmark_wall_us();
	for (iteration = 0; iteration < 1000; ++iteration)
		assert(select_compiled(
		    &view, &compiled_plan, &live_plan, &certificate, &summary));
	metrics.wall_us = (benchmark_wall_us() - started) / 1000.0;
	metrics.evaluated_edges = summary.evaluated_edges;
	metrics.segment_child_calls = fixture.segment_child_calls / 1000;
	metrics.segment_center_calls = fixture.segment_center_calls;
	metrics.wall_shootable_calls = fixture.wall_shootable_calls;
	return metrics;
}

static void print_benchmark_case(
    const char *name, const certifier_benchmark_result *result, int last)
{
	printf(
	    "  \"%s\": {\"wall_us\": %.0f, \"slices\": %u, "
	    "\"evaluated_edges\": %u, \"evaluated_firing_positions\": %u, "
	    "\"shootability_calls\": %u, \"segment_child_calls\": %u, "
	    "\"segment_center_calls\": %u, \"max_callbacks_per_slice\": %u}%s\n",
	    name, result->wall_us, result->slices, result->evaluated_edges,
	    result->evaluated_firing_positions, result->wall_shootable_calls,
	    result->segment_child_calls, result->segment_center_calls,
	    result->max_callbacks_per_slice, last ? "" : ",");
}

static int run_benchmarks(void)
{
	const certifier_benchmark_result ordinary =
	    run_certifier_benchmark(0, 0, 0, 512);
	const certifier_benchmark_result detailed =
	    run_certifier_benchmark(1, 1, 0, 512);
	const certifier_benchmark_result cached =
	    run_cached_certifier_benchmark();
	const certifier_benchmark_result frontier =
	    run_certifier_benchmark(1, 0, 1, 512);
	const certifier_benchmark_result unsliced_frontier =
	    run_certifier_benchmark(1, 0, 1, 0);
	const certifier_benchmark_result unexplored =
	    run_unexplored_benchmark(512);
	const certifier_benchmark_result compiled =
	    run_compiled_selector_benchmark();
	const certifier_benchmark_result compiled_switch =
	    run_compiled_switch_benchmark();

	assert(ordinary.max_callbacks_per_slice <= 1024);
	assert(detailed.wall_shootable_calls <= 64);
	assert(detailed.max_callbacks_per_slice <= 1024);
	assert(cached.wall_shootable_calls == 1);
	assert(cached.evaluated_firing_positions == 0);
	assert(cached.max_callbacks_per_slice <= 1024);
	assert(frontier.slices > 1);
	assert(frontier.max_callbacks_per_slice <= 4096);
	assert(unexplored.max_callbacks_per_slice <= 1024);
	assert(compiled.segment_child_calls == 0);
	assert(compiled.segment_center_calls == 0);
	assert(compiled.wall_shootable_calls == 0);
	assert(compiled_switch.evaluated_edges <= BENCHMARK_SEGMENTS *
	                                              LEVEL_METADATA_MAX_SIDES);
	assert(compiled_switch.wall_shootable_calls == 0);
	assert(compiled_switch.segment_center_calls == 0);

	printf("{\n  \"schema\": \"dxx-guidebot-live-calculation-benchmark-v3\",\n");
	printf("  \"segments\": %d,\n  \"cases\": {\n", BENCHMARK_SEGMENTS);
	print_benchmark_case("ordinary_certification", &ordinary, 0);
	print_benchmark_case("detailed_switch_search", &detailed, 0);
	print_benchmark_case("cached_switch_reuse", &cached, 0);
	print_benchmark_case("unreachable_switch_frontier", &frontier, 0);
	print_benchmark_case("unreachable_switch_frontier_unsliced", &unsliced_frontier, 0);
	print_benchmark_case("largest_unexplored_area", &unexplored, 0);
	print_benchmark_case("compiled_action_selection", &compiled, 0);
	print_benchmark_case("compiled_switch_guidance", &compiled_switch, 1);
	printf("  }\n}\n");
	return 0;
}

int main(int argc, char **argv)
{
	if (argc == 2 && !strcmp(argv[1], "--benchmark"))
		return run_benchmarks();
	test_compiled_selector_never_restores_collected_key();
	test_compiled_selector_rebinds_moving_key_object();
	test_compiled_selector_chooses_reachable_switch_guidance();
	test_current_start_and_accessibility_select_action();
	test_first_reachable_required_action_preserves_order();
	test_identical_state_is_history_independent();
	test_disabled_action_uses_reachable_prepared_alternative();
	test_destroyed_switch_stays_complete_when_link_recloses();
	test_equal_switch_positions_use_segment_center();
	test_remote_cached_switch_position_reranks_near_switch();
	test_budgeted_certification_resumes_without_repeating_candidates();
	test_budgeted_certification_keeps_start_while_companion_moves();
	test_budgeted_certification_skips_trigger_completed_mid_scan();
	test_budgeted_unexplored_scan_publishes_complete_component();
	test_unexplored_scan_prefers_larger_locked_component();
	test_rejected_confirmed_switch_degrades_to_approximate_warning();
	test_steep_switch_position_loses_to_square_shot();
	test_nearby_detailed_switch_pose_avoids_minewide_scan();
	test_solid_illusion_wall_is_not_passable();
	test_visible_unlocked_triggered_door_is_physically_passable();
	test_keyed_buddy_proof_door_keeps_objective_reachable();
	test_keyed_door_blocks_objective_route_but_not_player_progress();
	test_reverse_side_keyed_buddy_proof_door_is_player_reachable();
	test_physical_frontier_follows_strategic_route();
	test_exit_projection_skips_only_countdown_steps();
	test_deferred_countdown_frontier_stops_before_closed_link();
	test_physical_frontier_can_plan_toward_triggered_link();
	test_unreachable_switch_uses_physical_frontier();
	test_switch_frontier_prefers_square_nearby_approach();
	test_reclosed_hidden_door_behind_start_does_not_regress_route();
	printf("guidebot route certifier tests passed\n");
	return 0;
}
