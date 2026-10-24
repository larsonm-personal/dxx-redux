#include "lighting.h"
#include "inventory.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace xl_compat
{
namespace
{
void scale(Rgb &color, float factor)
{
	for (auto &channel : color) channel *= factor;
}
void cap_to_light(Rgb &color, float light)
{
	const auto maximum = *std::max_element(color.begin(), color.end());
	if (maximum > light) scale(color, light / maximum);
}
std::int32_t fixed(std::int64_t value)
{
	if (value < std::numeric_limits<std::int32_t>::min() || value > std::numeric_limits<std::int32_t>::max())
		throw std::runtime_error("XL vertex light overflow");
	return static_cast<std::int32_t>(value);
}
} // namespace

std::int32_t flare_intensity(std::int32_t light, std::int32_t life, unsigned sample)
{
	if (sample > 32767) throw std::runtime_error("Invalid XL flare random sample");
	const float intensity = (1.75f + float(sample) / (32767.0f * 4.0f)) * std::min(light, life);
	if (!std::isfinite(intensity) || double(intensity) < std::numeric_limits<std::int32_t>::min() || double(intensity) > std::numeric_limits<std::int32_t>::max())
		throw std::runtime_error("XL flare intensity overflow");
	return static_cast<std::int32_t>(intensity);
}

void DynamicVertexLight::add(std::int32_t contribution, const Rgb *source_color, bool force_color)
{
	const auto next_intensity = fixed(std::int64_t(intensity) + contribution);
	if (source_color) {
		for (const float channel : *source_color)
			if (!std::isfinite(channel)) throw std::runtime_error("Invalid XL dynamic source color");
		const bool white = (*source_color)[0] == 1 && (*source_color)[1] == 1 && (*source_color)[2] == 1;
		if (force_color || !white) {
			if (has_color) {
				for (unsigned i = 0; i < 3; ++i) color[i] = (color[i] + (*source_color)[i]) / 2;
			} else color = *source_color;
			has_color = true;
		}
	}
	intensity = next_intensity;
}

std::vector<float> vertex_brightness(const Inventory &level)
{
	if (level.level_version != 15 || level.game_version != 33)
		throw std::runtime_error("XL brightness requires audited level 15/game 33 geometry");
	// Pinned XL segment.cpp sideVertIndex; only the audited four-corner layout
	constexpr unsigned corners[6][4] = {
		{ 7, 6, 2, 3 }, { 0, 4, 7, 3 }, { 0, 1, 5, 4 }, { 2, 6, 5, 1 }, { 4, 5, 6, 7 }, { 3, 2, 1, 0 }
	};
	std::vector<float> brightness(level.vertices.size(), 0);
	for (const auto &segment : level.segments) {
		for (unsigned side = 0; side < 6; ++side) {
			// Open portals have no serialized UVLs and do not contribute
			if (segment.children[side] != -1 && segment.sides[side].wall == -1) continue;
			for (unsigned corner = 0; corner < 4; ++corner) {
				const int vertex = segment.vertices[corners[side][corner]];
				const int light = segment.sides[side].uvl[corner * 3 + 2];
				if (vertex < 0 || std::size_t(vertex) >= brightness.size() || light < 0 || light > 65535)
					throw std::runtime_error("Invalid XL brightness geometry");
				// The disk light is an unsigned 16-bit half-fixed value
				brightness[vertex] = std::max(brightness[vertex], float(light * 2) / 65536);
			}
		}
	}
	return brightness;
}

ShadedVertex shade_vertex(const VertexLighting &in)
{
	if (in.blend < 0 || in.blend > 2) throw std::runtime_error("Unknown XL face blend");
	auto color = in.face;
	if (in.ambient_enabled) {
		for (unsigned i = 0; i < 3; ++i) {
			if (in.blend == 1) color[i] *= in.ambient[i];
			else if (in.blend == 2) color[i] = color[i] * in.face_alpha + in.ambient[i] * (1 - in.face_alpha);
			else color[i] += in.ambient[i];
		}
	}
	auto light = in.darkness ? 0 : in.static_light;
	if (!in.darkness && in.flashing) {
		const auto product = std::int64_t(light) * in.flash_scale;
		light = fixed(product >= 0 ? product / 65536 : -((-product + 65535) / 65536));
	}
	const float fl = float(light) / 65536, dl = float(in.dynamic_light) / 65536;
	light = fixed(std::int64_t(light) + in.dynamic_light);
	if (in.extra_game_info && in.has_dynamic_color) {
		if (in.mix_colors) {
			if (in.colored) {
				if (in.ambient_enabled && fl != 0 && in.vertex_brightness != 0) {
					scale(color, fl / in.vertex_brightness);
					for (unsigned i = 0; i < 3; ++i) color[i] += in.dynamic_color[i] * dl;
					cap_to_light(color, fl + dl);
				} else if (in.ambient_enabled) {
					color = in.dynamic_color;
					scale(color, dl);
					cap_to_light(color, dl);
				} else {
					for (unsigned i = 0; i < 3; ++i) color[i] = fl + in.dynamic_color[i] * dl;
					cap_to_light(color, fl + dl);
				}
			} else color.fill(fl + dl);
			if (in.cap_colors)
				for (auto &channel : color) channel = std::min(channel, 1.0f);
		} else {
			color = in.dynamic_color;
			scale(color, float(std::pow(float(light) / 65536, 1.0f / 3.0f)));
		}
	} else cap_to_light(color, fl + dl);
	scale(color, in.brightness);
	const float bright_light = float(light) * in.brightness;
	if (!std::isfinite(bright_light) || double(bright_light) < std::numeric_limits<std::int32_t>::min() || double(bright_light) > std::numeric_limits<std::int32_t>::max())
		throw std::runtime_error("Invalid XL vertex brightness");
	light = std::min(static_cast<std::int32_t>(bright_light), std::int32_t(65536));
	const float factor = in.scale_light ? (in.headlight ? 0.4f : 0.3f) : 1.0f;
	if (!in.face_index || !in.ambient_enabled || in.outside_endlevel)
		color.fill((in.no_bitmap_lighting ? 1.0f : float(light) / 65536) * factor);
	else if (factor != 1) scale(color, factor);
	for (const auto channel : color)
		if (!std::isfinite(channel)) throw std::runtime_error("Invalid XL vertex color");
	return { color, light };
}
} // namespace xl_compat
