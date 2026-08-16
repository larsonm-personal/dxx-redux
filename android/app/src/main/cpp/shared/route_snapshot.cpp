#include "route_snapshot.h"
#include "route_snapshot_c.h"

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <exception>
#include <utility>

namespace dxx_route
{
namespace
{

class stable_hasher
{
  public:
	void add_bool(bool value)
	{
		add_byte(value ? 1u : 0u);
	}

	void add_int(int value)
	{
		const std::uint32_t bits = static_cast<std::uint32_t>(value);
		for (unsigned shift = 0; shift < 32; shift += 8)
			add_byte(static_cast<unsigned char>(bits >> shift));
	}

	std::uint64_t value() const
	{
		return m_value;
	}

  private:
	void add_byte(unsigned char value)
	{
		m_value ^= value;
		m_value *= 1099511628211ull;
	}

	std::uint64_t m_value = 14695981039346656037ull;
};

void hash_position(stable_hasher &hasher, const route_position &position)
{
	hasher.add_bool(position.valid);
	for (const int coordinate : position.value)
		hasher.add_int(coordinate);
}

route_position read_position(int (*callback)(void *, int, int[3]),
                             void *user, int index)
{
	route_position result;
	int value[3] = {};
	if (callback && callback(user, index, value)) {
		result.valid = true;
		result.value = { { value[0], value[1], value[2] } };
	}
	return result;
}

route_position read_segment_vertex(const level_metadata_scan_view &view,
                                   int segment, int vertex)
{
	route_position result;
	int value[3] = {};
	if (view.segment_vertex &&
	    view.segment_vertex(view.user, segment, vertex, value)) {
		result.valid = true;
		result.value = { { value[0], value[1], value[2] } };
	}
	return result;
}

route_position read_start_position(const level_metadata_scan_view &view)
{
	route_position result;
	int value[3] = {};
	if (view.start_position && view.start_position(view.user, value)) {
		result.valid = true;
		result.value = { { value[0], value[1], value[2] } };
	}
	return result;
}

route_trigger_kind normalize_trigger_kind(
    const level_metadata_scan_view &view, int raw_type)
{
	if (raw_type == view.trigger_type_open_door)
		return route_trigger_kind::open_door;
	if (raw_type == view.trigger_type_exit)
		return route_trigger_kind::exit;
	if (raw_type == view.trigger_type_secret_exit)
		return route_trigger_kind::secret_exit;
	if (raw_type == view.trigger_type_illusion_off)
		return route_trigger_kind::illusion_off;
	if (raw_type == view.trigger_type_unlock_door)
		return route_trigger_kind::unlock_door;
	if (raw_type == view.trigger_type_open_wall)
		return route_trigger_kind::open_wall;
	if (raw_type == view.trigger_type_illusory_wall)
		return route_trigger_kind::illusory_wall;
	return route_trigger_kind::other;
}

route_wall_kind normalize_wall_kind(const level_metadata_scan_view &view,
                                    int raw_type)
{
	if (raw_type < 0)
		return route_wall_kind::none;
	if (raw_type == view.wall_type_blastable)
		return route_wall_kind::blastable;
	if (raw_type == view.wall_type_door)
		return route_wall_kind::door;
	if (raw_type == view.wall_type_illusion)
		return route_wall_kind::illusion;
	if (raw_type == view.wall_type_open)
		return route_wall_kind::open;
	return route_wall_kind::other;
}

route_key_requirement normalize_wall_key(const level_metadata_scan_view &view,
                                         int raw_key)
{
	if (raw_key == 0 || raw_key == view.wall_key_none)
		return route_key_requirement::none;
	if (raw_key == view.wall_key_blue)
		return route_key_requirement::blue;
	if (raw_key == view.wall_key_red)
		return route_key_requirement::red;
	if (raw_key == view.wall_key_gold)
		return route_key_requirement::gold;
	return route_key_requirement::unknown;
}

route_object_kind normalize_object_kind(const level_metadata_scan_view &view,
                                        int raw_type)
{
	if (raw_type == view.obj_type_robot)
		return route_object_kind::robot;
	if (raw_type == view.obj_type_powerup)
		return route_object_kind::powerup;
	if (raw_type == view.obj_type_control_center)
		return route_object_kind::control_center;
	return route_object_kind::other;
}

route_key_requirement normalize_powerup_key(const level_metadata_scan_view &view,
                                            int raw_id)
{
	if (raw_id == view.powerup_key_blue)
		return route_key_requirement::blue;
	if (raw_id == view.powerup_key_red)
		return route_key_requirement::red;
	if (raw_id == view.powerup_key_gold)
		return route_key_requirement::gold;
	return route_key_requirement::none;
}

std::uint64_t hash_topology(const route_topology &topology)
{
	stable_hasher hasher;
	hasher.add_int(1);
	hasher.add_int(static_cast<int>(topology.segments.size()));
	hasher.add_int(static_cast<int>(topology.walls.size()));
	hasher.add_int(static_cast<int>(topology.triggers.size()));
	for (const auto &segment : topology.segments) {
		hash_position(hasher, segment.center);
		hasher.add_bool(segment.control_center);
		for (const auto &vertex : segment.vertices)
			hash_position(hasher, vertex);
		for (const auto &side : segment.sides) {
			hasher.add_int(side.child);
			hasher.add_int(side.reverse_side);
			hasher.add_int(side.wall);
			hasher.add_int(side.clearance_radius);
			hash_position(hasher, side.center);
			hasher.add_int(static_cast<int>(side.opener_walls.size()));
			for (const int opener_wall : side.opener_walls)
				hasher.add_int(opener_wall);
		}
	}
	for (const auto &wall : topology.walls) {
		hasher.add_int(wall.segment);
		hasher.add_int(wall.side);
		hash_position(hasher, wall.target);
		hasher.add_bool(wall.shootable_trigger);
	}
	for (const auto &trigger : topology.triggers) {
		hasher.add_int(trigger.raw_type);
		hasher.add_int(static_cast<int>(trigger.kind));
		hasher.add_int(static_cast<int>(trigger.links.size()));
		for (const auto &link : trigger.links) {
			hasher.add_int(link.segment);
			hasher.add_int(link.side);
		}
	}
	return hasher.value();
}

route_state_fingerprints fingerprint_state(const route_state &state)
{
	route_state_fingerprints result;
	stable_hasher start;
	start.add_int(state.start_segment);
	hash_position(start, state.start_position);
	result.start = start.value();

	stable_hasher progression;
	progression.add_int(state.key_mask);
	progression.add_bool(state.control_center_destroyed);
	result.progression = progression.value();

	stable_hasher navigation;
	navigation.add_int(static_cast<int>(state.segments.size()));
	navigation.add_int(static_cast<int>(state.walls.size()));
	for (const auto &segment : state.segments) {
		for (const auto &side : segment.sides) {
			navigation.add_bool(side.flyable);
			navigation.add_bool(side.hard_blocked);
			navigation.add_bool(side.control_center_link);
			navigation.add_bool(side.exit_trigger);
		}
	}
	for (const auto &wall : state.walls) {
		navigation.add_int(wall.type);
		navigation.add_int(static_cast<int>(wall.kind));
		navigation.add_int(wall.flags);
		navigation.add_int(wall.keys);
		navigation.add_int(static_cast<int>(wall.key));
		navigation.add_int(wall.clip_flags);
		navigation.add_int(wall.trigger);
		navigation.add_bool(wall.locked);
		navigation.add_bool(wall.opened);
		navigation.add_bool(wall.hidden);
		navigation.add_bool(wall.buddy_proof);
	}
	result.navigation = navigation.value();

	stable_hasher triggers;
	triggers.add_int(static_cast<int>(state.triggers.size()));
	for (const auto &trigger : state.triggers) {
		triggers.add_int(trigger.flags);
		triggers.add_bool(trigger.disabled);
	}
	result.triggers = triggers.value();

	stable_hasher objects;
	objects.add_int(static_cast<int>(state.objects.size()));
	for (const auto &object : state.objects) {
		objects.add_int(object.segment);
		objects.add_int(object.type);
		objects.add_int(object.id);
		objects.add_int(object.flags);
		objects.add_int(object.contains_type);
		objects.add_int(object.contains_id);
		objects.add_int(object.contains_count);
		objects.add_int(static_cast<int>(object.kind));
		objects.add_int(static_cast<int>(object.key));
		objects.add_int(static_cast<int>(object.contains_key));
		hash_position(objects, object.position);
		objects.add_bool(object.should_be_dead);
		objects.add_bool(object.boss);
		objects.add_bool(object.companion);
		objects.add_bool(object.fleeing);
	}
	result.objects = objects.value();

	stable_hasher automap;
	automap.add_int(static_cast<int>(state.segments.size()));
	for (const auto &segment : state.segments)
		automap.add_bool(segment.explored);
	result.automap = automap.value();
	return result;
}

std::uint64_t hash_state(const route_state &state)
{
	stable_hasher hasher;
	hasher.add_int(1);
	hasher.add_int(state.start_segment);
	hash_position(hasher, state.start_position);
	hasher.add_int(state.key_mask);
	hasher.add_bool(state.control_center_destroyed);
	hasher.add_int(static_cast<int>(state.segments.size()));
	hasher.add_int(static_cast<int>(state.walls.size()));
	hasher.add_int(static_cast<int>(state.triggers.size()));
	hasher.add_int(static_cast<int>(state.objects.size()));
	for (const auto &segment : state.segments) {
		hasher.add_bool(segment.explored);
		for (const auto &side : segment.sides) {
			hasher.add_bool(side.flyable);
			hasher.add_bool(side.hard_blocked);
			hasher.add_bool(side.control_center_link);
			hasher.add_bool(side.exit_trigger);
		}
	}
	for (const auto &wall : state.walls) {
		hasher.add_int(wall.type);
		hasher.add_int(static_cast<int>(wall.kind));
		hasher.add_int(wall.flags);
		hasher.add_int(wall.keys);
		hasher.add_int(static_cast<int>(wall.key));
		hasher.add_int(wall.clip_flags);
		hasher.add_int(wall.trigger);
		hasher.add_bool(wall.locked);
		hasher.add_bool(wall.opened);
		hasher.add_bool(wall.hidden);
	}
	for (const auto &trigger : state.triggers) {
		hasher.add_int(trigger.flags);
		hasher.add_bool(trigger.disabled);
	}
	for (const auto &object : state.objects) {
		hasher.add_int(object.segment);
		hasher.add_int(object.type);
		hasher.add_int(object.id);
		hasher.add_int(object.flags);
		hasher.add_int(object.contains_type);
		hasher.add_int(object.contains_id);
		hasher.add_int(object.contains_count);
		hasher.add_int(static_cast<int>(object.kind));
		hasher.add_int(static_cast<int>(object.key));
		hasher.add_int(static_cast<int>(object.contains_key));
		hash_position(hasher, object.position);
		hasher.add_bool(object.should_be_dead);
		hasher.add_bool(object.boss);
		hasher.add_bool(object.companion);
		hasher.add_bool(object.fleeing);
	}
	return hasher.value();
}

bool fail(std::string *problem, const char *message)
{
	if (problem)
		*problem = message;
	return false;
}

} // namespace

bool build_route_snapshot(const level_metadata_scan_view &view,
                          route_snapshot &snapshot,
                          std::string *problem)
{
	if (problem)
		problem->clear();
	if (view.num_segments <= 0 ||
	    view.num_segments > LEVEL_METADATA_MAX_SEGMENTS)
		return fail(problem, "invalid route snapshot segment count");
	if (view.num_walls < 0 || view.num_walls > LEVEL_METADATA_MAX_WALLS)
		return fail(problem, "invalid route snapshot wall count");
	if (view.num_triggers < 0 ||
	    view.num_triggers > LEVEL_METADATA_MAX_TRIGGERS)
		return fail(problem, "invalid route snapshot trigger count");
	if (!view.segment_child)
		return fail(problem, "route snapshot requires segment adjacency");
	const int object_count = view.object_count ? view.object_count(view.user) : 0;
	if (object_count < 0 || object_count > LEVEL_METADATA_MAX_OBJECTS)
		return fail(problem, "invalid route snapshot object count");

	route_snapshot next;
	next.topology.segments.resize(static_cast<std::size_t>(view.num_segments));
	next.topology.walls.resize(static_cast<std::size_t>(view.num_walls));
	next.topology.triggers.resize(static_cast<std::size_t>(view.num_triggers));
	next.state.start_segment = view.start_segment;
	next.state.start_position = read_start_position(view);
	next.state.key_mask = view.initial_key_mask;
	next.state.control_center_destroyed =
	    view.initial_control_center_destroyed != 0;
	next.state.segments.resize(static_cast<std::size_t>(view.num_segments));
	next.state.walls.resize(static_cast<std::size_t>(view.num_walls));
	next.state.triggers.resize(static_cast<std::size_t>(view.num_triggers));
	next.state.objects.resize(static_cast<std::size_t>(object_count));

	for (int segment_index = 0; segment_index < view.num_segments;
	     ++segment_index) {
		auto &topology_segment = next.topology.segments[segment_index];
		auto &state_segment = next.state.segments[segment_index];
		topology_segment.center =
		    read_position(view.segment_center, view.user, segment_index);
		topology_segment.control_center =
		    view.segment_special &&
		    view.segment_special(view.user, segment_index) ==
		        view.segment_special_control_center;
		for (int vertex = 0; vertex < 8; ++vertex)
			topology_segment.vertices[vertex] =
			    read_segment_vertex(view, segment_index, vertex);
		state_segment.explored = view.segment_is_explored &&
		                         view.segment_is_explored(
		                             view.user, segment_index) != 0;
		for (int side_index = 0; side_index < LEVEL_METADATA_MAX_SIDES;
		     ++side_index) {
			auto &topology_side = topology_segment.sides[side_index];
			auto &state_side = state_segment.sides[side_index];
			topology_side.child = view.segment_child(
			    view.user, segment_index, side_index);
			if (topology_side.child >= 0 &&
			    topology_side.child < view.num_segments && view.reverse_side)
				topology_side.reverse_side = view.reverse_side(
				    view.user, segment_index, topology_side.child);
			if (view.wall_num)
				topology_side.wall = view.wall_num(
				    view.user, segment_index, side_index);
			if (view.side_clearance_radius)
				topology_side.clearance_radius =
				    view.side_clearance_radius(
				        view.user, segment_index, side_index);
			if (view.side_center) {
				int center[3] = {};
				if (view.side_center(view.user, segment_index, side_index, center)) {
					topology_side.center.valid = true;
					topology_side.center.value = {
						{ center[0], center[1], center[2] }
					};
				}
			}
			if (view.triggered_side_opener_count &&
			    view.triggered_side_opener_wall_num) {
				const int opener_count = view.triggered_side_opener_count(
				    view.user, segment_index, side_index);
				if (opener_count < 0 || opener_count > view.num_walls)
					return fail(problem, "invalid route snapshot opener count");
				for (int opener = 0; opener < opener_count; ++opener)
					topology_side.opener_walls.push_back(
					    view.triggered_side_opener_wall_num(
					        view.user, segment_index, side_index, opener));
			}
			state_side.flyable = view.side_is_flyable &&
			                     view.side_is_flyable(
			                         view.user, segment_index, side_index) != 0;
			state_side.hard_blocked = view.side_is_hard_blocked &&
			                          view.side_is_hard_blocked(
			                              view.user, segment_index, side_index) != 0;
			state_side.control_center_link =
			    view.side_is_control_center_link &&
			    view.side_is_control_center_link(
			        view.user, segment_index, side_index) != 0;
			state_side.exit_trigger = view.side_has_exit_trigger &&
			                          view.side_has_exit_trigger(
			                              view.user, segment_index, side_index) != 0;
		}
	}

	for (int trigger_index = 0; trigger_index < view.num_triggers;
	     ++trigger_index) {
		auto &topology_trigger = next.topology.triggers[trigger_index];
		auto &state_trigger = next.state.triggers[trigger_index];
		if (view.trigger_type)
			topology_trigger.raw_type = view.trigger_type(
			    view.user, trigger_index);
		topology_trigger.kind = normalize_trigger_kind(
		    view, topology_trigger.raw_type);
		if (view.trigger_flags)
			state_trigger.flags = view.trigger_flags(view.user, trigger_index);
		state_trigger.disabled = view.trigger_flag_disabled != 0 &&
		                         (state_trigger.flags &
		                          view.trigger_flag_disabled) != 0;
		const int link_count = view.trigger_link_count ? view.trigger_link_count(view.user, trigger_index) : 0;
		if (link_count < 0 || link_count > LEVEL_METADATA_MAX_ROUTE_LINKS)
			return fail(problem, "invalid route snapshot trigger link count");
		for (int link_index = 0; link_index < link_count; ++link_index) {
			route_topology_trigger_link link;
			if (view.trigger_link_segment)
				link.segment = view.trigger_link_segment(
				    view.user, trigger_index, link_index);
			if (view.trigger_link_side)
				link.side = view.trigger_link_side(
				    view.user, trigger_index, link_index);
			topology_trigger.links.push_back(link);
		}
	}

	for (int object_index = 0; object_index < object_count; ++object_index) {
		auto &object = next.state.objects[object_index];
		if (view.object_segment)
			object.segment = view.object_segment(view.user, object_index);
		if (view.object_type)
			object.type = view.object_type(view.user, object_index);
		if (view.object_id)
			object.id = view.object_id(view.user, object_index);
		if (view.object_flags)
			object.flags = view.object_flags(view.user, object_index);
		if (view.object_contains_type)
			object.contains_type = view.object_contains_type(view.user, object_index);
		if (view.object_contains_id)
			object.contains_id = view.object_contains_id(view.user, object_index);
		if (view.object_contains_count)
			object.contains_count = view.object_contains_count(view.user, object_index);
		object.kind = normalize_object_kind(view, object.type);
		if (object.kind == route_object_kind::powerup)
			object.key = normalize_powerup_key(view, object.id);
		if (object.contains_type == view.obj_type_powerup)
			object.contains_key = normalize_powerup_key(view, object.contains_id);
		object.position = read_position(view.object_position, view.user, object_index);
		object.should_be_dead = view.obj_flag_should_be_dead != 0 &&
		                        (object.flags & view.obj_flag_should_be_dead) != 0;
		object.boss = view.object_is_boss &&
		              view.object_is_boss(view.user, object_index) != 0;
		object.companion = view.object_is_companion &&
		                   view.object_is_companion(view.user, object_index) != 0;
		object.fleeing = view.object_is_fleeing &&
		                 view.object_is_fleeing(view.user, object_index) != 0;
	}

	for (int wall_index = 0; wall_index < view.num_walls; ++wall_index) {
		auto &topology_wall = next.topology.walls[wall_index];
		auto &state_wall = next.state.walls[wall_index];
		if (view.wall_segment)
			topology_wall.segment = view.wall_segment(view.user, wall_index);
		if (view.wall_side)
			topology_wall.side = view.wall_side(view.user, wall_index);
		if (topology_wall.segment >= 0 &&
		    topology_wall.segment < view.num_segments &&
		    topology_wall.side >= 0 &&
		    topology_wall.side < LEVEL_METADATA_MAX_SIDES)
			topology_wall.target = next.topology
			                           .segments[topology_wall.segment]
			                           .sides[topology_wall.side]
			                           .center;
		topology_wall.shootable_trigger =
		    view.wall_is_shootable_trigger &&
		    view.wall_is_shootable_trigger(view.user, wall_index) != 0;
		if (view.wall_type)
			state_wall.type = view.wall_type(view.user, wall_index);
		state_wall.kind = normalize_wall_kind(view, state_wall.type);
		if (view.wall_flags)
			state_wall.flags = view.wall_flags(view.user, wall_index);
		if (view.wall_keys)
			state_wall.keys = view.wall_keys(view.user, wall_index);
		state_wall.key = normalize_wall_key(view, state_wall.keys);
		if (view.wall_clip_flags)
			state_wall.clip_flags = view.wall_clip_flags(view.user, wall_index);
		if (view.wall_trigger)
			state_wall.trigger = view.wall_trigger(view.user, wall_index);
		state_wall.locked = view.wall_flag_door_locked != 0 &&
		                    (state_wall.flags & view.wall_flag_door_locked) != 0;
		state_wall.opened =
		    (view.wall_flag_door_opened != 0 &&
		     (state_wall.flags & view.wall_flag_door_opened) != 0) ||
		    (view.wall_is_opening &&
		     view.wall_is_opening(view.user, wall_index));
		state_wall.hidden = view.wall_clip_hidden != 0 &&
		                    (state_wall.clip_flags & view.wall_clip_hidden) != 0;
		state_wall.buddy_proof = view.wall_flag_buddy_proof != 0 &&
		                         (state_wall.flags &
		                          view.wall_flag_buddy_proof) != 0;
	}

	next.topology.hash = hash_topology(next.topology);
	next.state.fingerprints = fingerprint_state(next.state);
	next.state.hash = hash_state(next.state);
	snapshot = std::move(next);
	return true;
}

} // namespace dxx_route

