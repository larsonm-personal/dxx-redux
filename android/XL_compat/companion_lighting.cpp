#include "companion_lighting.h"
#include <stdexcept>

namespace xl_compat
{
namespace
{
std::int32_t signed_le32(const std::uint8_t *data)
{
	const auto value = std::uint32_t(data[0]) | (std::uint32_t(data[1]) << 8) |
	                   (std::uint32_t(data[2]) << 16) | (std::uint32_t(data[3]) << 24);
	// Avoid implementation-defined unsigned-to-signed conversion
	return value <= 0x7fffffffu ? static_cast<std::int32_t>(value) : -1 - static_cast<std::int32_t>(0xffffffffu - value);
}
} // namespace

TextureBrightnessTable decode_texture_brightness(const std::uint8_t *data, std::size_t size)
{
	if (!data || size != companion_texture_count * 4)
		throw std::runtime_error("LGT requires exactly 910 signed 32-bit brightness entries (3640 bytes)");
	TextureBrightnessTable result{};
	for (std::size_t i = 0; i < result.size(); ++i) result[i] = signed_le32(data + i * 4);
	return result;
}

TextureColorTable decode_texture_colors(const std::uint8_t *data, std::size_t size)
{
	if (!data || size != companion_texture_count * 13)
		throw std::runtime_error("CLR requires exactly 910 indexed RGB entries (11830 bytes)");
	TextureColorTable result{};
	for (std::size_t i = 0; i < result.size(); ++i) {
		result[i].serialized_index = data[i * 13];
		for (std::size_t channel = 0; channel < 3; ++channel)
			result[i].channels[channel] = signed_le32(data + i * 13 + 1 + channel * 4);
	}
	return result;
}

ClassicFaceLightMaterials::ClassicFaceLightMaterials(const TextureBrightnessTable &brightness, const TextureColorTable &colors) : brightness_(brightness)
{
	for (std::size_t i = 0; i < colors.size(); ++i)
		for (std::size_t c = 0; c < 3; ++c)
			colors_[i][c] = static_cast<float>(colors[i].channels[c]) / static_cast<float>(0x7fffffff);
}

ClassicFaceLightMaterials::ClassicFaceLightMaterials(const TextureBrightnessTable &brightness, const TextureRgbTable &colors)
    : brightness_(brightness), colors_(colors)
{
}

std::optional<ClassicFaceLight> ClassicFaceLightMaterials::face(int base, int overlay, bool outdoors)
{
	const auto valid = [](int texture) { return texture >= 0 && texture < static_cast<int>(companion_texture_count); };
	// XL skips the entire face if its base texture is invalid
	if (!valid(base)) return std::nullopt;
	std::optional<ClassicFaceLight> result;
	const auto add = [&](int texture, bool ambient) {
		if (brightness_[texture] <= 0) return;
		auto &color = colors_[texture];
		if (!result) {
			// Add mutates the shared texture color, but Update returns before this fallback
			if (color[0] == 0 && color[1] == 0 && color[2] == 0) color = { 1, 1, 1 };
			result = ClassicFaceLight{};
			result->created_texture = texture;
			result->ambient = ambient;
		}
		result->material_texture = texture;
		result->brightness = static_cast<float>(brightness_[texture]) / 65536.0f;
		result->rgb = color;
	};
	add(base, outdoors || (valid(overlay) && brightness_[overlay] != 0));
	if (valid(overlay) && overlay > 0) add(overlay, outdoors);
	return result;
}
} // namespace xl_compat
