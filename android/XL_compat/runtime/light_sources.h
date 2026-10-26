#ifndef DXX_XL_RUNTIME_LIGHT_SOURCES_H
#define DXX_XL_RUNTIME_LIGHT_SOURCES_H
#ifdef _MSC_VER
#pragma pack(push, 8)
#endif
#include "geometry.h"
#include "visibility.h"
#include "../companion_lighting.h"
#include <vector>
namespace xl_compat
{
struct ClassicGeometryLight {
	int segment, side;
	ClassicLightingSide geometry;
	ClassicFaceLight material;
	LightPoint emission_direction;
	float radius = 0;
	bool destructible = false, flickering = false, triggered = false, volatile_wall = false;
	bool variable() const
	{
		return destructible || flickering || triggered || volatile_wall;
	}
};
// Single-player D1 or admitted XL full-color method-0 sources, static first then segment/side
// Classification does not implement enabled-state updates, visibility or shading
std::vector<ClassicGeometryLight> compile_classic_geometry_lights(const TextureBrightnessTable &brightness, const TextureColorTable &colors);
std::vector<ClassicGeometryLight> compile_classic_geometry_lights(const TextureBrightnessTable &brightness, const TextureRgbTable &colors);
// D1 or admitted XL method-0 mesh faces, no colored volumes or generated mesh vertices
std::vector<int> classic_lighting_face_vertices(int segment);
// Assemble source-view vertex flags using explicit optics and loaded geometry
// This table precedes and is independent of later light range/ray flag updates
std::vector<std::vector<bool>> compile_classic_vertex_visibility(const std::vector<ClassicGeometryLight> &sources,
                                                                 const ClassicVisibilityCamera &optics);
// Caller supplies the accumulated source-segment/vertex table from precomputation
// Returns up to 32 entries in pinned XL distance/tie order, including variable lights
std::vector<GeometryLightDistance> classic_nearest_vertex_lights(int vertex,
                                                                 const std::vector<ClassicGeometryLight> &sources, const std::vector<std::vector<bool>> &visibility);
struct ClassicAmbientProfile {
	float range_scale = 0.5f, linear_attenuation = 0.05f, quadratic_attenuation = 0.005f;
	float cutoff = float(222 * 65536), brightness = 1;
	Rgb material_ambient{ 0.3f, 0.3f, 0.3f }, material_diffuse{ 0.7f, 0.7f, 0.7f };
	bool renormalize_normals = true;
};
// Owns a snapshot of one loaded level; no engine pointers survive construction
// Caller must release on level replacement and map actual source lifecycle state
class ClassicAmbientLighting
{
	struct Source {
		ClassicGeometryLight light;
		std::array<int, 4> vertices;
		std::int32_t segment_radius = 0, stored_distance = 0;
		bool enabled = true;
	};
	std::vector<Source> sources_;
	std::vector<FixedLightPoint> positions_;
	std::vector<ClassicVertexNormal> normals_;
	std::vector<std::vector<GeometryLightDistance>> nearest_;

  public:
	ClassicAmbientLighting(const std::vector<ClassicGeometryLight> &sources, const std::vector<std::vector<bool>> &visibility);
	ClassicAmbientLighting(const std::vector<ClassicGeometryLight> &sources, const ClassicVisibilityCamera &optics);
	void set_source_state(std::size_t index, bool enabled, std::int32_t stored_distance);
	std::vector<Rgb> gather(const ClassicAmbientProfile &profile) const;
	void clear();
};
} // namespace xl_compat
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#endif
