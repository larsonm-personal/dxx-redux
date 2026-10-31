#ifndef DXX_XL_PARTICLE_SMOKE_H
#define DXX_XL_PARTICLE_SMOKE_H
#include <array>
#include <cmath>
#include "particle_fire.h"

namespace xl_compat
{
// Authored smoke uses fade type 1 and a linear remaining-life decay
struct SmokeColor {
	std::array<float, 4> color{}, rendered{};
	int life = 0, ttl = 1;
	bool bright = false;
	std::array<float, 4> target{};
	int fade_time = 0;
	bool emissive = false;
};

template <typename Random>
bool setup_smoke_color(SmokeColor &particle, float brightness, Random &random)
{
	constexpr double pi = 3.141592653589793240;
	if (particle.bright) brightness = float(std::sqrt(brightness));
	particle.rendered = particle.color;
	brightness *= 0.9f + float(random.bounded(1000)) / 5000.0f;
	for (unsigned i = 0; i < 3; ++i) particle.rendered[i] *= brightness;
	const float decay = float(particle.life) / float(particle.ttl);
	const float elapsed = 1.0f - decay;
	particle.rendered[3] *= float(std::sin(double(elapsed * elapsed) * pi * 1.5) * 0.5 + 0.5);
	if (decay >= 0.666f || particle.rendered[3] >= 0.01f) return true;
	particle.life = -1;
	return false;
}

template <typename Random>
void update_smoke_color(SmokeColor &particle, float brightness, Random &random)
{
	if (particle.fade_time > 0) {
		if (particle.ttl - particle.life < particle.fade_time) {
			particle.color[0] *= 0.95f;
			particle.color[1] *= 0.8f;
		} else {
			particle.color = particle.target;
			particle.emissive = false;
			particle.fade_time = -1;
		}
	} else if (particle.fade_time == 0) {
		for (unsigned i = 0; i < 3; ++i) particle.color[i] = particle.target[i] * (0.95f + random.unit() / 20.0f);
		particle.fade_time = -1;
	}
	setup_smoke_color(particle, brightness, random);
}

struct SmokeParticle {
	FireParticle motion;
	SmokeColor appearance;
	bool blow_up = false;
	int rotation_phase = 1;
};

template <typename Random, typename World>
bool advance_smoke_particle(SmokeParticle &particle, int time, float brightness, Random &random, World &world, bool collisions)
{
	auto &motion = particle.motion;
	auto &color = particle.appearance;
	if (motion.life <= 0) return false;
	const int elapsed = particle_detail::signed_checked(std::int64_t(time) - motion.updated);
	color.life = motion.life;
	update_smoke_color(color, brightness, random);
	motion.life = color.life;
	if (motion.life < 0) return true;
	const int drag = motion.decay > 0.9f ? int(std::sqrt((1.0f - motion.decay) / 0.1f) * 65536) : 65536;
	if (!advance_particle_motion(motion, time, random, world, collisions, drag)) return false;
	motion.life = std::max(0, motion.life - elapsed);
	motion.decay = float(motion.life) / float(motion.ttl);
	color.life = motion.life;
	if (motion.radius > 0) {
		if (particle.blow_up) {
			if (motion.width >= motion.radius) motion.radius = 0;
			else {
				motion.width = std::min(motion.width + motion.radius * 0.1f, motion.radius);
				motion.height = std::min(motion.height + motion.radius * 0.1f, motion.radius);
				color.color[3] = std::min(color.color[3] * (1.0f + 0.0725f), 1.0f);
			}
		} else {
			if (motion.width <= motion.radius) motion.radius = 0;
			else {
				motion.radius *= 1.2f;
				color.color[3] = std::min(color.color[3] * 1.0725f, 1.0f);
			}
		}
	}
	return true;
}

// Authored nonpositive alpha, quality 2, default alpha setting, stationary owner
template <typename Random>
SmokeParticle create_smoke_particle(const ParticleEmitterDefinition &definition, const ParticlePoint &position, const ParticleFace *face, int segment, bool skybox, int time, float brightness, Random &random)
{
	using namespace particle_detail;
	if (definition.image != 1 || definition.fade != 1 || definition.color[3] > 0 || definition.life >= 0 || definition.life == INT32_MIN || definition.speed < 0 || definition.speed > INT32_MAX / 65536 || !(definition.scale < 0) || !std::isfinite(definition.scale) || double(-definition.scale) > INT32_MAX)
		throw std::runtime_error("Invalid XL authored smoke particle definition");
	SmokeParticle result;
	auto &motion = result.motion;
	auto &color = result.appearance;
	result.blow_up = definition.blow_up;
	motion.segment = segment;
	motion.skybox = skybox;
	motion.updated = motion.moved = time;
	motion.life = -definition.life;
	for (unsigned i = 0; i < 4; ++i) color.color[i] = color.target[i] = std::min(definition.color[i], 1.0f);
	color.bright = random.bounded(50) == 0;
	for (unsigned i = 0; i < 3; ++i) color.color[i] *= 0.95f + random.unit() / 20.0f;
	if (definition.color[3] < 0) {
		const auto alpha = static_cast<unsigned char>(-definition.color[3] * 255.0f * 0.25f + 0.5f);
		color.target[3] = float(3 * alpha + scaled_random(random, 2 * alpha)) / 255.0f;
	} else {
		const int alpha = result.blow_up ? 128 : 160;
		color.target[3] = float(3 * alpha / 4 + scaled_random(random, alpha / 2)) / 255.0f;
	}
	if (result.blow_up && !color.bright) {
		// Source carries this modified argument into its final SetupColor call
		brightness = 1.0f - brightness;
		color.target[3] += brightness * brightness / 8.0f;
	}
	const float r = std::min(definition.color[0], 1.0f) * 255.0f;
	const float g = std::min(definition.color[1], 1.0f) * 255.0f;
	const float b = std::min(definition.color[2], 1.0f) * 255.0f;
	const float luminance = std::sqrt((r * r + g * g + b * b) / 3.0f) / 255.0f;
	color.target[3] /= 3.0f - luminance;
	color.color[3] = color.target[3];
	motion.direction = definition.direction;
	ParticlePoint jitter;
	for (auto &component : jitter) component = scaled_random(random, 16384) - 8192;
	motion.drift = geometry_default_direction(add(motion.direction, jitter));
	const float angle = float(std::int16_t(fixed_normalized_angle(geometry_default_direction(motion.drift), geometry_default_direction(motion.direction))));
	int speed = definition.speed * 65536;
	if (angle) speed = int(float(speed) / float(::exp(8192 / angle)));
	motion.drift = multiply(motion.drift, speed);
	motion.direction = multiply(motion.direction, 49152 + 1024 * scaled_random(random, 16));
	motion.position = face ? particle_face_point(*face, [&](int bound) { return random.bounded(bound); }) : add(position, multiply(motion.drift, 1024));
	motion.start = motion.position;
	motion.radius = -definition.scale;
	if (result.blow_up) motion.life = signed_checked(2LL * motion.life) / 3;
	motion.life = signed_checked(4LL * motion.life) / 5 + scaled_random(random, signed_checked(2LL * motion.life) / 5);
	motion.radius += float(scaled_random(random, int(motion.radius)));
	(void) random.bounded(9);
	(void) random.bounded(9);
	motion.width = motion.height = motion.radius / 65536.0f;
	motion.radius /= 65536.0f;
	motion.frame = random.bounded(64);
	motion.rotation = random.bounded(64);
	motion.orientation = random.bounded(4);
	motion.ttl = motion.life;
	if (motion.ttl <= 0) throw std::runtime_error("XL smoke particle lifetime is empty");
	motion.decay = float(motion.life) / float(motion.ttl);
	color.life = motion.life;
	color.ttl = motion.ttl;
	setup_smoke_color(color, brightness, random);
	motion.life = color.life;
	return result;
}
} // namespace xl_compat
#endif
