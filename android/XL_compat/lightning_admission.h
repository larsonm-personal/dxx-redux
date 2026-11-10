#ifndef DXX_XL_LIGHTNING_ADMISSION_H
#define DXX_XL_LIGHTNING_ADMISSION_H
#include "lightning_path.h"
#include "lighting.h"

namespace xl_compat
{
inline bool authored_lightning_shape(const Object &object)
{
	return object.type == 20 && object.reader_id == 1 && object.render == 12 && object.reader_render == 12 &&
	       !object.control && !object.movement && !object.multiplayer;
}
inline bool authored_lightning_emits(const Object &object)
{
	if (object.render_data.size() != 26 || object.render_data[6] < -32768 || object.render_data[6] > 32767)
		throw std::runtime_error("XL lightning has an invalid bolt-count payload");
	return object.render_data[6] > 0;
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
		// StaticFrame retains target objects but creates no emitter for nBolts <= 0
		LightningDefinition anchor{};
		anchor.enabled = object.render_data[25] != 0;
		return anchor;
	}
	const int target = authored_lightning_target(level, object);
	const bool random_end = object.render_data[19] && !object.render_data[13];
	auto definition = lightning_definition(object, authored_lightning_global_style, random_end ? std::nullopt : std::optional<int>(0));
	const auto endpoint = place_authored_lightning(definition, object.position,
	                                               { object.orientation[6], object.orientation[7], object.orientation[8] }, object.render_data[2],
	                                               target < 0 ? std::nullopt : std::optional<LightningPoint>(level.objects[target].position));
	if (definition.glow < 0)
		throw std::runtime_error("XL lightning single-layer glow remains unsupported");
	if (object.render_data[5] != -1 || definition.angle < 0 || definition.in_plane || (definition.clamp && definition.amplitude <= 0) ||
	    definition.style < -1 || definition.style > 2 || lightning_active_style(definition) || definition.frames <= 0 || definition.nodes > 32767 || definition.bolts <= 0 || definition.bolts > 32767)
		throw std::runtime_error("XL lightning owner requires the implemented style-0 endpoint profile");
	(void) lightning_lifetime(definition, endpoint.has_value(), authored_lightning_children);
	return definition;
}
} // namespace xl_compat
#endif
