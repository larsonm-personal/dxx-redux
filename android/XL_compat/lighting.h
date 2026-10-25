#ifndef DXX_XL_COMPAT_LIGHTING_H
#define DXX_XL_COMPAT_LIGHTING_H

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#ifdef _MSC_VER
#pragma pack(push, 8)
#endif

namespace xl_compat
{
using Rgb = std::array<float, 3>;
using LightPoint = std::array<float, 3>;
using FixedLightPoint = std::array<std::int32_t, 3>;
// Returning XL Cross overload, for edges already bounded by CFixVector::Check
FixedLightPoint geometry_fixed_cross(const FixedLightPoint &a, const FixedLightPoint &b);
// Geometry light direction is normalized from the fixed side-normal average
LightPoint geometry_emission_direction(const std::array<FixedLightPoint, 2> &normals);
// XL ComputeCenter projection, given the compiler's fixed triangle normals
FixedLightPoint geometry_side_center(const std::array<LightPoint, 4> &corners, const std::array<FixedLightPoint, 2> &normals);
// FixNormals consumes independently normalized float/fixed directions toward segment center
// Result slots 0/1 are corrected normals; slot 2 is the reference's first-pass average
std::array<LightPoint, 3> geometry_orient_normals(const std::array<LightPoint, 2> &normals,
                                                  const std::array<FixedLightPoint, 2> &fixed_normals, const LightPoint &float_direction, const FixedLightPoint &fixed_direction);
// XL scalar CFloatVector::Normal, before side-center orientation correction
LightPoint geometry_triangle_normal(const LightPoint &a, const LightPoint &b, const LightPoint &c);
// XL portal normals use absolute vertex ordering to agree across adjacent segments
LightPoint geometry_portal_normal(const std::array<LightPoint, 3> &points, const std::array<std::uint16_t, 3> &vertices);
struct GeometryLightFace {
	std::array<LightPoint, 4> corners{};
	std::array<std::array<unsigned, 3>, 2> triangles{};
	std::array<LightPoint, 2> normals{};
	unsigned corner_count = 4, triangle_count = 2;
};
// XL DistToFace's hit-point path: first containing projection, otherwise perimeter
// Supplied triangle order and normals must match the compiled engine side
LightPoint geometry_light_point(const GeometryLightFace &face, const LightPoint &receiver);
struct GeometryLightDistance {
	int index;
	std::int32_t distance;
};
// Candidates must already pass reference visibility/range rules, in light-table order
// Preserve XL's equal-distance ordering before applying MAX_NEAREST_LIGHTS=32
std::vector<GeometryLightDistance> nearest_geometry_lights(std::vector<GeometryLightDistance> candidates);
struct GeometryLightSample {
	float distance, normal_dot, emission_angle;
	float range_scale, linear_attenuation, quadratic_attenuation, cutoff;
	float brightness;
};
struct GeometryAttenuation {
	float distance, normal_dot, attenuation;
};
// Pinned ATTENUATION_TYPE=0 arithmetic after geometry/visibility selection
// Cutoff is explicit: do not silently reinterpret the reference fixed-point constant
std::optional<GeometryAttenuation> geometry_attenuation(const GeometryLightSample &input);
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
