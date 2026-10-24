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
