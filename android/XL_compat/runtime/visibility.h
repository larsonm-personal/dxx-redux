#ifndef DXX_XL_RUNTIME_VISIBILITY_H
#define DXX_XL_RUNTIME_VISIBILITY_H
#ifdef _MSC_VER
#pragma pack(push, 8)
#endif
#include "../lighting.h"
namespace xl_compat
{
// Classic D1 portal eligibility for BuildSegList(..., bIgnoreDoors=true)
// Does not establish projected segment or vertex visibility
bool classic_light_portal_passable(int segment, int side);
// Router topology uses IsPassable(..., false) & (transparent | passable)
// Reachability alone does not reproduce compressed SegDist values or overflow
std::vector<int> classic_light_route_reachable(int start);
struct ClassicRouteEdge {
	int child = -1;
	std::int32_t distance = -1;
	std::uint16_t cost = 65535;
	bool passable = false;
};
struct ClassicRouteGraph {
	std::int32_t scale;
	std::vector<FixedLightPoint> centers;
	std::vector<std::array<ClassicRouteEdge, 6>> edges;
};
// Loaded geometry and initial wall state; integer-root parity remains required
ClassicRouteGraph classic_light_route_graph();
struct ClassicDistanceEntry {
	std::uint16_t distance = 65535;
	int first = -1, last = -1;
};
struct ClassicDistanceRow {
	float scale = 1;
	std::vector<ClassicDistanceEntry> entries;
};
ClassicDistanceRow classic_distance_row(const ClassicRouteGraph &graph, const std::vector<std::vector<bool>> &visibility, int start);
std::int32_t classic_segment_distance(const ClassicDistanceRow &row, const std::vector<FixedLightPoint> &centers,
                                      int destination, const FixedLightPoint &start_position, const FixedLightPoint &end_position);
struct ClassicLightView {
	FixedLightPoint position, forward;
};
// Four corner views followed by the offset-center view; degenerate normal skips
std::optional<std::array<ClassicLightView, 5>> classic_light_views(int segment, int side);
struct ClassicVisibilityCamera {
	FixedLightPoint position{}, forward{}, aspect{ 65536, 65536, 65536 };
	std::int32_t zoom = 65536;
	// Diagonal XY terms of the non-stereo perspective matrix, after GL readback
	std::array<float, 2> projection_xy{};
	float half_width = 0, half_height = 0;
	int screen_width = 0, screen_height = 0;
	unsigned detail_depth = 0;
};
// Pinned non-stereo default zoom and square lighting projection; parity remains experimental
ClassicVisibilityCamera classic_default_lighting_camera();
// Explicit camera configuration: no inferred reference viewport or FOV defaults
std::vector<int> classic_camera_visible_segments(int start, const ClassicVisibilityCamera &camera);
// Six outward center views, symmetric OR union as in the reference SegVis table
// This is not the source-view VERTVIS table used by nearest-vertex lights
std::vector<std::vector<bool>> classic_segment_visibility(const ClassicVisibilityCamera &optics);
struct ClassicLightViewCandidates {
	std::array<std::vector<int>, 5> views;
	std::vector<int> segments;
};
// Geometric candidates only: no SegDist qualification, CheckLightVisibility,
// or visible-mesh-face vertex marking; optics must be supplied explicitly
ClassicLightViewCandidates classic_light_view_candidates(int segment, int side, const ClassicVisibilityCamera &optics);
// One prepared camera view, indexed by the loaded mine's absolute vertex IDs
// Does not create the camera, union views, or populate source vertex visibility
std::vector<int> classic_visible_segments(int start, const std::vector<GeometryScreenPoint> &projected,
                                          int width, int height, unsigned detail_depth);
} // namespace xl_compat
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#endif
