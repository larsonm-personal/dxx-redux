#ifndef DXX_XL_FIRE_EMITTER_H
#define DXX_XL_FIRE_EMITTER_H
#include "particle_fire.h"
#include "particle_smoke.h"
#include "particle_bubble.h"
#include "particle_waterfall.h"
#include "particle_weather.h"
#include <optional>
#include <type_traits>

#ifdef _MSC_VER
#pragma pack(push, 8)
#endif
namespace xl_compat
{
inline constexpr unsigned particle_system_limit = 10000;
// Source retirement order is significant for FX RNG for both authored kinds
template <ParticleKind Kind>
struct AuthoredParticleEmitter {
	static constexpr bool Smoke = Kind == ParticleKind::smoke;
	static constexpr bool Bubble = Kind == ParticleKind::bubbles;
	static constexpr bool Waterfall = Kind == ParticleKind::waterfall;
	static constexpr bool Rain = Kind == ParticleKind::rain;
	static constexpr bool Snow = Kind == ParticleKind::snow;
	static_assert(Kind == ParticleKind::fire || Smoke || Bubble || Waterfall || Rain || Snow);
	using Particle = std::conditional_t<Bubble, BubbleParticle, std::conditional_t<Smoke, SmokeParticle, std::conditional_t<Waterfall, WaterfallParticle, std::conditional_t<Rain || Snow, WeatherParticle, FireParticle>>>>;
	ParticleEmitterDefinition definition;
	ParticleOrientation orientation{ { { 65536, 0, 0 }, { 0, 65536, 0 }, { 0, 0, 65536 } } };
	ParticlePoint position{}, previous{};
	std::optional<ParticleFace> face;
	std::vector<Particle> slots;
	int segment = -1, first = 0, count = 0, ticks = 0, moved = 0, life = 0, demand = 0;
	float rate = 0;
	bool skybox = false;

	AuthoredParticleEmitter(const ParticleEmitterDefinition &input, const ParticlePoint &origin, const ParticleFace *surface, int location, bool sky, int time)
	    : definition(input), position(origin), previous(origin), segment(location), moved(time), skybox(sky)
	{
		if (input.image != (Bubble ? 2 : Smoke   ? 1
		                             : Waterfall ? 6
		                             : Rain      ? 3
		                             : Snow      ? 4
		                                         : 5) ||
		    input.count <= 0 || input.count > 100000 || input.life >= 0 || input.life == INT32_MIN)
			throw std::runtime_error("Invalid XL authored particle emitter");
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
	static const FireParticle &motion(const Particle &particle)
	{
		if constexpr (Smoke || Bubble || Waterfall || Rain || Snow) return particle.motion;
		else return particle;
	}
	static FireParticle &motion(Particle &particle)
	{
		if constexpr (Smoke || Bubble || Waterfall || Rain || Snow) return particle.motion;
		else return particle;
	}

	template <typename Random, typename World>
	int update(int time, Random &random, World &world, bool collisions)
	{
		using namespace particle_detail;
		if (slots.empty()) return 0;
		const int elapsed = signed_checked(std::int64_t(time) - moved);
		if (elapsed < 0) throw std::runtime_error("XL emitter clock moved backwards");
		const int limit = int(slots.size());
		for (int i = 0; i < count; ++i) {
			auto &slot = slots[std::size_t((first + i) % limit)];
			if constexpr (Bubble) (void) advance_bubble_particle(slot, time, world, collisions);
			else if constexpr (Smoke) (void) advance_smoke_particle(slot, time, float(definition.brightness) / 100.0f, random, world, collisions);
			else if constexpr (Waterfall) (void) advance_waterfall_particle(slot, time, world, collisions);
			else if constexpr (Rain || Snow) (void) advance_weather_particle(slot, time, world, collisions);
			else (void) advance_fire_particle(slot, time, random, world, collisions);
		}
		// XL shrinks the loop bound while retiring; preserve deferred dead entries
		for (int i = 0, j = first; i < count; ++i, j = (j + 1) % limit)
			if (motion(slots[std::size_t(j)]).life <= 0) {
				if (j != first) slots[std::size_t(j)] = slots[std::size_t(first)];
				first = (first + 1) % limit;
				--count;
			}
		ticks = signed_checked(std::int64_t(ticks) + elapsed);
		const float requested = rate * float(ticks);
		if (!std::isfinite(requested) || double(requested) > INT32_MAX) throw std::runtime_error("XL emitter demand exceeds the source arithmetic range");
		demand = int(requested);
		if (demand >= 1) {
			if constexpr (Bubble || Rain || Snow) demand = random.bounded(demand) + 1;
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
						auto &slot = slots[std::size_t((next + i) % limit)];
						if constexpr (Bubble) slot = create_bubble_particle(definition, at, orientation, face ? &*face : nullptr, segment, skybox, time, random);
						else if constexpr (Smoke) slot = create_smoke_particle(definition, at, face ? &*face : nullptr, segment, skybox, time, float(definition.brightness) / 100.0f, random);
						else if constexpr (Waterfall) slot = create_waterfall_particle(definition, at, face ? &*face : nullptr, segment, skybox, time, random);
						else if constexpr (Rain || Snow) slot = create_weather_particle<Kind>(definition, at, orientation, face ? &*face : nullptr, segment, skybox, time, random);
						else slot = create_fire_particle(definition, at, face ? &*face : nullptr, segment, skybox, time, random, &slot);
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
using FireEmitter = AuthoredParticleEmitter<ParticleKind::fire>;
using SmokeEmitter = AuthoredParticleEmitter<ParticleKind::smoke>;
using BubbleEmitter = AuthoredParticleEmitter<ParticleKind::bubbles>;
using WaterfallEmitter = AuthoredParticleEmitter<ParticleKind::waterfall>;
using RainEmitter = AuthoredParticleEmitter<ParticleKind::rain>;
using SnowEmitter = AuthoredParticleEmitter<ParticleKind::snow>;
} // namespace xl_compat
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#endif
