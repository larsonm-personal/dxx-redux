#ifndef DXX_XL_LIGHTNING_BOLT_H
#define DXX_XL_LIGHTNING_BOLT_H
#include "lightning_path.h"
#include "lighting.h"
#include <cmath>

#ifdef _MSC_VER
#pragma pack(push, 8)
#endif
namespace xl_compat
{
struct LightningBolt {
	LightningDefinition definition;
	LightningLifetime lifetime;
	LightningPoint base{}, position{}, end{}, direction{};
	std::vector<LightningNode> nodes;
	std::vector<LightningPoint> draw_positions;
	int segment = -1;
};

inline LightningPoint lightning_line_point(const LightningPoint &a, const LightningPoint &b, const LightningPoint &point)
{
	using namespace lightning_detail;
	const auto line = subtract(b, a), relative = subtract(point, a);
	const double magnitude = double(line[0]) * line[0] + double(line[1]) * line[1] + double(line[2]) * line[2];
	if (!magnitude) return a;
	const double fraction = (double(relative[0]) * line[0] + double(relative[1]) * line[1] + double(relative[2]) * line[2]) / magnitude;
	if (fraction < 0) return a;
	if (fraction > 1) return b;
	LightningPoint result;
	for (unsigned axis = 0; axis < 3; ++axis) result[axis] = checked(std::int64_t(a[axis]) + std::int64_t(fraction * line[axis]));
	return result;
}

inline void lightning_bolt_move(LightningBolt &bolt, const LightningPoint &position, int segment)
{
	using namespace lightning_detail;
	if (segment < 0 || bolt.nodes.empty() || bolt.lifetime.nodes < 0) return;
	const auto end = add(bolt.end, subtract(position, bolt.position));
	if (position == bolt.position && end == bolt.end) return;
	const auto line = subtract(end, position);
	const int length = geometry_default_magnitude(line);
	if (!length || !bolt.lifetime.length) throw std::runtime_error("XL lightning relocation has a degenerate length");
	const float scale = (float(length) / 65536.0f) / (float(bolt.lifetime.length) / 65536.0f);
	for (auto &node : bolt.nodes) {
		const auto intersection = lightning_line_point(bolt.position, bolt.end, node.position);
		float offset = float(geometry_default_magnitude(subtract(intersection, bolt.position))) / 65536.0f * scale;
		offset /= float(length) / 65536.0f;
		LightningPoint along;
		for (unsigned axis = 0; axis < 3; ++axis) {
			const float value = float(line[axis]) * offset;
			if (!std::isfinite(value)) throw std::runtime_error("XL lightning relocation is not finite");
			along[axis] = checked(std::int64_t(value));
		}
		const auto delta = subtract(add(position, along), intersection);
		node.next = add(node.next, delta);
		node.base = add(node.base, delta);
		node.position = add(node.position, delta);
	}
	bolt.nodes.front().position = bolt.position = bolt.base = position;
	bolt.nodes.back().position = bolt.end = end;
	bolt.direction = line;
	bolt.lifetime.length = length;
	bolt.segment = segment;
}

template <class Random>
LightningPoint lightning_random_direction(Random &random)
{
	LightningPoint direction;
	do {
		for (auto &axis : direction) axis = (90 - random.bounded(181)) * (65536 / 90);
	} while (direction == LightningPoint{});
	return geometry_default_direction(direction);
}

inline int lightning_dot(const LightningPoint &a, const LightningPoint &b)
{
	return lightning_detail::checked(std::int64_t((double(a[0]) * b[0] + double(a[1]) * b[1] + double(a[2]) * b[2]) / 65536.0));
}

template <class Random>
void lightning_bolt_setup(LightningBolt &bolt, Random &random)
{
	using namespace lightning_detail;
	auto &state = bolt.lifetime;
	bolt.position = bolt.base;
	bolt.direction = lightning_random_direction(random);
	bolt.end = add(bolt.position, multiply(bolt.direction, state.length));
	if (bolt.definition.offset) {
		const int offset = checked(std::int64_t(bolt.definition.offset / 2) + int(random.unit() * bolt.definition.offset / 2));
		const auto delta = multiply(bolt.direction, offset);
		bolt.position = add(bolt.position, delta);
		bolt.end = add(bolt.end, delta);
	}
	std::array<LightningPoint, 2> plane;
	do {
		plane[0] = lightning_random_direction(random);
	} while (magnitude(lightning_dot(bolt.direction, plane[0])) > 65536 * 9 / 10);
	plane[1] = geometry_default_normal(bolt.position, bolt.end, plane[0]);
	plane[0] = geometry_default_normal(bolt.position, bolt.end, add(bolt.position, plane[1]));
	const int divisor = checked(std::int64_t(state.nodes - 1) * 65536);
	if (!divisor) throw std::runtime_error("XL lightning node spacing has a zero divisor");
	const int scale = checked(std::int64_t(double(state.length) / divisor * 65536.0));
	const auto step = multiply(bolt.direction, scale);
	state.nodes = magnitude(state.nodes);
	state.step = 0;
	if (std::size_t(state.nodes) != bolt.nodes.size()) throw std::runtime_error("XL lightning node capacity changed during restart");
	auto position = bolt.position;
	for (auto &node : bolt.nodes) {
		lightning_node_setup(node, position, plane);
		position = add(position, step);
	}
	state.frames = -magnitude(state.frames);
}

template <class Random>
LightningBolt create_lightning_bolt(const LightningDefinition &definition, const LightningPoint &position, Random &random)
{
	// Complete original Pumo profile; unsupported geometry must never enter this owner
	if (!definition.random_end || definition.angle || definition.in_plane || definition.children || definition.clamp ||
	    definition.style != 0 || definition.frames <= 0 || definition.nodes > 32767)
		throw std::runtime_error("XL lightning bolt requires the implemented random style-0 profile");
	LightningBolt bolt{ definition, lightning_lifetime(definition, false, false) };
	bolt.base = position;
	bolt.nodes.resize(bolt.lifetime.nodes);
	auto &state = bolt.lifetime;
	state.ttl = lightning_detail::checked(std::int64_t(3) * state.ttl) / 4 + int(random.unit() * state.ttl / 2);
	state.length = lightning_detail::checked(std::int64_t(3) * state.length) / 4 + int(random.unit() * state.length / 2);
	lightning_bolt_setup(bolt, random);
	return bolt;
}

template <class Random>
void lightning_bolt_animate(LightningBolt &bolt, int elapsed, Random &random)
{
	auto &state = bolt.lifetime;
	if (elapsed < 0) throw std::runtime_error("XL lightning frame time is negative");
	state.ttl = lightning_detail::checked(std::int64_t(state.ttl) - elapsed);
	if (state.nodes > 0) {
		const bool initial = state.frames < 0;
		if (initial) state.frames = -state.frames;
		if (!state.step) {
			lightning_random_erratic_path(bolt.nodes, state.frames, bolt.definition.amplitude, false, [&] { return random.unit(); });
			state.step = state.frames;
		}
		lightning_animate_nodes(bolt.nodes, initial, true);
		// Source RenderSetup precedes lifetime restart and owns the drawn coordinates
		bolt.draw_positions.clear();
		for (const auto &node : bolt.nodes) bolt.draw_positions.push_back(node.position);
		--state.step;
	}
}

template <class Random>
bool lightning_bolt_set_life(LightningBolt &bolt, Random &random)
{
	const auto action = lightning_set_life(bolt.lifetime, [&] { return random.unit(); });
	if (action == LightningLifeAction::expire) return false;
	if (action == LightningLifeAction::restart) lightning_bolt_setup(bolt, random);
	return true;
}

template <class Random>
bool lightning_bolt_frame(LightningBolt &bolt, int elapsed, Random &random)
{
	lightning_bolt_animate(bolt, elapsed, random);
	return lightning_bolt_set_life(bolt, random);
}
} // namespace xl_compat
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#endif
