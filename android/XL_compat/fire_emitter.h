#ifndef DXX_XL_FIRE_EMITTER_H
#define DXX_XL_FIRE_EMITTER_H
#include "particle_fire.h"
#include <optional>

#ifdef _MSC_VER
#pragma pack(push, 8)
#endif
namespace xl_compat
{
inline constexpr unsigned particle_system_limit = 10000;
// Owned authored-fire ring; source retirement order is significant for FX RNG
struct FireEmitter {
	ParticleEmitterDefinition definition;
	ParticlePoint position{}, previous{};
	std::optional<ParticleFace> face;
	std::vector<FireParticle> slots;
	int segment = -1, first = 0, count = 0, ticks = 0, moved = 0, life = 0, demand = 0;
	float rate = 0;
	bool skybox = false;

	FireEmitter(const ParticleEmitterDefinition &input, const ParticlePoint &origin, const ParticleFace *surface, int location, bool sky, int time)
	    : definition(input), position(origin), previous(origin), segment(location), moved(time), skybox(sky)
	{
		if (input.image != 5 || input.count <= 0 || input.count > 100000 || input.life >= 0 || input.life == INT32_MIN)
			throw std::runtime_error("Invalid XL authored fire emitter");
		if (surface) face = *surface;
		slots.resize(std::size_t(input.count));
		set_enabled(true);
	}
	void set_enabled(bool enabled)
	{
		life = enabled ? definition.life : 0;
		rate = life ? float(definition.count) / float(-life * 1.25f) : 0.0f;
		ticks = 0;
	}
	bool drained() const
	{
		return !life && !count;
	}

	template <typename Random, typename World>
	int update(int time, Random &random, World &world, bool collisions)
	{
		using namespace particle_detail;
		if (slots.empty()) return 0;
		const int elapsed = signed_checked(std::int64_t(time) - moved);
		if (elapsed < 0) throw std::runtime_error("XL emitter clock moved backwards");
		const int limit = int(slots.size());
		for (int i = 0; i < count; ++i) (void) advance_fire_particle(slots[std::size_t((first + i) % limit)], time, random, world, collisions);
		// XL shrinks the loop bound while retiring; preserve deferred dead entries
		for (int i = 0, j = first; i < count; ++i, j = (j + 1) % limit)
			if (slots[std::size_t(j)].life <= 0) {
				if (j != first) slots[std::size_t(j)] = slots[std::size_t(first)];
				first = (first + 1) % limit;
				--count;
			}
		ticks = signed_checked(std::int64_t(ticks) + elapsed);
		const float requested = rate * float(ticks);
		if (!std::isfinite(requested) || double(requested) > INT32_MAX) throw std::runtime_error("XL emitter demand exceeds the source arithmetic range");
		demand = int(requested);
		if (demand >= 1) {
			ticks = 0;
			if (life < 0) {
				const int created = std::min(demand, definition.count - count);
				if (created > 0) {
					std::array<float, 3> origin{}, step{};
					for (unsigned axis = 0; axis < 3; ++axis) {
						origin[axis] = float(previous[axis]) / 65536.0f;
						step[axis] = float(signed_checked(std::int64_t(position[axis]) - previous[axis])) / 65536.0f / float(created);
					}
					const int next = (first + count) % limit;
					for (int i = 0; i < created; ++i) {
						ParticlePoint at;
						for (unsigned axis = 0; axis < 3; ++axis) at[axis] = signed_checked(std::int64_t((origin[axis] + step[axis] * float(i)) * 65536.0f));
						slots[std::size_t((next + i) % limit)] = create_fire_particle(definition, at, face ? &*face : nullptr, segment, skybox, time, random);
					}
					count += created;
				}
			}
		}
		moved = time;
		previous = position;
		return count;
	}
};
} // namespace xl_compat
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#endif
