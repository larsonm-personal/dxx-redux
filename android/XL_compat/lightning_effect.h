#ifndef DXX_XL_LIGHTNING_EFFECT_H
#define DXX_XL_LIGHTNING_EFFECT_H
#include "inventory.h"
#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>

#ifdef _MSC_VER
#pragma pack(push, 8)
#endif
namespace xl_compat
{
namespace lightning_detail
{
inline int checked(std::int64_t value)
{
	if (value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max())
		throw std::runtime_error("XL lightning exceeds source arithmetic range");
	return int(value);
}
inline int magnitude(int value)
{
	return checked(value < 0 ? -std::int64_t(value) : value);
}
inline int signed_byte(int value)
{
	if (value < 0 || value > 255) throw std::runtime_error("XL lightning contains a non-byte parameter");
	return value < 128 ? value : value - 256;
}
} // namespace lightning_detail

struct LightningDefinition {
	int life, delay, length, amplitude, offset;
	int bolts, nodes, children, frames, angle, smooth, style;
	int clamp, glow, blur, sound;
	float width;
	std::array<float, 4> color;
	bool enabled, random_end, in_plane, light;
};

// Endpoint selection supplies the fixed-point distance when authored length is absent
inline LightningDefinition lightning_definition(const Object &object, int global_style, std::optional<int> endpoint_distance = {})
{
	using namespace lightning_detail;
	const auto &p = object.render_data;
	if (object.render != 12 || p.size() != 26) throw std::runtime_error("XL lightning has an invalid payload layout");
	if (global_style < 0 || global_style > 2) throw std::runtime_error("XL lightning style is outside the source options");
	for (unsigned i = 6; i < 12; ++i)
		if (p[i] < -32768 || p[i] > 32767) throw std::runtime_error("XL lightning contains a non-short parameter");
	for (unsigned i = 12; i < 26; ++i) signed_byte(p[i]);
	LightningDefinition d{};
	d.life = -magnitude(p[0]);
	d.delay = p[1];
	if (p[2] > 0) d.length = checked(std::int64_t(p[2]) * 65536);
	else {
		if (!endpoint_distance || *endpoint_distance < 0) throw std::runtime_error("XL lightning length requires a valid endpoint");
		d.length = *endpoint_distance;
	}
	const int amplitude = checked(std::int64_t(p[3]) * 65536);
	d.amplitude = p[3] > 0 ? amplitude : checked(-std::int64_t(amplitude) * global_style);
	d.offset = checked(std::int64_t(p[4]) * 65536);
	d.bolts = p[6];
	d.nodes = p[9] > 0 ? p[9] : checked(-std::int64_t(p[9]) * global_style);
	d.children = p[10];
	d.frames = p[11];
	d.width = float(signed_byte(p[12]));
	d.angle = signed_byte(p[13]);
	d.style = signed_byte(p[14]);
	d.smooth = signed_byte(p[15]);
	d.clamp = signed_byte(p[16]);
	d.glow = d.blur = signed_byte(p[17]);
	d.sound = signed_byte(p[18]);
	d.random_end = p[19] != 0 && !d.angle;
	d.in_plane = p[20] != 0;
	for (unsigned i = 0; i < 4; ++i) d.color[i] = float(p[21 + i]) / 255.0f;
	d.enabled = p[25] != 0;
	d.light = true;
	return d;
}

struct LightningLifetime {
	int life, ttl, delay, nodes, random_nodes, length, children, frames, step;
	bool forcefield;
};

inline LightningLifetime lightning_lifetime(const LightningDefinition &d, bool has_endpoint, bool children_enabled)
{
	using namespace lightning_detail;
	if (!d.life || !d.length || d.nodes <= 4 || d.nodes > 32767 || d.bolts <= 0 || d.frames < -32767 || d.frames > 32767)
		throw std::runtime_error("XL lightning cannot create the authored bolt population");
	LightningLifetime s{};
	s.random_nodes = !has_endpoint || d.angle > 0 ? d.nodes / 5 : 0;
	s.nodes = checked(std::int64_t(d.nodes) + s.random_nodes);
	if (s.random_nodes > 127 || s.nodes > 32767)
		throw std::runtime_error("XL lightning exceeds source byte or short node capacity");
	s.children = children_enabled ? (d.children < 0 ? (d.nodes - s.random_nodes) / 10 : d.children) : 0;
	if (s.children > s.nodes) s.children = s.nodes;
	s.life = d.life;
	s.ttl = magnitude(d.life);
	s.length = s.random_nodes ? checked(std::int64_t(d.length) * s.nodes) / d.nodes : d.length;
	s.delay = checked(std::int64_t(magnitude(d.delay)) * 10);
	s.frames = checked(-std::int64_t(d.frames));
	s.forcefield = !d.delay && (has_endpoint || d.angle <= 0);
	return s;
}

enum class LightningLifeAction { keep,
	                             restart,
	                             expire };

// Restart asks the geometry owner to execute Setup(0), including signed-node handling
template <class Random>
LightningLifeAction lightning_set_life(LightningLifetime &s, Random &&random)
{
	using namespace lightning_detail;
	if (s.ttl > 0) return LightningLifeAction::keep;
	if (s.life >= 0) return LightningLifeAction::expire;
	if (s.delay > 1 && (s.nodes = -s.nodes) < 0) {
		const int half = s.delay / 2;
		s.ttl = half + int(random() * half);
		return LightningLifeAction::keep;
	}
	if (s.random_nodes) {
		const int life = magnitude(s.life);
		s.ttl = checked(std::int64_t(3) * life) / 4 + int(random() * life / 2);
	} else {
		s.ttl = magnitude(s.life);
		s.nodes = magnitude(s.nodes);
	}
	return LightningLifeAction::restart;
}
} // namespace xl_compat
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#endif