namespace
{

void copy_problem(char *out, int capacity, const char *problem)
{
	if (!out || capacity <= 0)
		return;
	std::snprintf(out, static_cast<std::size_t>(capacity), "%s",
	              problem ? problem : "");
}

} // namespace

extern "C" int route_snapshot_build_summary(
    const level_metadata_scan_view *view,
    route_snapshot_summary *summary,
    char *problem,
    int problem_capacity)
{
	if (summary)
		std::memset(summary, 0, sizeof(*summary));
	copy_problem(problem, problem_capacity, "");
	if (!view || !summary) {
		copy_problem(problem, problem_capacity,
		             "route snapshot summary requires input and output");
		return 0;
	}
	try {
		dxx_route::route_snapshot snapshot;
		std::string detail;
		if (!dxx_route::build_route_snapshot(*view, snapshot, &detail)) {
			copy_problem(problem, problem_capacity, detail.c_str());
			return 0;
		}
		summary->topology_hash = snapshot.topology.hash;
		summary->state_hash = snapshot.state.hash;
		summary->start_hash = snapshot.state.fingerprints.start;
		summary->progression_hash = snapshot.state.fingerprints.progression;
		summary->navigation_hash = snapshot.state.fingerprints.navigation;
		summary->trigger_hash = snapshot.state.fingerprints.triggers;
		summary->object_hash = snapshot.state.fingerprints.objects;
		summary->automap_hash = snapshot.state.fingerprints.automap;
		summary->segment_count =
		    static_cast<int>(snapshot.topology.segments.size());
		summary->wall_count =
		    static_cast<int>(snapshot.topology.walls.size());
		summary->trigger_count =
		    static_cast<int>(snapshot.topology.triggers.size());
		summary->object_count =
		    static_cast<int>(snapshot.state.objects.size());
		summary->start_segment = snapshot.state.start_segment;
		summary->key_mask = snapshot.state.key_mask;
		summary->control_center_destroyed =
		    snapshot.state.control_center_destroyed ? 1 : 0;
		return 1;
	} catch (const std::exception &error) {
		copy_problem(problem, problem_capacity, error.what());
	} catch (...) {
		copy_problem(problem, problem_capacity,
		             "unknown route snapshot failure");
	}
	return 0;
}
