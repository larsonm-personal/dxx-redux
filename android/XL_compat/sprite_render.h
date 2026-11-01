#ifndef DXX_XL_SPRITE_RENDER_H
#define DXX_XL_SPRITE_RENDER_H
#include <stdexcept>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace xl_compat
{
// CBitmap::AvgColorIndex and ObjectBlobColor, using decoded indexed rows
inline std::array<float, 3> sprite_bitmap_color(unsigned average, const unsigned char *pixels, unsigned width, unsigned height, unsigned stride, const unsigned char *palette)
{
	if (average > 255 || !palette || !pixels || !width || !height || stride < width) throw std::runtime_error("Invalid XL sprite bitmap color inputs");
	if (!average) {
		std::array<std::uint64_t, 3> sums{};
		std::uint64_t count = 0;
		for (unsigned y = 0; y < height; ++y)
			for (unsigned x = 0; x < width; ++x) {
				const unsigned index = pixels[std::size_t(y) * stride + x];
				if (!index || index == 254 || index == 255) continue;
				for (unsigned axis = 0; axis < 3; ++axis) sums[axis] += palette[index * 3 + axis];
				++count;
			}
		if (count) {
			for (auto &sum : sums) sum /= count;
			int best = 3 * 255 * 255 + 1;
			for (unsigned index = 0; index < 255; ++index) {
				int distance = 0;
				for (unsigned axis = 0; axis < 3; ++axis) {
					const int delta = int(sums[axis]) - palette[index * 3 + axis];
					distance += delta * delta;
				}
				if (distance < best) {
					best = distance;
					average = index;
				}
			}
		}
	}
	std::array<float, 3> color{};
	for (unsigned axis = 0; axis < 3; ++axis) color[axis] = float(palette[average * 3 + axis]) / 255.f;
	if (color[0] + color[1] + color[2] == 0) color = { 1, 1, 1 };
	return color;
}
inline bool energy_powerup(int id)
{
	return id == 0 || id == 1 || id == 2 || id == 7 || id == 23 || id == 25;
}
struct PowerupCorona {
	std::array<float, 4> color;
	std::int32_t size;
};
// RenderPowerupCorona consumes ObjectBlobColor (black is already replaced by white)
inline PowerupCorona powerup_corona(int id, std::int32_t size, std::array<float, 3> rgb, float intensity = .5f)
{
	PowerupCorona result{};
	if (id >= 4 && id <= 6) {
		constexpr float keys[3][3] = { { .2f, .2f, .9f }, { .9f, .2f, .2f }, { .9f, .8f, .2f } };
		std::copy_n(keys[id - 4], 3, result.color.begin());
		result.size = 12 * 65536;
	} else {
		const float brightness = id >= 2 ? std::max(rgb[0], std::max(rgb[1], rgb[2])) * .5f : float(std::sqrt(double((rgb[0] + rgb[1] + rgb[2]) / 3.f)));
		if (!(brightness > 0) || size < 0 || size > INT32_MAX / 2) throw std::runtime_error("Invalid XL powerup corona inputs");
		for (unsigned axis = 0; axis < 3; ++axis) result.color[axis] = rgb[axis] / brightness;
		result.size = 2 * size;
	}
	for (unsigned axis = 0; axis < 3; ++axis) result.color[axis] *= intensity * .5f;
	result.color[3] = intensity;
	return result;
}
enum class SpriteBatchAction { keep,
	                           end,
	                           begin };
struct SpriteBatchPlan {
	SpriteBatchAction action = SpriteBatchAction::keep;
	int blend = 0, strength = 0;
	float brightness = 1;
};
// CTranspSprite::Render treats negative additive modes as a bypass of Begin/End
// Done returns rendering to the scene while retaining any existing glow batch
inline SpriteBatchPlan sprite_batch_plan(int additive, bool glow_available)
{
	if (additive < -2 || additive > 2) throw std::runtime_error("Unsupported XL sprite blend mode");
	SpriteBatchPlan plan;
	plan.blend = additive < 0 ? -additive : additive;
	if (additive < 0 || !glow_available) return plan;
	if (!additive) plan.action = SpriteBatchAction::end;
	else {
		plan.action = SpriteBatchAction::begin;
		plan.strength = additive == 1 ? 2 : 1;
		plan.brightness = additive == 1 ? .85f : .95f;
	}
	return plan;
}
} // namespace xl_compat
#endif
