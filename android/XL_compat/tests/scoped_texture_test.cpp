#include "scoped_texture.h"
#include "scoped_texture_image.h"
#include <fstream>
#include <iterator>
#include <iostream>
#include <stdexcept>
static void require(bool value)
{
	if (!value) throw std::runtime_error("Scoped static texture path contract failed");
}
static void image_controls()
{
	require(xl_compat::scoped_texture_factor(256, 2816, 0) == 1);
	require(xl_compat::scoped_texture_factor(256, 2816, 256) == 1);
	require(xl_compat::scoped_texture_factor(256, 2816, 64) == 4);
	require(xl_compat::scoped_texture_factor(300, 600, 64) == 1);
	require(xl_compat::scoped_texture_factor(0, 3, 64) == 1);
	xl_compat::PogBitmap strip;
	strip.width = 2;
	strip.height = 4;
	strip.frames = 2;
	strip.pixels = { 120, 88, 128, 0, 120, 88, 128, 0, 9, 19, 29, 80, 21, 31, 41, 120,
		             120, 88, 128, 0, 12, 24, 36, 48, 18, 30, 42, 54, 24, 36, 48, 60 };
	xl_compat::shrink_scoped_texture(strip, 2);
	require(strip.width == 1 && strip.height == 2 && strip.frames == 2);
	require(strip.pixels == std::vector<unsigned char>{ 120, 88, 128, 0, 18, 30, 42, 54 });
	unsigned cases = 0;
	for (unsigned channels : { 3u, 4u })
		for (unsigned descriptor : { 0u, 16u, 32u, 48u }) {
			// A repeated row is followed by two distinct raw pixels
			std::vector<unsigned char> bytes(18, 0);
			bytes[2] = 10;
			bytes[12] = 2;
			bytes[14] = 2;
			bytes[16] = static_cast<unsigned char>(channels * 8);
			bytes[17] = static_cast<unsigned char>(descriptor | (channels == 4 ? 8 : 0));
			bytes.insert(bytes.end(), { 129, 11, 22, 33 });
			if (channels == 4) bytes.push_back(128);
			bytes.insert(bytes.end(), { 1, 11, 22, 33 });
			if (channels == 4) bytes.push_back(128);
			bytes.insert(bytes.end(), { 55, 66, 77 });
			if (channels == 4) bytes.push_back(255);
			const auto decoded = xl_compat::read_scoped_texture_image(bytes.data(), bytes.size());
			require(decoded.width == 2 && decoded.height == 2 && decoded.rgb_opaque == (channels == 3));
			for (unsigned y = 0; y < 2; ++y)
				for (unsigned x = 0; x < 2; ++x) {
					const bool last = x == (descriptor & 16 ? 0u : 1u) && y == (descriptor & 32 ? 1u : 0u);
					const auto at = (y * 2 + x) * 4;
					require(decoded.pixels[at] == (last ? 77 : 33) && decoded.pixels[at + 1] == (last ? 66 : 22) &&
					        decoded.pixels[at + 2] == (last ? 55 : 11) && decoded.pixels[at + 3] == (last || channels == 3 ? 255 : 128));
				}
			// The same uncompressed source-order pixels retain existing alpha metadata
			std::vector<unsigned char> raw(18, 0);
			raw[2] = 2;
			raw[10] = 1;
			raw[12] = raw[14] = 2;
			raw[16] = static_cast<unsigned char>(channels * 8);
			for (unsigned pixel = 0; pixel < 4; ++pixel)
				raw.insert(raw.end(), decoded.pixels.begin() + pixel * 4, decoded.pixels.begin() + pixel * 4 + channels);
			const auto ordinary = xl_compat::read_particle_image(raw.data(), raw.size());
			require(ordinary.pixels == decoded.pixels && ordinary.flags == decoded.flags && ordinary.transparent_frames == decoded.transparent_frames && ordinary.average_rgb == decoded.average_rgb);
			for (unsigned variant = 0; variant < 9; ++variant) {
				auto malformed = bytes;
				if (variant == 0) malformed.resize(18);
				if (variant == 1) malformed.pop_back();
				if (variant == 2) malformed[18] = 132;
				if (variant == 3) malformed[18] = 127;
				if (variant == 4) malformed[17] |= 64;
				if (variant == 5) malformed[8] = 1;
				if (variant == 6) malformed[0] = 127;
				if (variant == 7) malformed[16] = 16;
				if (variant == 8) malformed[18] = 130;
				bool rejected = false;
				try {
					(void) xl_compat::read_scoped_texture_image(malformed.data(), malformed.size());
				} catch (const std::runtime_error &) {
					rejected = true;
				}
				require(rejected);
			}
			++cases;
		}
	std::cout << "Scoped RLE image: " << cases << " row/packet/orientation/alpha profiles and 72 malformed controls passed\n";
	for (unsigned channels : { 3u, 4u }) {
		std::vector<unsigned char> bytes(18, 0);
		bytes[2] = 10;
		bytes[12] = 4;
		bytes[14] = 1;
		bytes[16] = static_cast<unsigned char>(channels * 8);
		bytes[17] = 32;
		bytes.push_back(3);
		for (const auto &pixel : std::array<std::array<unsigned char, 4>, 4>{ { { 128, 88, 120, 255 }, { 9, 8, 7, 0 }, { 33, 22, 11, 223 }, { 66, 55, 44, 224 } } })
			bytes.insert(bytes.end(), pixel.begin(), pixel.begin() + channels);
		const auto decoded = xl_compat::read_scoped_texture_image(bytes.data(), bytes.size());
		require(decoded.pixels[0] == 120 && decoded.pixels[1] == 88 && decoded.pixels[2] == 128);
		require(decoded.pixels[3] == (channels == 4 ? 0 : 255));
		require(decoded.transparency(0) == (channels == 4 ? -1 : 0));
		require(decoded.pixels[4] == (channels == 4 ? 0 : 7) && decoded.pixels[7] == (channels == 4 ? 0 : 255));
		require(decoded.pixels[11] == (channels == 4 ? 223 : 255) && decoded.pixels[15] == (channels == 4 ? 224 : 255));
	}
	std::cout << "Scoped RLE image: RGB opacity and RGBA alpha/key policies passed\n";
}
int main(int argc, char **argv)
{
	if (argc == 3) {
		std::ifstream input(argv[1], std::ios::binary);
		require(bool(input));
		const std::vector<unsigned char> bytes{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
		const auto image = xl_compat::read_scoped_texture_image(bytes.data(), bytes.size());
		std::ofstream output(argv[2], std::ios::binary);
		output.write(reinterpret_cast<const char *>(image.pixels.data()), image.pixels.size());
		require(bool(output));
		return 0;
	}
	require(argc == 1);
	image_controls();
	using xl_compat::static_mission_texture_path;
	require(static_mission_texture_path("missions/single/boilpnt.hog", "ceil002") == "mods/boilpnt/textures/ceil002.tga");
	require(static_mission_texture_path("missions\\BOILPNT.HOG", "CEIL002") == "mods/BOILPNT/textures/CEIL002.tga");
	require(xl_compat::addon_mission_texture_path("missions/pmines.hog", "smokingfire") == "mods/pmines/textures/d2x-xl/smokingfire.tga");
	require(xl_compat::addon_mission_texture_path("archive.zip", "smokingfire").empty());
	require(xl_compat::addon_mission_texture_path("pmines.hog", "../smokingfire").empty());
	for (const char *name : { "", "..", "../ceil002", "x/ceil002", "x\\ceil002", "C:ceil002", "door#0", "bad\n" })
		require(static_mission_texture_path("boilpnt.hog", name).empty());
	for (const char *hog : { "", ".hog", "archive.zip", "..hog", "C:bad.hog" })
		require(static_mission_texture_path(hog, "ceil002").empty());
	require(static_mission_texture_path(nullptr, "ceil002").empty());
	require(static_mission_texture_path("boilpnt.hog", nullptr).empty());
	require(static_mission_texture_path(std::string(4096, 'a').c_str(), "ceil002").empty());
	require(static_mission_texture_path("boilpnt.hog", std::string(128, 'a').c_str()).empty());
	std::cout << "Static mission texture: selected HOG identity, exact TGA and bounded components passed\n";
}
