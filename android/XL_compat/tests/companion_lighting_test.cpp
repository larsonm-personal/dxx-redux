#include "companion_lighting.h"
#include "compatibility.h"
#include "inventory.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>

static void check(bool value, const char *reason)
{
	if (!value) throw std::runtime_error(reason);
}
template <typename Decode>
static void malformed(Decode decode, std::size_t size)
{
	for (const auto length : { std::size_t(0), size - 1, size + 1, size * 2 }) {
		xl_compat::Bytes bytes(length);
		bool rejected = false;
		try {
			decode(bytes.data(), bytes.size());
		} catch (const std::runtime_error &) {
			rejected = true;
		}
		check(rejected, "malformed lighting companion accepted");
	}
	bool rejected = false;
	try {
		decode(nullptr, size);
	} catch (const std::runtime_error &) {
		rejected = true;
	}
	check(rejected, "null lighting data accepted");
}
int main(int argc, char **argv)
{
	try {
		using namespace xl_compat;
		TextureBrightnessTable strengths{};
		TextureColorTable palette{};
		strengths[1] = 65536;
		strengths[2] = 131072;
		strengths[3] = 32768;
		strengths[4] = -1;
		palette[1].channels = { 0x7fffffff, 0, 0 };
		palette[2].channels = { 0, 0x7fffffff, 0 };
		ClassicFaceLightMaterials materials(strengths, palette);
		auto face = materials.face(1, 2, false);
		check(face && face->created_texture == 1 && face->material_texture == 2 && face->brightness == 2 && face->ambient && face->rgb == std::array<float, 3>{ 0, 1, 0 }, "overlay must update rather than add or recreate base light");
		face = materials.face(1, 3, false);
		check(face && face->rgb == std::array<float, 3>{ 0, 0, 0 }, "black update incorrectly received creation fallback");
		face = materials.face(0, 3, false);
		check(face && face->created_texture == 3 && !face->ambient && face->rgb == std::array<float, 3>{ 1, 1, 1 }, "new classic black light must become white");
		face = materials.face(1, 3, false);
		check(face && face->rgb == std::array<float, 3>{ 1, 1, 1 }, "creation fallback must persist for later uses of the texture");
		face = materials.face(1, 4, false);
		check(face && face->material_texture == 1 && face->ambient, "negative overlay brightness must not replace base material");
		check(!materials.face(-1, 2, false) && !materials.face(910, 2, false) && !materials.face(0, 0, false), "invalid or unlit face created a light");
		ClassicFaceLightMaterials fresh(strengths, palette);
		check(fresh.face(1, 3, false)->rgb == std::array<float, 3>{ 0, 0, 0 }, "material state leaked between level profiles");
		malformed(decode_texture_brightness, companion_texture_count * 4);
		malformed(decode_texture_colors, companion_texture_count * 13);
		Bytes brightness(companion_texture_count * 4), colors(companion_texture_count * 13);
		brightness[3] = 0x80;
		brightness[4] = brightness[5] = brightness[6] = brightness[7] = 0xff;
		brightness.back() = 0x01;
		const auto b = decode_texture_brightness(brightness.data(), brightness.size());
		check(b[0] == std::numeric_limits<std::int32_t>::min() && b[1] == -1 && b.back() == 0x01000000, "LGT signed endian or final entry mismatch");
		colors[0] = 255;
		colors[1] = colors[2] = colors[3] = 255;
		colors[4] = 127;
		colors[8] = 128;
		colors.back() = 1;
		const auto c = decode_texture_colors(colors.data(), colors.size());
		const auto report = compatibility_json("mission/LEVEL.CLR", colors);
		check(report.find("\"detected\": true") != std::string::npos && report.find("\"status\": \"blocked\"") != std::string::npos && report.find("decoded layout only") != std::string::npos, "decoded companion falsely qualified or not detected");
		check(compatibility_json("broken.lgt", {}).find("Cannot decode lighting companion") != std::string::npos, "malformed companion lacks diagnostic");
		check(c[0].serialized_index == 255 && c[0].channels[0] == 0x7fffffff && c[0].channels[1] == std::numeric_limits<std::int32_t>::min() && c.back().channels[2] == 0x01000000, "CLR packing or signed channels mismatch");
		if (argc == 2) {
			std::ifstream file(argv[1], std::ios::binary);
			check(bool(file), "cannot open original HOG");
			const Bytes bytes{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
			bool lgt = false, clr = false;
			TextureBrightnessTable original_brightness{};
			TextureColorTable original_colors{};
			for (const auto &member : read_hog(bytes)) {
				if (member.name == "drgpit.lgt") {
					check(compatibility_json(member.name, member.bytes).find("69 nonzero") != std::string::npos, "original brightness browser evidence missing");
					const auto table = decode_texture_brightness(member.bytes.data(), member.bytes.size());
					original_brightness = table;
					check(std::count_if(table.begin(), table.end(), [](auto v) { return v != 0; }) == 69 && *std::max_element(table.begin(), table.end()) == 131072, "original LGT values differ");
					lgt = true;
				}
				if (member.name == "drgpit.clr") {
					check(compatibility_json(member.name, member.bytes).find("103 chromatic") != std::string::npos, "original color browser evidence missing");
					const auto table = decode_texture_colors(member.bytes.data(), member.bytes.size());
					original_colors = table;
					check(std::count_if(table.begin(), table.end(), [](const auto &v) { return v.channels[0] != v.channels[1] || v.channels[1] != v.channels[2]; }) == 103, "original CLR chromatic count differs");
					clr = true;
				}
			}
			check(lgt && clr, "original lighting companions missing");
			ClassicFaceLightMaterials original(original_brightness, original_colors);
			const auto orange = original.face(286, 0, false);
			check(orange && orange->brightness == 85150.0f / 65536 && orange->rgb[0] > orange->rgb[1] && orange->rgb[1] > orange->rgb[2], "original texture 286 light material differs");
			const auto doubled = original.face(333, 0, false);
			check(doubled && doubled->brightness == 2, "original texture 333 strength differs");
		}
		std::cout << "Lighting companion layout passed; no renderer qualification\n";
		return 0;
	} catch (const std::exception &e) {
		std::cerr << e.what() << '\n';
		return 1;
	}
}
