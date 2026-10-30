#include "fixed_angle.h"
#include "audio_spatial.h"
#include "runtime/visibility.h"
#include <cmath>
#include <limits>
#include <stdexcept>
namespace xl_compat
{
namespace
{
FixedLightPoint point_difference(const FixedLightPoint &a, const FixedLightPoint &b)
{
	FixedLightPoint delta;
	for (unsigned i = 0; i < 3; ++i) {
		const auto value = std::int64_t(a[i]) - b[i];
		if (value < INT32_MIN || value > INT32_MAX) throw std::runtime_error("XL sound position difference exceeds fixed range");
		delta[i] = int(value);
	}
	return delta;
}
int point_distance(const FixedLightPoint &a, const FixedLightPoint &b)
{
	return geometry_default_magnitude(point_difference(a, b));
}
} // namespace
int audio_point_distance(const FixedLightPoint &a, const FixedLightPoint &b)
{
	return point_distance(a, b);
}
int audio_extended_range(int max_distance, int decay)
{
	if (max_distance <= 0 || max_distance > INT32_MAX / (decay ? 2 : 5)) throw std::runtime_error("XL sound range multiplication exceeds the defined source range");
	return decay ? max_distance * 2 : max_distance * 5 / 4;
}
std::int16_t audio_ear_angle(const FixedLightPoint &right, const FixedLightPoint &up, const FixedLightPoint &listener, const FixedLightPoint &sound)
{
	const auto direction = geometry_default_direction(point_difference(sound, listener));
	int angle = fixed_normalized_angle(right, direction);
	if (!angle) return 0;
	FixedLightPoint cross;
	for (unsigned axis = 0; axis < 3; ++axis) {
		const auto a = (axis + 1) % 3, b = (axis + 2) % 3;
		const auto product = std::int64_t(right[a]) * direction[b] - std::int64_t(right[b]) * direction[a];
		const auto value = product / 65536 - (product < 0 && product % 65536 != 0);
		if (value < INT32_MIN || value > INT32_MAX) throw std::runtime_error("XL ear-angle cross product exceeds fixed range");
		cross[axis] = int(value);
	}
	if (fixed_vector_dot(cross, up) < 0) angle = -angle;
	return std::int16_t(angle);
}
int audio_geometry_distance(const ClassicRouteGraph &graph, const std::vector<std::vector<bool>> &visibility, const std::vector<GeometryRouteNode> &routes, int listener, int sound, const FixedLightPoint &listener_position, const FixedLightPoint &sound_position, int max_distance, int decay)
{
	const auto count = graph.centers.size();
	if (listener < 0 || sound < 0 || unsigned(listener) >= count || unsigned(sound) >= count || graph.edges.size() != count || visibility.size() != count || routes.size() != count || max_distance <= 0)
		throw std::runtime_error("Invalid XL sound routing context");
	const int limit = audio_extended_range(max_distance, decay);
	int distance = point_distance(listener_position, sound_position);
	if (distance > limit) return -1;
	if (listener == sound) return distance;
	const auto path = geometry_route_path(routes, sound);
	if (path.empty()) return -1;
	if (path.front() != listener) throw std::runtime_error("XL sound route belongs to another listener");
	const auto visible = [&](int a, int b) { return bool(visibility.at(unsigned(a)).at(unsigned(b))); };
	const auto interior = geometry_shorten_route(path, visible, [&](int a, int b) { return point_distance(graph.centers.at(unsigned(a)), graph.centers.at(unsigned(b))); });
	if (visible(listener, sound)) {
		const float adjustment = distance * 0.6f * float(distance) / float(limit);
		if (!std::isfinite(adjustment) || double(adjustment) > INT32_MAX || std::int64_t(distance) + std::int64_t(adjustment) > INT32_MAX)
			throw std::runtime_error("XL visible sound distance exceeds fixed range");
		distance += int(adjustment);
	} else if (path.size() > 2) {
		const auto edge_distance = [&](int from, int to) {
			for (const auto &edge : graph.edges.at(unsigned(from)))
				if (edge.child == to) return edge.distance;
			throw std::runtime_error("XL sound route has no reciprocal endpoint edge");
		};
		const auto adjusted = std::int64_t(interior.distance) - edge_distance(listener, path[1]) + point_distance(listener_position, graph.centers[unsigned(path[1])]) - edge_distance(sound, path[path.size() - 2]) + point_distance(sound_position, graph.centers[unsigned(path[path.size() - 2])]);
		if (adjusted > INT32_MAX) throw std::runtime_error("XL routed sound distance exceeds fixed range");
		if (adjusted > 0) distance = int(adjusted);
	}
	return distance < limit ? distance : -1;
}
AudioSpatialOutput audio_spatial_output(int distance, int max_volume, int max_distance, int decay, std::int16_t ear_angle, bool reverse_channels, int hires_profile)
{
	if (max_volume < 0 || max_distance <= 0 || hires_profile < 0 || hires_profile > 2)
		throw std::runtime_error("Invalid XL spatial sound controls");
	AudioSpatialOutput result;
	if (distance < 0) return result;
	float volume;
	if (!decay) volume = float(max_volume) * (1.0f - float(distance) / float(max_distance));
	else if (decay == 1) {
		if (max_distance < 2) throw std::runtime_error("XL exponential sound range is too small");
		// Match the source's global math overloads, including platform rounding
		const float attenuation = float(::exp(-::log(2.0f) * 4.0f * (float(distance) / 65536.0f) / (float(max_distance / 2) / 65536.0f)));
		volume = max_volume * attenuation;
	} else {
		const float attenuation = 1.0f - (float(distance) / 65536.0f) / (float(max_distance) / 65536.0f);
		volume = max_volume * attenuation * attenuation * attenuation;
	}
	if (!std::isfinite(volume) || double(volume) < std::numeric_limits<int>::min() || double(volume) > std::numeric_limits<int>::max())
		throw std::runtime_error("XL spatial volume exceeds the defined source range");
	result.volume = int(volume);
	if (result.volume <= 0) {
		result.volume = 0;
		return result;
	}
	constexpr double pi = 3.141592653589793240;
	const double radians = (ear_angle * 2.0 * pi) / 65536.0;
	int cosine = int(std::cos(radians) * 65536.0);
	if (int(reverse_channels) != hires_profile) cosine = -cosine;
	result.pan = (cosine + 65536) / 2;
	return result;
}
} // namespace xl_compat
