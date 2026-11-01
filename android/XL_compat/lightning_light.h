#ifndef DXX_XL_LIGHTNING_LIGHT_H
#define DXX_XL_LIGHTNING_LIGHT_H
#ifdef _MSC_VER
#pragma pack(push, 8)
#endif
#include "lightning_bolt.h"
#include <map>
namespace xl_compat
{
struct LightningSegmentLight {
	int segment = -1, brightness = 0, samples = 0;
	LightningPoint position{};
	std::array<float, 4> color{};
};

class LightningLightAccumulator
{
	std::map<int, LightningSegmentLight> lights;

  public:
	void add(int segment, const LightningPoint &position, const std::array<float, 4> &color)
	{
		if (segment < 0) throw std::runtime_error("Lightning light has an invalid segment");
		auto &light = lights[segment];
		light.segment = segment;
		light.samples = lightning_detail::checked(std::int64_t(light.samples) + 1);
		for (unsigned axis = 0; axis < 3; ++axis)
			light.position[axis] = lightning_detail::checked(std::int64_t(light.position[axis]) + position[axis]);
		for (unsigned channel = 0; channel < 4; ++channel) {
			light.color[channel] += color[channel];
			if (!std::isfinite(light.color[channel])) throw std::runtime_error("Lightning light color is not finite");
		}
	}
	std::map<int, LightningSegmentLight> finish() const
	{
		auto result = lights;
		for (auto &[segment, light] : result) {
			for (unsigned axis = 0; axis < 3; ++axis) {
				light.position[axis] /= light.samples;
				light.color[axis] /= light.samples;
			}
			// Source averages RGB but retains summed alpha when computing brightness
			const auto brightness = std::sqrt(10 * (light.color[0] + light.color[1] + light.color[2]) * light.color[3]) * 65536;
			if (!std::isfinite(brightness) || brightness < 0 || double(brightness) > INT32_MAX)
				throw std::runtime_error("Lightning brightness exceeds the defined source range");
			light.brightness = int(brightness);
		}
		return result;
	}
};

template <class Locate, class Add>
unsigned lightning_bolt_lights(const LightningBolt &bolt, Locate locate, Add add)
{
	if (!bolt.definition.light || bolt.nodes.empty() || bolt.lifetime.nodes <= 0) return 0;
	const int nodes = bolt.lifetime.nodes;
	if (std::size_t(nodes) > bolt.nodes.size() || bolt.lifetime.length < 0)
		throw std::runtime_error("Lightning light samples exceed their node storage");
	int stride = int(double(bolt.lifetime.length) / (20 * 65536) + 0.5);
	if (!stride) stride = 1;
	double step = double(nodes - 1) / stride;
	if (step <= 0) step = nodes;
	unsigned count = 0;
	for (double h = step / 2; h < nodes; h += step) {
		const auto &position = bolt.nodes[std::size_t(h)].position;
		const int segment = locate(position, bolt.segment);
		if (segment < 0) break;
		add(segment, position, bolt.definition.color);
		++count;
	}
	return count;
}
} // namespace xl_compat
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#endif
