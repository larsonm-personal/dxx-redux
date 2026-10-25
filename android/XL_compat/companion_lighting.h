#ifndef DXX_XL_COMPANION_LIGHTING_H
#define DXX_XL_COMPANION_LIGHTING_H
#ifdef _MSC_VER
#pragma pack(push, 8)
#endif
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace xl_compat
{
// Pinned XL MAX_WALL_TEXTURES; these tables have no count or version header
constexpr std::size_t companion_texture_count = 910;
using TextureBrightnessTable = std::array<std::int32_t, companion_texture_count>;
struct CompanionTextureColor {
	// Retained for inspection; XL LoadTextureColors resets the runtime index to zero
	std::uint8_t serialized_index;
	std::array<std::int32_t, 3> channels;
};
using TextureColorTable = std::array<CompanionTextureColor, companion_texture_count>;
// Exact pinned-layout decoding only, not renderer or gameplay qualification
TextureBrightnessTable decode_texture_brightness(const std::uint8_t *data, std::size_t size);
TextureColorTable decode_texture_colors(const std::uint8_t *data, std::size_t size);

struct ClassicFaceLight {
	int created_texture = -1;
	int material_texture = -1;
	float brightness = 0;
	std::array<float, 3> rgb{};
	bool ambient = false;
};

// Classic D1, lighting method 0, full level colors, no nostalgia override
// Call once per unique compiled face in reference order; construct anew per level
// Builds materials only: no visibility, nearest-light selection or vertex shading
class ClassicFaceLightMaterials
{
	TextureBrightnessTable brightness_;
	std::array<std::array<float, 3>, companion_texture_count> colors_{};

  public:
	ClassicFaceLightMaterials(const TextureBrightnessTable &brightness, const TextureColorTable &colors);
	std::optional<ClassicFaceLight> face(int base, int overlay, bool outdoors);
};
} // namespace xl_compat
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#endif
