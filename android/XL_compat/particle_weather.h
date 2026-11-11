#ifndef DXX_XL_PARTICLE_WEATHER_H
#define DXX_XL_PARTICLE_WEATHER_H
#include "particle_bubble.h"

namespace xl_compat
{
struct WeatherParticle {
	FireParticle motion;
	std::array<float, 4> color{};
	int rotation_phase = 1;
	bool snow = false;
};

template <ParticleKind Kind, typename Random>
WeatherParticle create_weather_particle(const ParticleEmitterDefinition &definition, const ParticlePoint &position,
                                        const ParticleOrientation &orientation, const ParticleFace *face,
                                        int segment, bool skybox, int time, Random &random)
{
	using namespace particle_detail;
	static_assert(Kind == ParticleKind::rain || Kind == ParticleKind::snow);
	constexpr bool Snow = Kind == ParticleKind::snow;
	if (definition.image != (Snow ? 4 : 3) || definition.fade != -1 || definition.life >= 0 || definition.life == INT32_MIN ||
	    definition.speed < 0 || definition.speed > INT32_MAX / 65536 || !(definition.scale < 0) ||
	    !std::isfinite(definition.scale) || double(-definition.scale) > INT32_MAX)
		throw std::runtime_error("Invalid XL authored weather particle definition");
	WeatherParticle result;
	result.snow = Snow;
	auto &motion = result.motion;
	motion.segment = segment;
	motion.skybox = skybox;
	motion.updated = motion.moved = time;
	motion.life = -definition.life;
	motion.bounce = 1;
	motion.rotates = Snow;
	motion.decay = 1;
	for (unsigned i = 0; i < 4; ++i) result.color[i] = std::min(definition.color[i], 1.0f);
	if (definition.color[3] < 0) {
		const auto alpha = static_cast<unsigned char>(-definition.color[3] * 255.0f * 0.25f + 0.5f);
		result.color[3] = float(3 * alpha + scaled_random(random, 2 * alpha)) / 255.0f;
	}
	result.color[3] /= 2;
	motion.direction = definition.direction;
	if constexpr (Snow) {
		ParticlePoint jitter;
		for (auto &component : jitter) component = scaled_random(random, 16384) - 8192;
		motion.drift = geometry_default_direction(add(motion.direction, jitter));
		const float angle = float(std::int16_t(fixed_normalized_angle(geometry_default_direction(motion.drift), geometry_default_direction(motion.direction))));
		int speed = definition.speed * 65536;
		if (angle) speed = int(float(speed) / float(::exp(8192 / angle)));
		motion.drift = multiply(motion.drift, speed);
	} else motion.drift = multiply(motion.direction, definition.speed * 65536);
	if (face) motion.position = particle_face_point(*face, [&](int bound) { return random.bounded(bound); });
	else {
		const int spread = geometry_default_magnitude(motion.drift) / 16;
		const auto average = [](const ParticlePoint &right, const ParticlePoint &up) {
			auto result = add(right, up);
			for (auto &component : result) component /= 2;
			return result;
		};
		const auto offset = average(multiply(orientation[0], spread - scaled_random(random, signed_checked(2LL * spread))),
		                            multiply(orientation[1], spread - scaled_random(random, signed_checked(2LL * spread))));
		motion.position = add(add(position, offset), multiply(orientation[2], 32768 - scaled_random(random, 65536)));
	}
	motion.start = motion.position;
	motion.radius = -definition.scale;
	if constexpr (Snow) motion.radius = motion.radius / 20 + float(scaled_random(random, int(9 * motion.radius / 20)));
	else {
		motion.radius *= 2;
		(void) random.bounded(9);
		(void) random.bounded(9);
	}
	motion.width = motion.height = motion.radius / 65536.0f;
	motion.radius /= 65536.0f;
	motion.frame = 0;
	motion.rotation = 0;
	motion.orientation = 1;
	motion.ttl = motion.life;
	if (motion.ttl <= 0) throw std::runtime_error("XL weather lifetime is empty");
	return result;
}

template <typename World>
bool advance_weather_particle(WeatherParticle &particle, int time, World &world, bool collisions)
{
	using namespace particle_detail;
	auto &motion = particle.motion;
	if (motion.life <= 0) return false;
	const int elapsed = signed_checked(std::int64_t(time) - motion.updated);
	if (elapsed < 0) throw std::runtime_error("XL weather clock moved backwards");
	if (particle.color[3] < 0.01f) {
		motion.life = -1;
		return true;
	}
	motion.updated = time;
	const auto drift = geometry_default_direction(motion.drift), direction = geometry_default_direction(motion.direction);
	const int drag = fixed_vector_dot(drift, direction) < 0 ? -65536 : 65536;
	motion.position = add(motion.position, multiply(particle.snow ? motion.direction : motion.drift, drag));
	if (std::int64_t(time) - motion.moved >= 250) {
		motion.moved = time;
		int segment = motion.segment;
		const int previous = motion.segment;
		if (motion.segment < -1) ++motion.segment;
		if (motion.segment >= -1) {
			segment = world.locate(motion.position, motion.skybox, motion.segment);
			if (segment < 0) motion.segment = --motion.locate_delay;
		}
		if (segment < 0 && motion.ttl - motion.life > 500) {
			motion.life = -1;
			return false;
		}
		if (segment >= 0) {
			if (world.water(segment) || world.lava(segment)) {
				motion.life = -1;
				return false;
			}
			if (previous >= 0 && segment != previous && world.segments.at(std::size_t(previous)).skybox && motion.ttl - motion.life > 500) {
				motion.life = -1;
				return false;
			}
		}
		motion.segment = segment;
		if (collisions) {
			const auto normal = world.collide(motion);
			if (!normal) motion.bounce = 1;
			else if (!--motion.bounce) return false;
		}
	}
	motion.life = std::max(0, motion.life - elapsed);
	return true;
}
} // namespace xl_compat
#endif
