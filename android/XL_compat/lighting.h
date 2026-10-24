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
struct Inventory;

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
