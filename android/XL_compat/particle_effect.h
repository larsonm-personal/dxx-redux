#ifndef DXX_XL_PARTICLE_EFFECT_H
#define DXX_XL_PARTICLE_EFFECT_H
#include "inventory.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

#ifdef _MSC_VER
#pragma pack(push, 8)
#endif
namespace xl_compat
{
// Serialized effect subtype; these are not the renderer's particle image IDs
enum class ParticleKind { smoke,
	                      spray,
	                      bubbles,
	                      fire,
	                      waterfall,
	                      rain,
	                      snow };

struct ParticleEffect {
	int life, particles, speed, drift, brightness;
	std::array<int, 2> size;
	std::array<int, 4> color;
	int side;
	ParticleKind kind;
	bool enabled;
};

struct ParticleEmitterDefinition {
	int image, fade, count, life, speed, brightness;
	float scale;
	std::array<int, 3> direction;
	std::array<float, 4> color;
	bool blow_up, bubbles;
};

namespace particle_detail
{
inline int checked(std::int64_t value)
{
	if (value < 0 || value > std::numeric_limits<int>::max())
		throw std::runtime_error("XL particle definition exceeds the source arithmetic range");
	return int(value);
}
inline int triangle(int value)
{
	// Check the source's intermediate product before division
	return checked(std::int64_t(value) * (std::int64_t(value) + 1)) / 2;
}
} // namespace particle_detail

inline ParticleEffect decode_particle_effect(const Object &object, int level_version, float face_size = 1.0f, bool water_texture = false)
{
	using particle_detail::checked;
	const auto &p = object.render_data;
	if (object.render != 11 || p.size() != 13)
		throw std::runtime_error("XL particle object has an invalid payload layout");
	for (unsigned i = 0; i < 6; ++i)
		if (p[i] < 0) throw std::runtime_error("XL particle parameters must be nonnegative");
	for (unsigned i = 6; i < 13; ++i)
		if (p[i] < 0 || p[i] > 255) throw std::runtime_error("XL particle payload contains a non-byte value");
	if (p[10] > 6) throw std::runtime_error("XL particle emitter references an invalid side");
	if (p[11] > int(ParticleKind::snow)) throw std::runtime_error("XL particle subtype is unsupported");
	const int life = p[0] ? p[0] : 5, size = p[1] ? p[1] : 5;
	const int density = p[2] ? p[2] : 5, speed = p[3] ? p[3] : 5;
	if (density >= 11 || speed >= 31)
		throw std::runtime_error("XL particle density or speed exceeds the source arithmetic range");
	ParticleEffect result{};
	result.life = particle_detail::triangle(life);
	result.speed = particle_detail::triangle(speed);
	result.brightness = p[5] ? checked(std::int64_t(p[5]) * 10) : 50;
	result.drift = p[4] ? checked(std::int64_t(checked(std::int64_t(speed) * p[4])) * 75) : checked(std::int64_t(result.speed) * 50);
	result.size = { checked(std::int64_t(size) + 1), particle_detail::triangle(size) };
	const int count = checked(std::int64_t(checked(std::int64_t(density) * result.life)) * 3);
	result.particles = checked(90LL + checked(std::int64_t(count) * (std::int64_t(1) << speed)) / (11 - density));
	result.color = { p[6], p[7], p[8], p[9] };
	result.side = p[10];
	result.kind = ParticleKind(p[11]);
	result.enabled = p[12] != 0;
	if (result.side > 0) {
		if (!std::isfinite(face_size) || face_size < 0)
			throw std::runtime_error("XL particle emitter has an invalid face size");
		const float scaled = float(result.particles) * (face_size < 1 ? std::sqrt(face_size) : face_size);
		if (!std::isfinite(scaled) || double(scaled) > std::numeric_limits<int>::max())
			throw std::runtime_error("XL particle face density exceeds the source arithmetic range");
		result.particles = int(scaled);
		if ((level_version >= 18 && result.kind == ParticleKind::spray) || (level_version < 18 && water_texture))
			result.particles = checked(std::int64_t(result.particles) * 4);
	}
	if (result.kind == ParticleKind::bubbles) result.particles = checked(std::int64_t(result.particles) * 2);
	return result;
}

inline ParticleEmitterDefinition particle_emitter_definition(const ParticleEffect &effect, const std::array<int, 3> &forward, bool disperse)
{
	using particle_detail::checked;
	const int kind = int(effect.kind);
	const int type = kind == 4 ? 3 : kind == 3 ? 2
	                             : kind == 2   ? 1
	                             : kind >= 5   ? kind
	                                           : 0;
	constexpr int images[] = { 1, 2, 5, 6, 0, 3, 4 };
	ParticleEmitterDefinition result{};
	result.image = images[type];
	result.fade = type == 3 ? 2 : type == 2              ? 3
	                          : (type == 1 || type >= 5) ? -1
	                                                     : 1;
	// The source compares its remapped type against serialized constants here
	const int divisor = (type == 5 || type == 3 || type == 6) ? 10 : 1;
	result.count = std::min((effect.particles / divisor) >> int(!disperse), 100000);
	const int size = effect.size[disperse ? 1 : 0];
	const int radius = disperse ? checked(65536LL * (std::int64_t(size) + 1)) : checked(std::int64_t(checked(65536LL * (std::int64_t(size) + 1))) * (std::int64_t(size) + 2)) / 2;
	const float scale = float(radius) / (type == 1 ? 4.0f : 2.0f);
	result.scale = -float(scale + 0.5f);
	result.life = -(type == 2 ? checked(800LL * int(std::sqrt(double(effect.life)))) : checked(3200LL * effect.life));
	result.speed = type == 5 ? int(std::sqrt(float(effect.drift))) : effect.drift;
	result.brightness = effect.brightness;
	const int direction = checked(std::int64_t(effect.speed) * (2 * 65536)) / 55;
	for (unsigned axis = 0; axis < 3; ++axis) {
		// Pinned FixMul uses double arithmetic and truncation toward zero
		const double component = double(direction) / 65536.0 * double(forward[axis]);
		if (component < INT32_MIN || component > INT32_MAX)
			throw std::runtime_error("XL particle direction exceeds the source fixed-point range");
		result.direction[axis] = int(component);
		result.color[axis] = float(effect.color[axis]) / 255.0f;
	}
	if (result.color[0] + result.color[1] + result.color[2] > 0)
		result.color[3] = -float(effect.color[3]) / 255.0f;
	else
		result.color = type == 0 ? std::array<float, 4>{ 0.5f, 0.5f, 0.5f, 0.0f } : type == 1 ? std::array<float, 4>{ 0.8f, 0.9f, 1.0f, 1.0f }
		                                                                                      : std::array<float, 4>{ 1, 1, 1, 1 };
	result.blow_up = type != 2;
	result.bubbles = type == 1;
	return result;
}
} // namespace xl_compat
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#endif
