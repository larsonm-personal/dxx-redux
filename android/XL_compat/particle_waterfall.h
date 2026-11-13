#ifndef DXX_XL_PARTICLE_WATERFALL_H
#define DXX_XL_PARTICLE_WATERFALL_H
#include "particle_fire.h"

namespace xl_compat
{
struct WaterfallParticle {
	FireParticle motion;
	std::array<float, 4> color{}, rendered{};
	bool checked = false;
};

inline void waterfall_color(WaterfallParticle &particle)
{
	auto &motion = particle.motion;
	const float pivot = std::min(float(motion.ttl) / 4000.0f, 0.25f);
	const float lived = float(motion.ttl - motion.life) / 1000.0f;
	float fade = lived < pivot ? lived / pivot : 1.0f - (lived - pivot) / (1.0f - pivot);
	fade = fade < 0 ? 0.05f : std::min(0.1f + 0.9f * fade, 1.0f);
	particle.rendered = particle.color;
	particle.rendered[3] *= fade;
	if (motion.decay < 0.75f && particle.rendered[3] < 0.01f) motion.life = -1;
}

template <typename Random>
WaterfallParticle create_waterfall_particle(const ParticleEmitterDefinition &definition, const ParticlePoint &position,
                                            const ParticleFace *face, int segment, bool skybox, int time, Random &random)
{
	using namespace particle_detail;
	if (definition.image != 6 || definition.fade != 2 || definition.life >= 0 || definition.life == INT32_MIN ||
	    definition.speed < 0 || definition.speed > INT32_MAX / 65536 || !(definition.scale < 0) ||
	    !std::isfinite(definition.scale) || double(-definition.scale) > INT32_MAX)
		throw std::runtime_error("Invalid XL authored waterfall particle definition");
	WaterfallParticle result;
	auto &motion = result.motion;
	motion.segment = segment;
	motion.skybox = skybox;
	motion.updated = motion.moved = time;
	motion.life = -definition.life;
	motion.bounce = 1;
	motion.rotates = false;
	motion.decay = 1;
	for (unsigned i = 0; i < 4; ++i) result.color[i] = std::min(definition.color[i], 1.0f);
	if (definition.color[3] < 0) {
		const auto alpha = static_cast<unsigned char>(-definition.color[3] * 255.0f * 0.25f + 0.5f);
		result.color[3] = float(3 * alpha + scaled_random(random, 2 * alpha)) / 255.0f;
	}
	motion.direction = definition.direction;
	ParticlePoint jitter;
	for (auto &component : jitter) component = scaled_random(random, 16384) - 8192;
	motion.drift = geometry_default_direction(add(motion.direction, jitter));
	if (fixed_vector_dot(motion.direction, motion.drift) < 32768) {
		motion.life = 0;
		return result;
	}
	const float angle = float(std::int16_t(fixed_normalized_angle(geometry_default_direction(motion.drift), geometry_default_direction(motion.direction))));
	int speed = definition.speed * 65536;
	if (angle) speed = int(float(speed) / float(::exp(8192 / angle)));
	motion.drift = multiply(motion.drift, speed);
	motion.position = face ? particle_face_point(*face, [&](int bound) { return random.bounded(bound); }) : add(position, multiply(motion.drift, 1024));
	motion.start = motion.position;
	motion.radius = -definition.scale * 2.0f;
	(void) random.bounded(9);
	(void) random.bounded(9);
	motion.width = motion.radius * (definition.blow_up ? 0.6666667f : 0.3333333f) / 65536.0f;
	motion.height = motion.radius / 65536.0f;
	motion.radius /= 65536.0f;
	motion.frame = random.bounded(64);
	motion.rotation = random.bounded(64);
	motion.orientation = random.bounded(4);
	motion.ttl = motion.life;
	if (motion.ttl <= 0) throw std::runtime_error("XL waterfall lifetime is empty");
	waterfall_color(result);
	return result;
}

template <typename World>
bool advance_waterfall_particle(WaterfallParticle &particle, int time, World &world, bool collisions)
{
	using namespace particle_detail;
	auto &motion = particle.motion;
	if (motion.life <= 0) return false;
	const int elapsed = signed_checked(std::int64_t(time) - motion.updated);
	if (elapsed < 0) throw std::runtime_error("XL waterfall clock moved backwards");
	waterfall_color(particle);
	if (motion.life < 0) return true;
	motion.updated = time;
	const auto drift = geometry_default_direction(motion.drift), direction = geometry_default_direction(motion.direction);
	motion.position = add(motion.position, multiply(motion.direction, fixed_vector_dot(drift, direction) < 0 ? -65536 : 65536));
	if (std::int64_t(time) - motion.moved >= 250) {
		motion.moved = time;
		int segment = motion.segment;
		if (motion.segment < -1) ++motion.segment;
		if (motion.segment >= -1) {
			segment = world.locate(motion.position, motion.skybox, motion.segment);
			if (segment < 0) motion.segment = --motion.locate_delay;
		}
		if (segment < 0) {
			if (!particle.checked) {
				ParticlePoint displacement;
				for (unsigned axis = 0; axis < 3; ++axis)
					displacement[axis] = signed_checked(std::int64_t(motion.position[axis]) - motion.start[axis]);
				if (geometry_default_magnitude(displacement) >= 65536 &&
				    fixed_vector_dot(geometry_default_direction(displacement), direction) < 32768) {
					motion.life = -1;
					return false;
				}
				particle.checked = true;
				motion.life = 500;
			}
			if (motion.ttl - motion.life > 500) {
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
