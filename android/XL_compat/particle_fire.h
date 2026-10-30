#ifndef DXX_XL_PARTICLE_FIRE_H
#define DXX_XL_PARTICLE_FIRE_H
#include "fixed_angle.h"
#include "lighting.h"
#include "particle_effect.h"
#include "particle_geometry.h"

#ifdef _MSC_VER
#pragma pack(push, 8)
#endif
namespace xl_compat
{
struct FireParticle {
	ParticlePoint position{}, start{}, drift{}, direction{};
	int segment = -1, life = 0, ttl = 0, updated = 0, moved = 0;
	int frame = 0, rotation = 0, orientation = 0, bounce = 2, locate_delay = -2;
	float radius = 0, width = 0, height = 0, decay = 0;
	bool skybox = false;
};

namespace particle_detail
{
inline int signed_checked(std::int64_t value)
{
	if (value < INT32_MIN || value > INT32_MAX) throw std::runtime_error("XL particle motion exceeds the source fixed-point range");
	return int(value);
}
inline ParticlePoint multiply(const ParticlePoint &value, int scale)
{
	ParticlePoint result{};
	for (unsigned axis = 0; axis < 3; ++axis) result[axis] = signed_checked(std::int64_t(double(value[axis]) / 65536.0 * scale));
	return result;
}
inline ParticlePoint add(const ParticlePoint &a, const ParticlePoint &b)
{
	return { signed_checked(std::int64_t(a[0]) + b[0]), signed_checked(std::int64_t(a[1]) + b[1]), signed_checked(std::int64_t(a[2]) + b[2]) };
}
template <typename Random>
int scaled_random(Random &random, int limit)
{
	// RandN uses float multiplication, including negative limits and the endpoint
	return limit ? signed_checked(std::int64_t(random.unit() * float(limit))) : 0;
}
inline float fire_decay(int life, int ttl)
{
	constexpr double pi = 3.141592653589793240;
	return float(std::sin(double(life) / double(ttl) * pi));
}
} // namespace particle_detail

// Authored fire path of the active XL Create body; visuals use white additive
// fire/smoke atlas cells, regardless of the authored RGB/alpha values
template <typename Random>
FireParticle create_fire_particle(const ParticleEmitterDefinition &definition, const ParticlePoint &position, const ParticleFace *face, int segment, bool skybox, int time, Random &random)
{
	using namespace particle_detail;
	if (definition.image != 5 || definition.life >= 0 || definition.life == INT32_MIN || definition.speed < 0 || definition.speed > INT32_MAX / 65536 || !(definition.scale < 0) || !std::isfinite(definition.scale) || double(-definition.scale) > INT32_MAX)
		throw std::runtime_error("Invalid XL authored fire particle definition");
	FireParticle result;
	result.segment = segment;
	result.skybox = skybox;
	result.updated = result.moved = time;
	result.life = -definition.life;
	result.direction = definition.direction;
	ParticlePoint jitter;
	for (auto &component : jitter) component = scaled_random(random, 16384) - 8192;
	result.drift = geometry_default_direction(add(result.direction, jitter));
	const float angle = float(std::int16_t(fixed_normalized_angle(geometry_default_direction(result.drift), geometry_default_direction(result.direction))));
	int speed = definition.speed * 65536;
	if (angle) speed = int(float(speed) / float(::exp(8192 / angle)));
	result.drift = multiply(result.drift, speed);
	result.direction = multiply(result.direction, 49152 + 1024 * scaled_random(random, 16));
	if (face) result.position = particle_face_point(*face, [&](int bound) { return random.bounded(bound); });
	else result.position = add(position, multiply(result.drift, 1024));
	result.start = result.position;
	result.radius = -definition.scale;
	result.life = signed_checked(3LL * result.life) / 4 + scaled_random(random, result.life / 4);
	result.radius += float(scaled_random(random, int(result.radius)));
	// Fire billboards do not consume the orientation matrix, but XL draws two
	// random indices to construct it before choosing the atlas cell
	(void) random.bounded(9);
	(void) random.bounded(9);
	result.width = result.height = result.radius / 65536.0f;
	result.radius /= 65536.0f;
	result.frame = random.bounded(10) < 6 ? 0 : 2;
	result.life = result.frame < 2 ? signed_checked(9LL * result.life) / 10 : signed_checked(10LL * result.life) / 9;
	result.rotation = random.bounded(64);
	result.orientation = random.bounded(4);
	result.ttl = result.life;
	if (result.ttl <= 0) throw std::runtime_error("XL fire particle lifetime is empty");
	result.decay = fire_decay(result.life, result.ttl);
	return result;
}

// The movement portion precedes source segment lookup and collision handling
// Callers must perform those consumers before completing the lifetime update
template <typename Random>
void move_fire_particle(FireParticle &particle, int time, Random &random)
{
	using namespace particle_detail;
	int elapsed = signed_checked(std::int64_t(time) - particle.updated);
	if (elapsed < 0) throw std::runtime_error("XL particle clock moved backwards");
	if (particle.decay > 0.9f) elapsed = int(elapsed * ::sqrt((1.0f - particle.decay) / 0.1f));
	particle.updated = time;
	particle.position = add(particle.position, multiply(particle.drift, elapsed));
	for (auto &component : particle.drift) {
		const int variation = component ? component / 2 - scaled_random(random, component) : 0;
		component = signed_checked(std::int64_t(component) * 10 + variation) / 10;
	}
	const auto drift = geometry_default_direction(particle.drift), direction = geometry_default_direction(particle.direction);
	particle.position = add(particle.position, multiply(particle.direction, fixed_vector_dot(drift, direction) < 0 ? -65536 : 65536));
}

// World supplies exhaustive segment lookup and side-plane collision, including
// portal transitions. A false return alone does not remove a source particle:
// the emitter retires it only when its remaining lifetime is nonpositive
template <typename Random, typename World>
bool advance_fire_particle(FireParticle &particle, int time, Random &random, World &world, bool collisions)
{
	using namespace particle_detail;
	if (particle.life <= 0) return false;
	const int elapsed = signed_checked(std::int64_t(time) - particle.updated);
	move_fire_particle(particle, time, random);
	if (std::int64_t(time) - particle.moved >= 250) {
		particle.moved = time;
		int segment = particle.segment;
		if (particle.segment < -1) ++particle.segment;
		if (particle.segment >= -1) {
			segment = world.locate(particle.position, particle.skybox, particle.segment);
			if (segment < 0) particle.segment = --particle.locate_delay;
		}
		if (segment < 0 && particle.ttl - particle.life > 500) {
			particle.life = -1;
			return false;
		}
		particle.segment = segment;
		if (collisions) {
			const auto normal = world.collide(particle);
			if (!normal) particle.bounce = 2;
			else {
				if (!--particle.bounce) return false;
				const int dot = fixed_vector_dot(particle.drift, *normal);
				if (dot < 65536 / 100) {
					particle.life = -1;
					return false;
				}
				particle.drift = add(particle.drift, add(particle.drift, multiply(*normal, signed_checked(-2LL * dot))));
			}
		}
	}
	particle.life = std::max(0, particle.life - elapsed);
	particle.decay = fire_decay(particle.life, particle.ttl);
	return true;
}
} // namespace xl_compat
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#endif
