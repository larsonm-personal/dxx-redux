#ifndef DXX_XL_LIGHTNING_ADMISSION_H
#define DXX_XL_LIGHTNING_ADMISSION_H
#include "lightning_waypoints.h"
#include "lighting.h"
#include <algorithm>

namespace xl_compat
{
inline bool authored_lightning_shape(const Object &object)
{
	return object.type == 20 && object.reader_id == 1 && object.render == 12 && object.reader_render == 12 &&
	       !object.control && !object.movement && !object.multiplayer;
}

inline bool authored_lightning_emits(const Object &object)
{
	if (!authored_lightning_shape(object)) return false;
	if (object.render_data.size() != 26 || object.render_data[6] < -32768 || object.render_data[6] > 32767)
		throw std::runtime_error("XL lightning has an invalid bolt-count payload");
	// StaticFrame skips empty bolt populations before calling the emitter
	// The source emitter also rejects zero lifetime
	return object.render_data[6] > 0 && object.render_data[0] != 0;
}

inline int authored_lightning_target(const Inventory &level, const Object &object)
{
	if (object.render_data.size() != 26) throw std::runtime_error("XL lightning has an invalid payload layout");
	if (!authored_lightning_emits(object)) return -1;
	if (object.render_data[19] && !object.render_data[13]) return -1;
	const int id = object.render_data[8];
	if (!id) return -1;
	for (unsigned i = 0; i < level.objects.size(); ++i) {
		const auto &candidate = level.objects[i];
		if (&candidate == &object || candidate.type != 20 || candidate.reader_id != 1 || candidate.render_data.size() != 26 || candidate.render_data[7] != id) continue;
		if (!authored_lightning_shape(candidate) || candidate.segment < 0 || std::size_t(candidate.segment) >= level.segments.size())
			throw std::runtime_error("XL fixed lightning requires a stationary target owner");
		// Source effect lists link authored objects in ascending object order
		return int(i);
	}
	return -1;
}

inline bool authored_waypoint_shape(const Object &object)
{
	return object.type == 20 && object.reader_id == 3 && object.control == 17 && !object.movement &&
	       !object.render && !object.reader_render && !object.multiplayer && object.control_data.size() == 3;
}

inline int lightning_waypoint_id(const Object &object)
{
	return authored_lightning_shape(object) && object.render_data.size() == 26 ? object.render_data[5] : -1;
}

inline LightningPoint authored_lightning_position(const Inventory &level, const Object &object)
{
	const int id = lightning_waypoint_id(object);
	if (id < 0) return object.position;
	const auto node = std::find_if(level.objects.begin(), level.objects.end(), [id](const Object &candidate) {
		return authored_waypoint_shape(candidate) && candidate.control_data[0] == id;
	});
	if (node == level.objects.end()) throw std::runtime_error("XL moving lightning references a missing waypoint");
	return node->position;
}

inline LightningWaypoints prepare_lightning_waypoints(const Inventory &level)
{
	LightningWaypoints result;
	std::map<int, int> indices;
	for (const auto &object : level.objects) {
		if (object.control != 17) continue;
		if ((level.level_version != 23 && level.level_version != 26) || level.game_version != 40 || !authored_waypoint_shape(object))
			throw std::runtime_error("XL waypoint requires a 23/40 or 26/40 effect controller");
		const auto id = object.control_data[0];
		const auto speed = object.control_data[2];
		if (id < 0 || speed < 0 || !indices.emplace(id, int(result.nodes.size())).second)
			throw std::runtime_error("XL waypoint has an invalid or duplicate ID or negative speed");
		result.nodes.push_back({ object.position, speed, object.control_data[1] });
	}
	for (int index = 0; index < int(result.nodes.size()); ++index) {
		auto &node = result.nodes[index];
		const auto next = indices.find(node.next);
		if (next == indices.end() || next->second == index || result.nodes[next->second].previous >= 0)
			throw std::runtime_error("XL waypoint movement requires closed unbranched routes");
		node.next = next->second;
		result.nodes[node.next].previous = index;
	}
	for (int index = 0; index < int(result.nodes.size()); ++index) {
		const auto &node = result.nodes[index];
		for (int successor : { node.next, node.previous }) {
			const auto span = lightning_detail::subtract(result.nodes[successor].position, node.position);
			if (waypoint_distance(span) <= float(node.speed) / 40.0f)
				throw std::runtime_error("XL waypoint leg is too short for the authored speed");
		}
		for (int current = index; result.nodes[current].route < 0; current = result.nodes[current].next)
			result.nodes[current].route = index;
	}
	for (int index = 0; index < int(level.objects.size()); ++index) {
		const auto &object = level.objects[index];
		const int id = lightning_waypoint_id(object);
		if (id < 0) continue;
		const auto node = indices.find(id);
		const int partner = authored_lightning_target(level, object);
		if (node == indices.end() || !result.nodes[node->second].speed || partner < 0 ||
		    !authored_lightning_emits(level.objects[partner]) || lightning_waypoint_id(level.objects[partner]) < 0 ||
		    authored_lightning_target(level, level.objects[partner]) != index)
			throw std::runtime_error("XL moving lightning requires a positive-speed start and a reciprocal routed target");
		result.owners.emplace(index, LightningWaypointOwner{ node->second, partner });
	}
	return result;
}

inline int authored_lightning_target_length(const LightningPoint &position, const LightningPoint &endpoint)
{
	const int distance = geometry_default_magnitude(lightning_detail::subtract(endpoint, position));
	return distance >= 65536 ? distance / 65536 * 65536 : distance;
}

inline std::optional<LightningPoint> place_authored_lightning(LightningDefinition &definition, const LightningPoint &position,
                                                              const LightningPoint &forward, int length, std::optional<LightningPoint> target)
{
	if (definition.random_end) return std::nullopt;
	if (target) {
		definition.length = authored_lightning_target_length(position, *target);
		return target;
	}
	// StaticFrame uses forward * I2X(authored length), including signed lengths
	(void) lightning_detail::checked(std::int64_t(length) * 65536);
	LightningPoint endpoint{};
	for (unsigned axis = 0; axis < 3; ++axis)
		endpoint[axis] = lightning_detail::checked(std::int64_t(position[axis]) + std::int64_t(forward[axis]) * length);
	definition.length = length > 0 ? length * 65536 : geometry_default_magnitude(lightning_detail::subtract(endpoint, position));
	return endpoint;
}

inline LightningDefinition prepare_authored_lightning(const Inventory &level, const Object &object)
{
	if (!authored_lightning_shape(object))
		throw std::runtime_error("XL lightning requires a stationary uncontrolled single-player lightning object");
	if (object.segment < 0 || std::size_t(object.segment) >= level.segments.size())
		throw std::runtime_error("XL lightning object references an invalid segment");
	if (!authored_lightning_emits(object)) {
		// Other lightning can still use the authored object as a target
		LightningDefinition anchor{};
		anchor.enabled = object.render_data[25] != 0;
		return anchor;
	}
	const int target = authored_lightning_target(level, object);
	const bool random_end = object.render_data[19] && !object.render_data[13];
	auto definition = lightning_definition(object, authored_lightning_global_style, random_end ? std::nullopt : std::optional<int>(0));
	const auto endpoint = place_authored_lightning(definition, authored_lightning_position(level, object),
	                                               { object.orientation[6], object.orientation[7], object.orientation[8] }, object.render_data[2],
	                                               target < 0 ? std::nullopt : std::optional<LightningPoint>(authored_lightning_position(level, level.objects[target])));
	if (definition.glow < 0)
		throw std::runtime_error("XL lightning single-layer glow remains unsupported");
	if (object.render_data[5] < -1 || definition.angle < 0 || definition.in_plane || (definition.clamp && definition.amplitude <= 0) ||
	    definition.style < -1 || definition.style > 2 || lightning_active_style(definition) || definition.frames <= 0 || definition.nodes > 32767 || definition.bolts <= 0 || definition.bolts > 32767)
		throw std::runtime_error("XL lightning owner requires the implemented style-0 endpoint profile");
	(void) lightning_lifetime(definition, endpoint.has_value(), authored_lightning_children);
	return definition;
}
} // namespace xl_compat
#endif
