#ifndef DXX_XL_STATIC_EXPLOSION_H
#define DXX_XL_STATIC_EXPLOSION_H
#include "inventory.h"
#include <cstdint>
#include <stdexcept>
namespace xl_compat
{
inline bool static_explosion_shape(const Object &object)
{
	return object.type == 19 && object.control == 13 && object.movement == 0 && object.render == 5 &&
	       object.multiplayer == 0 && object.control_data.size() == 1 && object.render_data.size() == 3;
}
inline int static_explosion_logical_sound(int sample, const std::uint8_t *table, int count)
{
	if (sample < 0) return -1;
	for (int id = 0; id < count; ++id)
		if (table[id] == sample) return id;
	// Pinned UnXlatSound falls back to logical zero after its debug trap
	return 0;
}
struct StaticExplosionFrame {
	std::int32_t remaining;
	int frame;
};
// The pinned UpdatePowerupClip always advances type 19, without slot-based fudge
// Elapsed time is simulation time; high-resolution replacement animations are separate
inline StaticExplosionFrame advance_static_explosion(StaticExplosionFrame state, int frames, std::int32_t duration, std::int32_t elapsed)
{
	if (frames <= 0 || frames > 127 || duration <= 0 || elapsed < 0 || state.frame < 0 || state.frame >= frames)
		throw std::runtime_error("Invalid XL static explosion animation");
	// XL divides by a float slow-motion speed even at normal speed, rounding
	// large elapsed values before converting back to fixed-point simulation time
	const auto source_elapsed = std::int64_t(float(elapsed));
	if (source_elapsed > INT32_MAX) throw std::runtime_error("XL static explosion step exceeds source time range");
	auto remaining = std::int64_t(state.remaining) - source_elapsed;
	if (remaining < 0) {
		const auto count = (-remaining + duration - 1) / duration;
		remaining += count * duration;
		state.frame = (state.frame + int(count % frames)) % frames;
	}
	state.remaining = std::int32_t(remaining);
	return state;
}
} // namespace xl_compat
#endif
