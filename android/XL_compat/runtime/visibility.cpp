#include "visibility.h"
#include "preview.h"
#include "../lighting.h"
#include "geometry.h"
#include <stdexcept>
#include <algorithm>
#include <cmath>
#include <limits>
extern "C" {
#include "gameseg.h"
#include "mission.h"
#include "wall.h"
#include "textures.h"
}

namespace xl_compat
{
bool geometry_lighting_available()
{
	// Shared geometry also serves audio in admitted legacy XL fluid levels
	// Availability here does not enable a rendering profile
	return Current_mission && (Current_mission->descent_version == 1 || xl_level_extensions_active());
}

ClassicVisibilityCamera classic_default_lighting_camera()
{
	ClassicVisibilityCamera camera;
	// XL DEFAULT_ZOOM affects both gluPerspective FOV and ScaleTransformation
	camera.zoom = 0x9000;
	const auto perspective = static_cast<float>(1.0 / std::tan(105.0 * (double(camera.zoom) / 65536.0) * std::acos(-1.0) / 360.0));
	camera.projection_xy = { perspective, perspective };
	camera.half_width = camera.half_height = 512;
	camera.screen_width = camera.screen_height = 1024;
	camera.detail_depth = 10;
	return camera;
}

std::optional<std::array<ClassicLightView, 5>> classic_light_views(int segment, int side)
{
	if (!geometry_lighting_available())
		throw std::runtime_error("Classic light views require D1 or admitted XL geometry");
	const auto geometry = compile_classic_lighting_side(segment, side);
	FixedLightPoint normal;
	for (unsigned axis = 0; axis < 3; ++axis)
		normal[axis] = static_cast<std::int32_t>((std::int64_t(geometry.fixed_normals[0][axis]) + geometry.fixed_normals[1][axis]) / 2);
	if (normal == FixedLightPoint{}) return std::nullopt;
	const auto checked = [](std::int64_t value) {
		if (value < (std::numeric_limits<std::int32_t>::min)() || value > (std::numeric_limits<std::int32_t>::max)())
			throw std::runtime_error("Classic light view exceeds fixed-point range");
		return static_cast<std::int32_t>(value);
	};
	std::array<ClassicLightView, 5> views;
	for (unsigned corner = 0; corner < 4; ++corner) {
		const auto &vertex = Vertices[Segments[segment].verts[Side_to_verts_int[side][corner]]];
		auto &view = views[corner];
		view.position = { vertex.x, vertex.y, vertex.z };
		FixedLightPoint toward_center;
		for (unsigned axis = 0; axis < 3; ++axis)
			toward_center[axis] = checked(std::int64_t(geometry.center[axis]) - view.position[axis]);
		toward_center = geometry_fixed_direction(toward_center);
		for (unsigned axis = 0; axis < 3; ++axis)
			view.forward[axis] = static_cast<std::int32_t>((std::int64_t(normal[axis]) + toward_center[axis]) / 2);
		view.forward = geometry_fixed_direction(view.forward);
	}
	views[4].forward = normal;
	for (unsigned axis = 0; axis < 3; ++axis)
		views[4].position[axis] = checked(std::int64_t(geometry.center[axis]) + normal[axis]);
	return views;
}

namespace
{
std::vector<int> visible_segments(int start, const std::function<GeometryScreenPoint(int)> &project,
                                  int width, int height, unsigned detail_depth);
}
std::vector<int> classic_camera_visible_segments(int start, const ClassicVisibilityCamera &camera)
{
	if (!geometry_lighting_available() || Highest_vertex_index < 0)
		throw std::runtime_error("Classic camera requires a loaded D1 or admitted XL mine");
	for (const auto factor : camera.projection_xy)
		if (!std::isfinite(factor) || factor <= 0) throw std::runtime_error("Invalid classic perspective projection");
	const auto basis = geometry_scaled_camera(geometry_camera_basis(camera.forward), camera.aspect, camera.zoom);
	std::vector<std::optional<GeometryScreenPoint>> projected(static_cast<std::size_t>(Highest_vertex_index) + 1);
	const auto project = [&](int i) {
		if (projected[i]) return *projected[i];
		const auto &world = Vertices[i];
		const auto view = geometry_camera_view({ world.x, world.y, world.z }, camera.position, basis);
		projected[i] = geometry_screen_point({ view[0] * camera.projection_xy[0], view[1] * camera.projection_xy[1] }, view[2], camera.half_width, camera.half_height);
		return *projected[i];
	};
	return visible_segments(start, project, camera.screen_width, camera.screen_height, camera.detail_depth);
}

std::vector<std::vector<bool>> classic_segment_visibility(const ClassicVisibilityCamera &optics)
{
	if (!geometry_lighting_available() || Highest_segment_index < 0)
		throw std::runtime_error("Classic segment visibility requires a loaded D1 or admitted XL mine");
	const auto count = static_cast<std::size_t>(Highest_segment_index) + 1;
	std::vector<std::vector<bool>> visibility(count, std::vector<bool>(count, false));
	for (int segment = 0; segment <= Highest_segment_index; ++segment) {
		visibility[segment][segment] = true;
		for (int side = 0; side < 6; ++side) {
			const auto geometry = compile_classic_lighting_side(segment, side);
			auto camera = optics;
			camera.position = geometry.segment_center;
			for (unsigned axis = 0; axis < 3; ++axis)
				camera.forward[axis] = -static_cast<std::int32_t>((std::int64_t(geometry.fixed_normals[0][axis]) + geometry.fixed_normals[1][axis]) / 2);
			if (camera.forward == FixedLightPoint{}) continue;
			for (const auto destination : classic_camera_visible_segments(segment, camera)) {
				visibility[segment][destination] = true;
				visibility[destination][segment] = true;
			}
		}
	}
	return visibility;
}

std::vector<int> classic_visible_segments(int start, const std::vector<GeometryScreenPoint> &projected,
                                          int width, int height, unsigned detail_depth)
{
	if (projected.size() != static_cast<std::size_t>(Highest_vertex_index) + 1)
		throw std::runtime_error("Classic visibility projection does not cover loaded vertices");
	return visible_segments(start, [&projected](int id) { return projected[id]; }, width, height, detail_depth);
}

ClassicLightViewCandidates classic_light_view_candidates(int segment, int side, const ClassicVisibilityCamera &optics)
{
	const auto views = classic_light_views(segment, side);
	ClassicLightViewCandidates result;
	std::vector<bool> included(static_cast<std::size_t>(Highest_segment_index) + 1, false);
	included[segment] = true; // Reference sets the source segment before testing views
	if (views)
		for (unsigned index = 0; index < views->size(); ++index) {
			auto camera = optics;
			camera.position = (*views)[index].position;
			camera.forward = (*views)[index].forward;
			result.views[index] = classic_camera_visible_segments(segment, camera);
			for (const auto candidate : result.views[index]) included[candidate] = true;
		}
	for (int candidate = 0; candidate <= Highest_segment_index; ++candidate)
		if (included[candidate]) result.segments.push_back(candidate);
	return result;
}

namespace
{
std::vector<int> visible_segments(int start, const std::function<GeometryScreenPoint(int)> &project,
                                  int width, int height, unsigned detail_depth)
{
	if (!geometry_lighting_available() || Highest_segment_index < 0 || Highest_vertex_index < 0)
		throw std::runtime_error("Classic visibility requires a loaded D1 or admitted XL mine");
	const auto vertex = [&project](int id) {
		if (id < 0 || id > Highest_vertex_index)
			throw std::runtime_error("Invalid classic visibility vertex");
		return project(id);
	};
	const auto prepare_segment = [&](int segment) {
		GeometryPortalSegment prepared;
		bool rotated = false;
		for (int side = 0; side < 6; ++side) {
			if (!classic_light_portal_passable(segment, side)) continue;
			if (!rotated) {
				for (const auto id : Segments[segment].verts) (void) vertex(id);
				rotated = true;
			}
			auto &portal = prepared[side];
			portal.child = Segments[segment].children[side];
			for (int corner = 0; corner < 4; ++corner)
				portal.points[corner] = vertex(Segments[segment].verts[Side_to_verts_int[side][corner]]);
		}
		return prepared;
	};
	const auto count = static_cast<std::size_t>(Highest_segment_index) + 1;
	const auto passes = static_cast<unsigned>(std::min<std::uint64_t>(4 * (std::uint64_t(detail_depth) + 1), count));
	const auto candidates = geometry_portal_traversal(count, prepare_segment, start, width, height, passes);
	std::vector<int> result;
	for (const auto &candidate : candidates) {
		// ProjectAndEncode retains only CC_BEHIND, so SegmentIsVisible's AND
		// becomes true when any of the eight valid classic corners is in front
		bool visible = false;
		for (const auto id : Segments[candidate.segment].verts)
			if (!vertex(id).behind) {
				visible = true;
				break;
			}
		if (visible) result.push_back(candidate.segment);
	}
	return result;
}
} // namespace

namespace
{
bool classic_portal_passable(int segment, int side, bool router)
{
	if (!geometry_lighting_available())
		throw std::runtime_error("Classic light portals require D1 or admitted XL geometry");
	if (segment < 0 || segment > Highest_segment_index || side < 0 || side >= 6)
		throw std::runtime_error("Invalid classic light portal");
	const auto &seg = Segments[segment];
	if (seg.children[side] < 0) return false;
	if (seg.children[side] > Highest_segment_index) throw std::runtime_error("Invalid classic light portal child");
	const auto &surface = seg.sides[side];
	if (surface.wall_num == -1) return true;
	if (surface.wall_num < -1 || surface.wall_num >= Num_walls) throw std::runtime_error("Invalid classic light portal wall");
	const auto &wall = Walls[surface.wall_num];
	if (wall.type == WALL_OPEN || wall.type == WALL_CLOAKED) return true;
	if (wall.type == WALL_DOOR && (!router || (wall.flags & WALL_DOOR_OPENED) || wall.state == WALL_DOOR_OPENING || wall.state == WALL_DOOR_CLOSING)) return true;
	if (router && wall.type == WALL_ILLUSION) return true;
	if (wall.type == WALL_ILLUSION && ((wall.flags & WALL_ILLUSION_OFF) || wall.cloak_value < 31)) return true;
	if (wall.type == WALL_BLASTABLE && ((wall.flags & WALL_BLASTED) || wall.cloak_value < 31)) return true;
	if (wall.cloak_value && wall.cloak_value < 31) return true;
	const auto flags = [](int texture) -> unsigned {
		if (texture < 0 || texture >= NumTextures) throw std::runtime_error("Invalid classic portal texture");
		const auto bitmap = Textures[texture].index;
		if (bitmap < 0 || bitmap >= Num_bitmap_files) throw std::runtime_error("Invalid classic portal bitmap");
		return static_cast<unsigned>(piggy_bitmap_get_flags(&GameBitmaps[bitmap]));
	};
	const int overlay = surface.tmap_num2 & 0x3fff;
	return classic_light_texture_transparent(flags(surface.tmap_num), overlay ? flags(overlay) : 0, overlay != 0);
}
} // namespace

bool classic_light_portal_passable(int segment, int side)
{
	return classic_portal_passable(segment, side, false);
}

ClassicRouteGraph classic_light_route_graph()
{
	if (!geometry_lighting_available() || Highest_vertex_index < 0 || Highest_segment_index < 0)
		throw std::runtime_error("Classic route graph requires a loaded D1 or admitted XL mine");
	FixedLightPoint minimum{ Vertices[0].x, Vertices[0].y, Vertices[0].z }, maximum = minimum;
	for (int i = 1; i <= Highest_vertex_index; ++i) {
		const FixedLightPoint point{ Vertices[i].x, Vertices[i].y, Vertices[i].z };
		for (unsigned axis = 0; axis < 3; ++axis) {
			minimum[axis] = (std::min) (minimum[axis], point[axis]);
			maximum[axis] = (std::max) (maximum[axis], point[axis]);
		}
	}
	ClassicRouteGraph graph;
	graph.scale = (std::max) (1, geometry_fixed_distance(minimum, maximum) / 65536);
	for (int segment = 0; segment <= Highest_segment_index; ++segment)
		graph.centers.push_back(compile_classic_lighting_side(segment, 0).segment_center);
	graph.edges.resize(graph.centers.size());
	for (int segment = 0; segment <= Highest_segment_index; ++segment)
		for (int side = 0; side < 6; ++side) {
			auto &edge = graph.edges[segment][side];
			edge.child = Segments[segment].children[side];
			if (edge.child < 0) continue;
			if (edge.child > Highest_segment_index) throw std::runtime_error("Invalid classic route child");
			edge.distance = (std::max) (1, geometry_fixed_distance(graph.centers[segment], graph.centers[edge.child]));
			const auto cost = (std::max) (1, edge.distance / graph.scale);
			if (cost > 65535) throw std::runtime_error("Classic route edge exceeds Dial heap range");
			edge.cost = static_cast<std::uint16_t>(cost);
			edge.passable = classic_portal_passable(segment, side, true);
		}
	return graph;
}

std::vector<int> classic_light_route_reachable(int start)
{
	if (!geometry_lighting_available() || start < 0 || start > Highest_segment_index)
		throw std::runtime_error("Invalid classic light route start");
	std::vector<bool> seen(static_cast<std::size_t>(Highest_segment_index) + 1, false);
	std::vector<int> pending{ start };
	seen[start] = true;
	for (std::size_t index = 0; index < pending.size(); ++index)
		for (int side = 0; side < 6; ++side) {
			const int segment = pending[index];
			if (!classic_portal_passable(segment, side, true)) continue;
			const int child = Segments[segment].children[side];
			if (!seen[child]) {
				seen[child] = true;
				pending.push_back(child);
			}
		}
	std::sort(pending.begin(), pending.end());
	return pending;
}

ClassicDistanceRow classic_distance_row(const ClassicRouteGraph &graph, const std::vector<std::vector<bool>> &visibility, int start)
{
	const auto count = graph.edges.size();
	if (graph.centers.size() != count || visibility.size() != count)
		throw std::runtime_error("Inconsistent classic distance graph");
	std::vector<std::array<GeometryRouteEdge, 6>> edges(count);
	for (std::size_t i = 0; i < count; ++i) {
		if (visibility[i].size() != count) throw std::runtime_error("Incomplete classic segment visibility");
		for (unsigned side = 0; side < 6; ++side)
			if (graph.edges[i][side].passable) edges[i][side] = { graph.edges[i][side].child, graph.edges[i][side].cost };
	}
	const auto routes = geometry_weighted_routes(edges, start);
	std::vector<GeometryRouteInterior> interiors;
	std::int32_t maximum = 0;
	for (std::size_t i = 0; i < count; ++i) {
		interiors.push_back(geometry_shorten_route(geometry_route_path(routes, static_cast<int>(i)), [&visibility](int a, int b) { return bool(visibility[a][b]); }, [&graph](int a, int b) { return geometry_fixed_distance(graph.centers[a], graph.centers[b]); }));
		maximum = (std::max) (maximum, interiors.back().distance);
	}
	ClassicDistanceRow result;
	result.scale = geometry_route_distance_scale(maximum);
	for (const auto &interior : interiors)
		result.entries.push_back({ geometry_pack_route_distance(interior.distance, result.scale), interior.first, interior.last });
	return result;
}

std::int32_t classic_segment_distance(const ClassicDistanceRow &row, const std::vector<FixedLightPoint> &centers,
                                      int destination, const FixedLightPoint &start_position, const FixedLightPoint &end_position)
{
	if (destination < 0 || static_cast<std::size_t>(destination) >= row.entries.size()) return -1;
	const auto &entry = row.entries[destination];
	const auto distance = geometry_unpack_route_distance(entry.distance, row.scale);
	if (distance < 0) return distance;
	if (entry.first < 0 && entry.last < 0) return geometry_fixed_distance(start_position, end_position);
	if (entry.first < 0 || static_cast<std::size_t>(entry.first) >= centers.size() || (entry.last >= 0 && static_cast<std::size_t>(entry.last) >= centers.size()))
		throw std::runtime_error("Invalid classic distance endpoint");
	const auto first_leg = geometry_fixed_distance(centers[entry.first], start_position);
	const auto last_leg = geometry_fixed_distance(centers[entry.last < 0 ? entry.first : entry.last], end_position);
	const auto total = std::int64_t(first_leg) + last_leg + (entry.last < 0 ? 0 : distance);
	if (total > (std::numeric_limits<std::int32_t>::max)()) throw std::runtime_error("Reconstructed classic distance exceeds fixed-point range");
	return static_cast<std::int32_t>(total);
}
} // namespace xl_compat
