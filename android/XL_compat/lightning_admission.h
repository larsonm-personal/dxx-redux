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

inline int authored_lightning_target(const Inventory &level, const Object &object)
{
	if (object.render_data.size() != 26) throw std::runtime_error("XL lightning has an invalid payload layout");
	if (object.render_data[19] && !object.render_data[13]) return -1;
	const int id = object.render_data[8];
	int target = -1;
	for (unsigned i = 0; i < level.objects.size(); ++i) {
		const auto &candidate = level.objects[i];
		if (&candidate == &object || candidate.type != 20 || candidate.reader_id != 1 || candidate.render_data.size() != 26 || candidate.render_data[7] != id) continue;
		if (id <= 0 || target != -1 || !authored_lightning_shape(candidate) || candidate.segment < 0 || std::size_t(candidate.segment) >= level.segments.size())
			throw std::runtime_error("XL fixed lightning requires one stationary target owner");
		target = int(i);
	}
	if (target < 0) throw std::runtime_error("XL fixed lightning requires an authored target owner");
	return target;
}

inline int authored_lightning_target_length(const LightningPoint &position, const LightningPoint &endpoint)
{
	const int distance = geometry_default_magnitude(lightning_detail::subtract(endpoint, position));
	return distance >= 65536 ? distance / 65536 * 65536 : distance;
}

inline LightningDefinition prepare_authored_lightning(const Inventory &level, const Object &object)
{
	if (!authored_lightning_shape(object))
		throw std::runtime_error("XL lightning requires a stationary uncontrolled single-player lightning object");
	if (object.segment < 0 || std::size_t(object.segment) >= level.segments.size())
		throw std::runtime_error("XL lightning object references an invalid segment");
	const int target = authored_lightning_target(level, object);
	auto definition = lightning_definition(object, authored_lightning_global_style, target < 0 ? std::nullopt : std::optional<int>(0));
	if (target >= 0) definition.length = authored_lightning_target_length(object.position, level.objects[target].position);
	if (definition.glow < 0)
		throw std::runtime_error("XL lightning single-layer glow remains unsupported");
	if (object.render_data[5] != -1 || definition.angle < 0 || definition.in_plane || (definition.clamp && definition.amplitude <= 0) ||
	    definition.style < -1 || definition.style > 2 || lightning_active_style(definition) || definition.frames <= 0 || definition.nodes > 32767 || definition.bolts <= 0 || definition.bolts > 32767)
		throw std::runtime_error("XL lightning owner requires the implemented style-0 endpoint profile");
	(void) lightning_lifetime(definition, target >= 0, authored_lightning_children);
	return definition;
}
} // namespace xl_compat
#endif
