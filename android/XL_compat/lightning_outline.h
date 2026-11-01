#ifndef DXX_XL_LIGHTNING_OUTLINE_H
#define DXX_XL_LIGHTNING_OUTLINE_H
#include "lightning_render.h"

namespace xl_compat
{
inline int lightning_outline_scale(float distance)
{
	if (!std::isfinite(distance)) throw std::runtime_error("XL lightning outline distance is not finite");
	if (distance <= 0) return 0;
	const float scale = std::log10(distance / (std::max) (1.f, std::log(distance)));
	return std::clamp(int(scale * scale + .5f), 0, 31);
}

// Source gameplay outline defaults, with portable smooth widths spanning 1..10
inline float lightning_outline_width(const LightningCore &core, int random_nodes, const LightningVertex &eye, float viewer_radius, int canvas_width)
{
	if (core.vertices.size() < 6 || random_nodes < 0 || std::size_t(random_nodes) + 6 > core.vertices.size() ||
	    !std::isfinite(viewer_radius) || viewer_radius < 0 || canvas_width <= 0)
		throw std::runtime_error("XL lightning outline has invalid viewer or core geometry");
	const auto distance = [&](const LightningVertex &point) {
		const auto delta = lightning_render_detail::subtract(point, eye);
		return std::sqrt(delta[0] * delta[0] + delta[1] * delta[1] + delta[2] * delta[2]);
	};
	const auto endpoint = core.vertices.size() - 5 - random_nodes;
	const float nearest = (std::min) (distance(core.vertices.front()), distance(core.vertices[endpoint])) - viewer_radius;
	if (!std::isfinite(nearest)) throw std::runtime_error("XL lightning outline endpoints are not finite");
	const int scale = lightning_outline_scale((std::max) (0.f, nearest));
	float width = (std::max) (1.f, float(canvas_width) / 640.f) * 3.f;
	if (scale) width /= float(scale * 2);
	return std::clamp(width, 1.f, 10.f);
}
} // namespace xl_compat
#endif
