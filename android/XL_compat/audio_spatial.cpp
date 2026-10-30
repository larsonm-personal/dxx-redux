#include "audio_spatial.h"
#include "runtime/visibility.h"
#include <cmath>
#include <limits>
#include <stdexcept>
namespace xl_compat
{
namespace
{
// Pinned XL inverse-cosine table; checked against maths/tables.cpp by the source oracle
static constexpr std::uint16_t audio_acos_table[258] = {
	16384,
	16343,
	16303,
	16262,
	16221,
	16180,
	16140,
	16099,
	16058,
	16017,
	15976,
	15936,
	15895,
	15854,
	15813,
	15772,
	15732,
	15691,
	15650,
	15609,
	15568,
	15527,
	15487,
	15446,
	15405,
	15364,
	15323,
	15282,
	15241,
	15200,
	15159,
	15118,
	15077,
	15036,
	14995,
	14953,
	14912,
	14871,
	14830,
	14789,
	14748,
	14706,
	14665,
	14624,
	14582,
	14541,
	14500,
	14458,
	14417,
	14375,
	14334,
	14292,
	14250,
	14209,
	14167,
	14125,
	14084,
	14042,
	14000,
	13958,
	13916,
	13874,
	13833,
	13791,
	13748,
	13706,
	13664,
	13622,
	13580,
	13537,
	13495,
	13453,
	13410,
	13368,
	13325,
	13283,
	13240,
	13197,
	13155,
	13112,
	13069,
	13026,
	12983,
	12940,
	12897,
	12854,
	12811,
	12767,
	12724,
	12680,
	12637,
	12593,
	12550,
	12506,
	12462,
	12419,
	12375,
	12331,
	12287,
	12242,
	12198,
	12154,
	12109,
	12065,
	12020,
	11976,
	11931,
	11886,
	11841,
	11796,
	11751,
	11706,
	11661,
	11616,
	11570,
	11525,
	11479,
	11433,
	11387,
	11341,
	11295,
	11249,
	11203,
	11156,
	11110,
	11063,
	11017,
	10970,
	10923,
	10876,
	10828,
	10781,
	10733,
	10686,
	10638,
	10590,
	10542,
	10494,
	10446,
	10397,
	10349,
	10300,
	10251,
	10202,
	10153,
	10103,
	10054,
	10004,
	9954,
	9904,
	9854,
	9804,
	9753,
	9703,
	9652,
	9601,
	9549,
	9498,
	9446,
	9394,
	9342,
	9290,
	9237,
	9185,
	9132,
	9078,
	9025,
	8971,
	8918,
	8863,
	8809,
	8754,
	8700,
	8644,
	8589,
	8533,
	8477,
	8421,
	8365,
	8308,
	8251,
	8193,
	8135,
	8077,
	8019,
	7960,
	7901,
	7841,
	7782,
	7721,
	7661,
	7600,
	7538,
	7477,
	7414,
	7352,
	7289,
	7225,
	7161,
	7096,
	7031,
	6966,
	6900,
	6833,
	6766,
	6698,
	6630,
	6561,
	6492,
	6421,
	6350,
	6279,
	6207,
	6133,
	6060,
	5985,
	5909,
	5833,
	5756,
	5678,
	5599,
	5518,
	5437,
	5355,
	5271,
	5186,
	5100,
	5013,
	4924,
	4834,
	4742,
	4648,
	4553,
	4455,
	4356,
	4254,
	4150,
	4044,
	3935,
	3823,
	3707,
	3588,
	3465,
	3338,
	3206,
	3069,
	2925,
	2774,
	2614,
	2445,
	2263,
	2065,
	1846,
	1598,
	1305,
	922,
	0,
	0,
};

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
int fixed_dot(const FixedLightPoint &a, const FixedLightPoint &b)
{
	const double value = (double(a[0]) * b[0] + double(a[1]) * b[1] + double(a[2]) * b[2]) / 65536.0;
	if (value < INT32_MIN || value > INT32_MAX) throw std::runtime_error("XL ear-angle dot product exceeds fixed range");
	return int(value);
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
	const int dot = fixed_dot(right, direction);
	const auto absolute = dot < 0 ? -std::int64_t(dot) : std::int64_t(dot);
	// The source returns zero even at the negative unit boundary
	if (absolute >= 65536) return 0;
	const unsigned index = unsigned(absolute) >> 8, fraction = unsigned(absolute) & 255;
	const int delta = int(audio_acos_table[index + 1]) - int(audio_acos_table[index]);
	// The table interpolation and returning Cross overload round negative values down
	int angle = int(audio_acos_table[index]) + int(std::floor(double(delta * int(fraction)) / 256.0));
	if (dot < 0) angle = 32768 - angle;
	FixedLightPoint cross;
	for (unsigned axis = 0; axis < 3; ++axis) {
		const auto a = (axis + 1) % 3, b = (axis + 2) % 3;
		const auto product = std::int64_t(right[a]) * direction[b] - std::int64_t(right[b]) * direction[a];
		const auto value = product / 65536 - (product < 0 && product % 65536 != 0);
		if (value < INT32_MIN || value > INT32_MAX) throw std::runtime_error("XL ear-angle cross product exceeds fixed range");
		cross[axis] = int(value);
	}
	if (fixed_dot(cross, up) < 0) angle = -angle;
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
