#include "lighting.h"
#include "inventory.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace
{
void expect(const xl_compat::VertexLighting &input, xl_compat::Rgb expected, int scalar)
{
	const auto result = xl_compat::shade_vertex(input);
	if (result.scalar_light != scalar) throw std::runtime_error("scalar light mismatch");
	for (unsigned i = 0; i < 3; ++i)
		if (std::abs(result.rgb[i] - expected[i]) > 0.000001f) throw std::runtime_error("RGB mismatch");
}
} // namespace
int main()
{
	try {
		xl_compat::GeometryColorInput self_light;
		self_light.self = true;
		self_light.light_color = { 1, 0.5f, 0.25f };
		self_light.material_ambient = { 0.3f, 0.3f, 0.3f };
		self_light.material_diffuse = { 0.7f, 0.7f, 0.7f };
		self_light.attenuation = { 0, 0, 0, 1, 0.05f, 0.005f, 100, 2 };
		const auto self_color = xl_compat::geometry_source_color(self_light);
		if (!self_color || *self_color != xl_compat::Rgb{ 2, 1, 0.5f }) throw std::runtime_error("Self source color differs");
		xl_compat::GeometryColorSum sum;
		sum.add(self_color, true);
		sum.add(xl_compat::Rgb{ 0, 1, 0.5f }, true);
		sum.add(xl_compat::Rgb{ 1, 0, 0 }, false);
		if (sum.finish() != xl_compat::Rgb{ 1, 0.5f, 0.25f }) throw std::runtime_error("Self averaging or peak-channel normalization differs");
		xl_compat::GeometryColorSum rejected_self;
		rejected_self.add(xl_compat::Rgb{ 1, 0, 0 }, true);
		rejected_self.add(std::nullopt, true);
		if (rejected_self.finish() != xl_compat::Rgb{ 0.5f, 0, 0 }) throw std::runtime_error("Rejected self source lost denominator");
		const auto active = xl_compat::geometry_active_lights({ { 0, 20000 }, { 1, 0 }, { 2, 0 }, { 3, -40000 }, { 1, -90000 }, { 4, 81920000 } });
		if (active.size() != 4 || active[0].index != 1 || active[1].index != 0 || active[2].index != 2 || active[3].index != 3)
			throw std::runtime_error("Active light bucket collision or duplicate handling differs");
		if (xl_compat::geometry_ambient_distance(100, 0, 200) != -100 ||
		    xl_compat::geometry_ambient_distance(223 * 65536, 1, 0) != 223 * 65536 ||
		    xl_compat::geometry_ambient_distance(223 * 65536 + 1, 1, 0) ||
		    xl_compat::geometry_ambient_distance(16777217, 1, 3000000) != 13777216)
			throw std::runtime_error("Ambient contribution radius, boundary or float conversion differs");
		std::vector<std::array<xl_compat::GeometryRouteEdge, 6>> routes(5);
		routes[0][0] = { 1, 2 };
		routes[0][1] = { 2, 2 };
		routes[0][2] = { 3, 20 };
		routes[1][0] = { 3, 3 };
		routes[2][0] = { 3, 3 };
		routes[3][0] = { 0, 1 };
		auto weighted = xl_compat::geometry_weighted_routes(routes, 0);
		if (xl_compat::geometry_route_path(weighted, 3) != std::vector<int>{ 0, 2, 3 } || !xl_compat::geometry_route_path(weighted, 4).empty())
			throw std::runtime_error("route predecessor reconstruction differs");
		const std::vector<int> path{ 0, 1, 2, 3, 4, 5 };
		const auto distance = [](int a, int b) { return b == a + 1 ? 100 : 10; };
		const auto open_route = xl_compat::geometry_shorten_route(path, [](int, int) { return true; }, distance);
		const auto closed_route = xl_compat::geometry_shorten_route(path, [](int, int) { return false; }, distance);
		const auto partial_route = xl_compat::geometry_shorten_route(path, [](int a, int b) { return a == 1 && b == 3; }, distance);
		if (open_route.distance != 10 || open_route.first != 1 || open_route.last != 4 || closed_route.distance != 300 || partial_route.distance != 110)
			throw std::runtime_error("route visibility shortening or endpoint exclusion differs");
		const auto short_route = xl_compat::geometry_shorten_route({ 0, 1, 2 }, [](int, int) { return false; }, distance);
		if (short_route.distance != 0 || short_route.first != 1 || short_route.last != 1)
			throw std::runtime_error("three-node route interior differs");
		if (weighted[3].cost != 5 || weighted[3].predecessor != 2 || weighted[3].side != 0 || weighted[4].cost != -1 || weighted[0].cost != 0)
			throw std::runtime_error("weighted route tie, decrease-key, cycle or unreachable behavior differs");
		routes[0][0].cost = 65535;
		routes[0][1].child = routes[0][2].child = -1;
		routes[1][0].cost = 65535;
		weighted = xl_compat::geometry_weighted_routes(routes, 0);
		if (weighted[3].cost != 131070 || weighted[3].predecessor != 1)
			throw std::runtime_error("Dial bucket wrap lost route cost");
		const float route_scale = xl_compat::geometry_route_distance_scale(131068);
		if (route_scale != 2 || xl_compat::geometry_route_distance_scale(65534) != 1)
			throw std::runtime_error("route row scale threshold differs");
		if (xl_compat::geometry_pack_route_distance(-1, route_scale) != 65535 || xl_compat::geometry_unpack_route_distance(65535, route_scale) != -1)
			throw std::runtime_error("unreachable route sentinel lost");
		if (xl_compat::geometry_pack_route_distance(131068, route_scale) != 65534 || xl_compat::geometry_unpack_route_distance(65534, route_scale) != 131068)
			throw std::runtime_error("maximum reachable route collides with sentinel");
		if (xl_compat::geometry_pack_route_distance(1, route_scale) != 0 || xl_compat::geometry_unpack_route_distance(0, route_scale) != 0)
			throw std::runtime_error("small reachable route became unreachable after quantization");
		bool route_overflow = false;
		try {
			(void) xl_compat::geometry_pack_route_distance(65535, 1);
		} catch (const std::runtime_error &) {
			route_overflow = true;
		}
		if (!route_overflow) throw std::runtime_error("route sentinel collision admitted");
		route_overflow = false;
		try {
			(void) xl_compat::geometry_unpack_route_distance(65534, 65536);
		} catch (const std::runtime_error &) {
			route_overflow = true;
		}
		if (!route_overflow) throw std::runtime_error("route reconstruction overflow admitted");
		const std::array<xl_compat::FixedLightPoint, 3> identity{ xl_compat::FixedLightPoint{ 65536, 0, 0 }, { 0, 65536, 0 }, { 0, 0, 65536 } };
		const auto zoom_in = xl_compat::geometry_scaled_camera(identity, { 32768, 65536, 65536 }, 32768);
		if (zoom_in[0][0] != 32768 || zoom_in[1][1] != 65536 || zoom_in[2][2] != 32768)
			throw std::runtime_error("camera zoom-in scaling differs");
		const auto zoom_out = xl_compat::geometry_scaled_camera(identity, { 32768, 65536, 65536 }, 131072);
		if (zoom_out[0][0] != 16384 || zoom_out[1][1] != 32768 || zoom_out[2][2] != 65536)
			throw std::runtime_error("camera zoom-out scaling differs");
		auto fractional_basis = identity;
		fractional_basis[0][0] = -1;
		if (xl_compat::geometry_scaled_camera(fractional_basis, { 32768, 65536, 65536 }, 65536)[0][0] != 0)
			throw std::runtime_error("camera multiplication rounded negative fraction down");
		if (xl_compat::geometry_camera_view({ 196608, 262144, 327680 }, { 65536, 65536, 65536 }, zoom_out) != xl_compat::LightPoint{ 0.5f, 1.5f, 4.0f })
			throw std::runtime_error("camera translation or row order differs");
		if (xl_compat::geometry_camera_view({ 1073741825, 0, 0 }, { 1073741824, 0, 0 }, identity) != xl_compat::LightPoint{})
			throw std::runtime_error("camera subtraction preceded float conversion");
		// Two routes into segment 1: the later wider view reveals segment 3
		std::vector<xl_compat::GeometryPortalSegment> graph(4);
		const auto connect = [&graph](int from, int side, int to, int left, int right) {
			graph[from][side] = { to, { xl_compat::GeometryScreenPoint{ left, 100, false }, { right, 100, false }, { right, 400, false }, { left, 400, false } } };
		};
		connect(0, 0, 1, 100, 200);
		connect(0, 1, 2, 300, 400);
		connect(1, 0, 3, 300, 400);
		connect(2, 0, 1, 300, 400);
		connect(3, 0, 0, 100, 400); // Cycle must not duplicate the starting segment
		if (xl_compat::geometry_portal_traversal(graph, 0, 1024, 1024, 2).size() != 3)
			throw std::runtime_error("portal traversal ignored its pass boundary");
		const auto visits = xl_compat::geometry_portal_traversal(graph, 0, 1024, 1024, 4);
		if (visits.size() != 4 || visits[1].segment != 1 || visits[2].segment != 2 || visits[3].segment != 3 || visits[1].portal.left != 100 || visits[1].portal.right != 400 || visits[3].portal.left != 300)
			throw std::runtime_error("expanded portal was not revisited in reference order");
		graph[2][0].child = -1;
		const auto lazy = xl_compat::geometry_portal_traversal(graph.size(), [&graph](int index) {
			if (index == 3) throw std::runtime_error("unvisited segment was projected");
			return graph[index]; }, 0, 1024, 1024, 4);
		if (lazy.size() != 3) throw std::runtime_error("lazy traversal candidate count differs");
		if (xl_compat::geometry_portal_traversal(graph, 0, 1024, 1024, 4).size() != 3)
			throw std::runtime_error("traversal substituted connectivity for portal visibility");
		const xl_compat::GeometryPortal current{ 100, 300, 100, 300 };
		std::array<xl_compat::GeometryScreenPoint, 4> opening{ xl_compat::GeometryScreenPoint{ 200, 200, false }, { 400, 200, false }, { 400, 400, false }, { 200, 400, false } };
		const auto clipped = xl_compat::geometry_light_portal(opening, current, 1024, 1024);
		if (!clipped || clipped->left != 200 || clipped->right != 300 || clipped->top != 200 || clipped->bottom != 300)
			throw std::runtime_error("portal intersection differs");
		for (auto &point : opening) point.x += 100;
		const auto touching = xl_compat::geometry_light_portal(opening, current, 1024, 1024);
		if (!touching || touching->left != 300 || touching->right != 300)
			throw std::runtime_error("edge-touching portal rejected");
		for (auto &point : opening) ++point.x;
		if (xl_compat::geometry_light_portal(opening, current, 1024, 1024))
			throw std::runtime_error("disjoint portal admitted");
		opening[0].behind = true;
		const auto mixed = xl_compat::geometry_light_portal(opening, current, 1024, 1024);
		if (!mixed || mixed->left != current.left || mixed->right != current.right || mixed->top != current.top || mixed->bottom != current.bottom)
			throw std::runtime_error("mixed-depth portal did not inherit current bounds");
		for (auto &point : opening) point.behind = true;
		if (xl_compat::geometry_light_portal(opening, current, 1024, 1024))
			throw std::runtime_error("fully behind portal admitted");
		opening[0].behind = false;
		for (auto &point : opening) point.x += 1024;
		if (xl_compat::geometry_light_portal(opening, current, 1024, 1024))
			throw std::runtime_error("off-screen mixed-depth portal admitted");
		if (!xl_compat::classic_light_texture_transparent(1, 1, true) || !xl_compat::classic_light_texture_transparent(0, 2, true) || !xl_compat::classic_light_texture_transparent(2, 0, false))
			throw std::runtime_error("classic light transparency omitted an eligible texture combination");
		if (xl_compat::classic_light_texture_transparent(1, 0, true) || xl_compat::classic_light_texture_transparent(0, 1, true))
			throw std::runtime_error("classic light transparency admitted an opaque texture combination");
		if (xl_compat::geometry_vertex_candidate(100000000, 0, true, false) != 0)
			throw std::runtime_error("own light vertex did not bypass range and visibility");
		if (xl_compat::geometry_vertex_candidate(0, 10, false, false))
			throw std::runtime_error("hidden source admitted by distance alone");
		if (xl_compat::geometry_vertex_candidate(223 * 65536, 0, false, true))
			throw std::runtime_error("out-of-range geometry source admitted");
		if (xl_compat::geometry_vertex_candidate(223 * 65536, 10, false, true) != 222 * 65536)
			throw std::runtime_error("source radius adjustment or inclusive cutoff differs");
		if (xl_compat::geometry_vertex_candidate(0, 10, false, true) != -65536)
			throw std::runtime_error("negative radius-adjusted distance was clamped");
		const auto emission = xl_compat::geometry_emission_direction({ xl_compat::FixedLightPoint{ 65536, 0, 0 }, { 0, 0, 65536 } });
		if (std::abs(emission[0] - 1 / std::sqrt(2.0f)) > 1e-6f || emission[1] != 0 || emission[0] != emission[2])
			throw std::runtime_error("geometry emission direction is not a normalized fixed-normal average");
		if (xl_compat::geometry_emission_direction({ xl_compat::FixedLightPoint{ -1, 0, 0 }, { 0, 0, 0 } }) != xl_compat::LightPoint{})
			throw std::runtime_error("geometry emission average did not truncate before float conversion");
		const std::array<xl_compat::LightPoint, 2> orientation_normals{ xl_compat::LightPoint{ 0, 0, -1 }, { 0, 0, 1 } };
		const std::array<xl_compat::FixedLightPoint, 2> orientation_fixed{ xl_compat::FixedLightPoint{ 0, 0, -65536 }, { 0, 0, 65536 } };
		const auto oriented = xl_compat::geometry_orient_normals(orientation_normals, orientation_fixed, { 0, 0, 1 }, { 0, 0, 65536 });
		if (oriented[0] != xl_compat::LightPoint{ 0, 0, -1 } || oriented[1] != xl_compat::LightPoint{ 0, 0, 1 } || oriented[2] != xl_compat::LightPoint{ 0, 0, 1 })
			throw std::runtime_error("reference double flip or first-pass average changed");
		const auto fractional = xl_compat::geometry_orient_normals(orientation_normals, orientation_fixed, { 0, 0, 0 }, { 0, 0, 0 });
		if (fractional[0] != orientation_normals[0] || fractional[2] != xl_compat::LightPoint{})
			throw std::runtime_error("zero orientation direction changed normals");
		const std::array<xl_compat::FixedLightPoint, 2> tiny_fixed{ xl_compat::FixedLightPoint{ 0, 0, -1 }, { 0, 0, 1 } };
		const auto tiny = xl_compat::geometry_orient_normals(orientation_normals, tiny_fixed, { 0, 0, 0 }, { 0, 0, 1 });
		if (tiny[0] != orientation_normals[0]) throw std::runtime_error("fractional fixed dot must truncate before orientation check");
		const std::array<xl_compat::LightPoint, 4> center_corners{ xl_compat::LightPoint{ 0, 0, 0 }, { 2, 0, 0 }, { 2, 2, 0 }, { 0, 2, 2 } };
		const std::array<xl_compat::FixedLightPoint, 2> center_normals{ xl_compat::FixedLightPoint{ 0, 0, 65536 }, { 0, 0, 65536 } };
		if (xl_compat::geometry_side_center(center_corners, center_normals, {}) != xl_compat::FixedLightPoint{ 65536, 65536, 0 })
			throw std::runtime_error("side center did not project to first plane");
		if (xl_compat::geometry_side_center(center_corners, {}, {}) != xl_compat::FixedLightPoint{ 65536, 65536, 32768 })
			throw std::runtime_error("parallel side center did not retain average");
		auto displaced_corners = center_corners;
		displaced_corners[3][2] = -8;
		if (xl_compat::geometry_side_center(displaced_corners, center_normals, {}) != xl_compat::FixedLightPoint{ 65536, 65536, 0 })
			throw std::runtime_error("negative side-center intersection fraction was rejected");
		displaced_corners[3][2] = 8;
		if (xl_compat::geometry_side_center(displaced_corners, center_normals, {}) != xl_compat::FixedLightPoint{ 65536, 65536, 131072 })
			throw std::runtime_error("out-of-segment intersection did not retain original center");
		const auto normal = xl_compat::geometry_triangle_normal({ 0, 0, 0 }, { 2, 0, 0 }, { 2, 3, 4 });
		if (std::abs(normal[0]) > 1e-6f || std::abs(normal[1] + 0.8f) > 1e-6f || std::abs(normal[2] - 0.6f) > 1e-6f)
			throw std::runtime_error("float triangle normal differs");
		if (xl_compat::geometry_triangle_normal({ 1, 2, 3 }, { 1, 2, 3 }, { 4, 5, 6 }) != xl_compat::LightPoint{})
			throw std::runtime_error("degenerate triangle normal differs");
		xl_compat::GeometryLightFace face;
		face.corners = { xl_compat::LightPoint{ 0, 0, 0 }, { 2, 0, 0 }, { 2, 2, 0 }, { 0, 2, 0 } };
		face.triangles = { std::array<unsigned, 3>{ 0, 1, 2 }, { 0, 2, 3 } };
		face.normals = { xl_compat::LightPoint{ 0, 0, 1 }, { 0, 0, 1 } };
		for (const auto &pair : {
		         std::pair<xl_compat::LightPoint, xl_compat::LightPoint>{ { 1, 1, 5 }, { 1, 1, 0 } },
		         { { 0.25f, 1.75f, -2 }, { 0.25f, 1.75f, 0 } },
		         { { 3, 1, 2 }, { 2, 1, 0 } },
		         { { -1, -1, 2 }, { 0, 0, 0 } },
		         { { 1, 1, 0.0005f }, { 1, 1, 0.0005f } } })
			if (xl_compat::geometry_light_point(face, pair.first) != pair.second) throw std::runtime_error("geometry emitting-face point mismatch");
		face.corners[3][2] = 2;
		const float unit = 1.0f / std::sqrt(3.0f);
		face.normals[1] = { unit, -unit, unit };
		const xl_compat::LightPoint receiver{ 1, 1, 1 };
		if (xl_compat::geometry_light_point(face, receiver) != xl_compat::LightPoint{ 1, 1, 0 }) throw std::runtime_error("first containing triangle not retained");
		std::swap(face.triangles[0], face.triangles[1]);
		std::swap(face.normals[0], face.normals[1]);
		const auto reversed = xl_compat::geometry_light_point(face, receiver);
		if (std::abs(reversed[0] - 2.0f / 3) > 1e-6f || std::abs(reversed[1] - 4.0f / 3) > 1e-6f || std::abs(reversed[2] - 2.0f / 3) > 1e-6f)
			throw std::runtime_error("nonplanar face projection order differs");
		face.normals = {};
		if (xl_compat::geometry_light_point(face, { 1, -1, 0 }) != xl_compat::LightPoint{ 1, 0, 0 }) throw std::runtime_error("invalid plane normal did not use perimeter fallback");
		const auto tied = xl_compat::nearest_geometry_lights({ { 0, 0 }, { 1, 0 }, { 2, 0 }, { 3, 0 } });
		if (tied.size() != 4 || tied[0].index != 2 || tied[1].index != 3 || tied[2].index != 0 || tied[3].index != 1)
			throw std::runtime_error("equal-distance geometry ordering changed");
		xl_compat::GeometryLightSample geometry{ 10, 1, 1, 1, 0.05f, 0.005f, 222, 1 };
		auto attenuation = xl_compat::geometry_attenuation(geometry);
		if (!attenuation || attenuation->attenuation != 2) throw std::runtime_error("geometry distance attenuation mismatch");
		geometry.emission_angle = -0.5f;
		attenuation = xl_compat::geometry_attenuation(geometry);
		if (!attenuation || attenuation->distance < 39.99f || attenuation->distance > 40.01f) throw std::runtime_error("back-facing geometry decay missing");
		geometry.distance = 1000;
		if (xl_compat::geometry_attenuation(geometry)) throw std::runtime_error("geometry cutoff ignored");
		geometry.distance = 0;
		geometry.brightness = 2;
		attenuation = xl_compat::geometry_attenuation(geometry);
		if (!attenuation || attenuation->normal_dot != 1 || attenuation->attenuation != 0.5f) throw std::runtime_error("self-light attenuation mismatch");
		geometry.brightness = 0;
		if (xl_compat::geometry_attenuation(geometry)) throw std::runtime_error("nonpositive light admitted");
		if (xl_compat::flare_intensity(65536, 131072, 0) != 114688 ||
		    xl_compat::flare_intensity(65536, 131072, 32767) != 131072 ||
		    xl_compat::flare_intensity(65536, 32768, 0) != 57344 ||
		    xl_compat::flare_intensity(65536, 0, 32767) != 0)
			throw std::runtime_error("XL flare range/lifetime formula mismatch");
		xl_compat::Inventory level;
		level.level_version = 15;
		level.game_version = 33;
		level.vertices.resize(8);
		xl_compat::Segment segment{};
		segment.children.fill(0);
		for (unsigned i = 0; i < 8; ++i) segment.vertices[i] = i;
		segment.children[0] = segment.children[1] = -1;
		segment.sides[0].uvl = { 0, 0, 8192, 0, 0, 16384, 0, 0, 24576, 0, 0, 32768 };
		segment.sides[1].uvl = { 0, 0, 4096, 0, 0, 65535, 0, 0, 16384, 0, 0, 16384 };
		// A portal's uninitialized UVLs must not contribute
		segment.sides[2].uvl.fill(65535);
		level.segments.push_back(segment);
		const std::vector<float> expected_brightness{ 0.125f, 0, 0.75f, 1, 65535.0f / 32768, 0, 0.5f, 0.5f };
		if (xl_compat::vertex_brightness(level) != expected_brightness)
			throw std::runtime_error("shared-vertex brightness mismatch");
		level.level_version = 17;
		level.game_version = 36;
		if (xl_compat::vertex_brightness(level) != expected_brightness)
			throw std::runtime_error("17/36 shared-vertex brightness mismatch");
		// A wall on the same portal makes its serialized UVLs contribute
		level.segments[0].sides[2].wall = 0;
		if (xl_compat::vertex_brightness(level)[1] != 65535.0f / 32768)
			throw std::runtime_error("portal wall brightness omitted");
		xl_compat::VertexLighting input;
		input.ambient = { 1, 0.5f, 0.25f };
		input.static_light = 32768;
		expect(input, { 0.5f, 0.25f, 0.125f }, 32768);
		// A dim ambient color is not multiplied by the scalar again
		input.ambient = { 0.25f, 0.125f, 0 };
		expect(input, { 0.25f, 0.125f, 0 }, 32768);
		input.flashing = true;
		input.flash_scale = 16384;
		expect(input, { 0.125f, 0.0625f, 0 }, 8192);
		input.flashing = false;
		input.extra_game_info = input.has_dynamic_color = true;
		input.dynamic_light = 16384;
		input.dynamic_color = { 0, 0, 1 };
		input.vertex_brightness = 0.5f;
		expect(input, { 0.25f, 0.125f, 0.25f }, 49152);
		input.darkness = true;
		expect(input, { 0, 0, 0.25f }, 16384);
		input.ambient_enabled = false;
		expect(input, { 0.25f, 0.25f, 0.25f }, 16384);
		input.scale_light = true;
		input.headlight = true;
		expect(input, { 0.1f, 0.1f, 0.1f }, 16384);
		input = {};
		input.static_light = 65536;
		input.face = { 0.8f, 0.4f, 0.2f };
		input.ambient = { 0.5f, 0.25f, 1 };
		input.blend = 1;
		expect(input, { 0.4f, 0.1f, 0.2f }, 65536);
		input.blend = 2;
		input.face_alpha = 0.5f;
		expect(input, { 0.65f, 0.325f, 0.6f }, 65536);
		input.extra_game_info = input.has_dynamic_color = true;
		input.mix_colors = false;
		input.dynamic_color = { 1, 0.5f, 0 };
		input.static_light = 8192;
		expect(input, { 0.5f, 0.25f, 0 }, 8192);
		input.outside_endlevel = true;
		expect(input, { 0.125f, 0.125f, 0.125f }, 8192);
		std::cout << "XL lighting analytical cases passed; rendering parity not established\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
