#ifndef DXX_XL_COMPAT_LIGHTING_H
#define DXX_XL_COMPAT_LIGHTING_H

#include <array>
#include <cstdint>
#include <optional>
#include <vector>
#include <functional>

#ifdef _MSC_VER
#pragma pack(push, 8)
#endif

namespace xl_compat
{
using Rgb = std::array<float, 3>;
using LightPoint = std::array<float, 3>;
using FixedLightPoint = std::array<std::int32_t, 3>;
// Pinned default math format 2; side-normal setup uses the separate legacy path
std::int32_t geometry_default_magnitude(const FixedLightPoint &vector);
FixedLightPoint geometry_default_direction(const FixedLightPoint &vector);
struct GeometryRouteEdge {
	int child = -1;
	std::uint16_t cost = 0;
};
struct GeometryRouteNode {
	std::int32_t cost = -1;
	int predecessor = -1, side = -1;
};
// Positive uint16 edges, six ordered sides, Dial buckets with LIFO equal costs
std::vector<GeometryRouteNode> geometry_weighted_routes(const std::vector<std::array<GeometryRouteEdge, 6>> &graph, int start);
std::vector<int> geometry_route_path(const std::vector<GeometryRouteNode> &routes, int destination);
struct GeometryRouteInterior {
	std::int32_t distance = -1;
	int first = -1, last = -1;
};
// All-destinations BuildPath: excludes endpoint legs, uses explicit SegVis input
GeometryRouteInterior geometry_shorten_route(const std::vector<int> &path,
                                             const std::function<bool(int, int)> &visible, const std::function<std::int32_t(int, int)> &distance);
// Pinned segment-distance row encoding; 0xffff is the unreachable sentinel
float geometry_route_distance_scale(std::int32_t maximum);
std::uint16_t geometry_pack_route_distance(std::int32_t distance, float scale);
std::int32_t geometry_unpack_route_distance(std::uint16_t distance, float scale);
// Non-stereo ScaleTransformation path with bOglScale=-1
std::array<FixedLightPoint, 3> geometry_scaled_camera(const std::array<FixedLightPoint, 3> &basis,
                                                      const FixedLightPoint &aspect, std::int32_t zoom);
// Float conversion precedes world-position subtraction in the reference
LightPoint geometry_camera_view(const FixedLightPoint &world, const FixedLightPoint &position,
                                const std::array<FixedLightPoint, 3> &scaled_basis);
struct GeometryScreenPoint {
	std::int32_t x, y;
	bool behind;
};
// ProjectAndEncode after view/projection matrix multiplication, with reset flags
// xy contains the projection matrix result; depth is the original view-space z
// Zero depth/overflow use the measured Windows integer-indefinite coordinate
// Nonfinite inputs remain invalid; no undefined float-to-integer cast is used
GeometryScreenPoint geometry_screen_point(const std::array<float, 2> &xy, float depth, float half_width, float half_height);
struct GeometryPortal {
	std::int32_t left, right, top, bottom;
};
// Classic four-corner BuildSegList portal clipping with nShadowMap=0, as set
// by light precomputation; mixed-depth portals inherit the current rectangle
std::optional<GeometryPortal> geometry_light_portal(const std::array<GeometryScreenPoint, 4> &points,
                                                    const GeometryPortal &current, int width, int height);
struct GeometryPortalSide {
	int child = -1; // Caller omits nontransparent sides by leaving child negative
	std::array<GeometryScreenPoint, 4> points{};
};
using GeometryPortalSegment = std::array<GeometryPortalSide, 6>;
struct GeometryPortalVisit {
	int segment;
	GeometryPortal portal;
};
// Prepared single-view traversal; caller supplies reference-projected corners
// pass_limit corresponds to min(4 * (detail depth + 1), segment count)
// Returned candidates still require the reference SegmentIsVisible test
std::vector<GeometryPortalVisit> geometry_portal_traversal(const std::vector<GeometryPortalSegment> &segments,
                                                           int start, int width, int height, unsigned pass_limit);
// Lazy variant requests only processed segments, including necessary revisits
std::vector<GeometryPortalVisit> geometry_portal_traversal(std::size_t segment_count,
                                                           const std::function<GeometryPortalSegment(int)> &segment, int start, int width, int height, unsigned pass_limit);
// Returning XL Cross overload, for edges already bounded by CFixVector::Check
FixedLightPoint geometry_fixed_cross(const FixedLightPoint &a, const FixedLightPoint &b);
// Geometry light direction is normalized from the fixed side-normal average
LightPoint geometry_emission_direction(const std::array<FixedLightPoint, 2> &normals);
// Nearest-vertex prefilter for geometry sources with the reference range scale 1
// Visibility must come from the segment/vertex precomputation, not distance alone
std::optional<std::int32_t> geometry_vertex_candidate(std::int32_t center_distance, float radius, bool own_vertex, bool visible);
// Classic XL bitmap flags: transparent=1, super-transparent=2
bool classic_light_texture_transparent(unsigned base_flags, unsigned overlay_flags, bool has_overlay);
// XL ComputeCenter projection, given the compiler's fixed triangle normals
FixedLightPoint geometry_side_center(const std::array<LightPoint, 4> &corners, const std::array<FixedLightPoint, 2> &normals, const FixedLightPoint &plane_point);
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
// Default-range Contribute for ambient gathering (destination segment -1)
std::optional<std::int32_t> geometry_ambient_distance(std::int32_t center_distance, float source_radius, std::int32_t segment_radius);
// SetActive distance buckets, forward probing, duplicate suppression, 4096 slots
std::vector<GeometryLightDistance> geometry_active_lights(const std::vector<GeometryLightDistance> &candidates);
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
struct GeometryColorInput {
	GeometryLightFace face;
	LightPoint center{}, emission_direction{}, receiver{}, normal{};
	Rgb light_color{}, material_ambient{}, material_diffuse{};
	GeometryLightSample attenuation{};
	bool self = false;
};
// Ambient gather geometry-source path: diffuse=true, no spot/specular/lightmaps
// Caller already selected source state and, for self sources, own-face membership
std::optional<Rgb> geometry_source_color(const GeometryColorInput &input);
struct GeometryColorSum {
	Rgb ordinary{}, self{};
	unsigned self_count = 0, contributing = 0;
	// Call for each eligible nonblack source, even if attenuation rejected its sample
	void add(const std::optional<Rgb> &color, bool self_source);
	Rgb finish() const;
};
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

// Non-volumetric colored-segment profile, after normalizing legacy functions
// Texture classification follows CSegment::HasTexture/IsWater/IsLava
struct WaterFace {
	bool water = false, child_water = false, has_child = false;
	bool has_wall = false, fluid_texture = false;
	bool colored_segments = true, volumetric_fog = false;
	int function = 0, child_function = 0;
};
bool water_face_colored(const WaterFace &face);
// ComputeStaticFaceLight blends the face into ambient before lighting, then
// multiplies the resulting RGB by the material color; alpha is handled by GL
ShadedVertex shade_water_vertex(VertexLighting input, bool textured);
} // namespace xl_compat
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#endif
