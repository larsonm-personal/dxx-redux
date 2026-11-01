#ifndef DXX_XL_LIGHTNING_ADMISSION_H
#define DXX_XL_LIGHTNING_ADMISSION_H
#include "lightning_effect.h"

namespace xl_compat
{
inline bool authored_lightning_shape(const Object &object)
{
	return object.type == 20 && object.reader_id == 1 && object.render == 12 && object.reader_render == 12 &&
	       !object.control && !object.movement && !object.multiplayer;
}

inline LightningDefinition prepare_authored_lightning(const Inventory &level, const Object &object)
{
	if (!authored_lightning_shape(object))
		throw std::runtime_error("XL lightning requires a stationary uncontrolled single-player lightning object");
	if (object.segment < 0 || std::size_t(object.segment) >= level.segments.size())
		throw std::runtime_error("XL lightning object references an invalid segment");
	const auto definition = lightning_definition(object, 2);
	if (definition.glow <= 0)
		throw std::runtime_error("XL lightning requires the implemented positive-glow renderer; non-glowing and single-layer profiles remain unsupported");
	if (object.render_data[5] != -1 || !definition.random_end || definition.angle || definition.in_plane || definition.children || definition.clamp ||
	    definition.style || definition.frames <= 0 || definition.nodes > 32767 || definition.bolts <= 0 || definition.bolts > 32767)
		throw std::runtime_error("XL lightning owner requires the implemented random style-0 profile");
	(void) lightning_lifetime(definition, false, false);
	return definition;
}
} // namespace xl_compat
#endif
