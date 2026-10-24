#ifndef DXX_XL_COMPAT_LIGHTING_H
#define DXX_XL_COMPAT_LIGHTING_H

#include <array>
#include <cstdint>
#include <vector>

#ifdef _MSC_VER
#pragma pack(push, 8)
#endif

namespace xl_compat
{
using Rgb = std::array<float, 3>;
// Pinned XL RandFloat(4) divides by four; sample is from a 15-bit FX stream
std::int32_t flare_intensity(std::int32_t light, std::int32_t life, unsigned sample);
struct Inventory;

// XL keeps scalar intensity and source-order color separate; RGB sums lose this
struct DynamicVertexLight {
	std::int32_t intensity = 0;
	Rgb color{};
	bool has_color = false;
	// Contribution is already attenuated by ApplyLight; null means no source color
	void add(std::int32_t contribution, const Rgb *source_color, bool force_color);
};

// XL CSide::Read takes the maximum decoded UVL light for each shared vertex
std::vector<float> vertex_brightness(const Inventory &level);

// Inputs to the XL vertex-color path, not a replacement for light generation
struct VertexLighting {
	Rgb face{}, ambient{}, dynamic_color{};
	float face_alpha = 1, vertex_brightness = 0, brightness = 1;
	std::int32_t static_light = 0, dynamic_light = 0, flash_scale = 65536;
	int face_index = 1, blend = 0;
	bool ambient_enabled = true, darkness = false, flashing = false;
	bool extra_game_info = false, has_dynamic_color = false;
	bool mix_colors = true, colored = true, cap_colors = false;
	bool scale_light = false, headlight = false, outside_endlevel = false;
	bool no_bitmap_lighting = false;
};

struct ShadedVertex {
	Rgb rgb;
	std::int32_t scalar_light;
};

// Source basis: XL renderlib SetVertexColor/SetVertexLight/AdjustVertexColor
// Does not implement lightmaps, light-source generation, face setup or GL output
ShadedVertex shade_vertex(const VertexLighting &input);
} // namespace xl_compat
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#endif
