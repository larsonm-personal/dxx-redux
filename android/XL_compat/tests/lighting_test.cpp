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
		if (xl_compat::geometry_side_center(center_corners, center_normals) != xl_compat::FixedLightPoint{ 65536, 65536, 0 })
			throw std::runtime_error("side center did not project to first plane");
		if (xl_compat::geometry_side_center(center_corners, {}) != xl_compat::FixedLightPoint{ 65536, 65536, 32768 })
			throw std::runtime_error("parallel side center did not retain average");
		auto displaced_corners = center_corners;
		displaced_corners[3][2] = -8;
		if (xl_compat::geometry_side_center(displaced_corners, center_normals) != xl_compat::FixedLightPoint{ 65536, 65536, 0 })
			throw std::runtime_error("negative side-center intersection fraction was rejected");
		displaced_corners[3][2] = 8;
		if (xl_compat::geometry_side_center(displaced_corners, center_normals) != xl_compat::FixedLightPoint{ 65536, 65536, 131072 })
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
