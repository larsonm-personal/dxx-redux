#include "../pog.h"
#include <cstring>
#include <iostream>
#include <stdexcept>

static void require(bool value, const char *message)
{
	if (!value) throw std::runtime_error(message);
}
static void put32(std::vector<unsigned char> &data, std::size_t offset, unsigned value)
{
	for (unsigned i = 0; i < 4; ++i) data[offset + i] = static_cast<unsigned char>(value >> (8 * i));
}
static std::vector<unsigned char> image(unsigned width, unsigned height, unsigned flags,
                                        std::vector<unsigned char> pixels)
{
	std::vector<unsigned char> data(32);
	std::memcpy(data.data(), "DPOG", 4);
	put32(data, 4, 1);
	put32(data, 8, 1);
	data[12] = 1;
	data[23] = width & 255;
	data[24] = height & 255;
	data[25] = static_cast<unsigned char>((width >> 8) | ((height >> 8) << 4));
	data[26] = flags;
	data.insert(data.end(), pixels.begin(), pixels.end());
	return data;
}
static unsigned rejected;
static void reject(const std::vector<unsigned char> &data)
{
	try {
		xl_compat::read_pog(data.data(), data.size(), 2620);
	} catch (const std::runtime_error &) {
		++rejected;
		return;
	}
	throw std::runtime_error("invalid POG was accepted");
}

int main()
try {
	const auto rgba = image(2, 2, 128, { 5, 7, 9, 255, 120, 88, 128, 255, 99, 88, 77, 0, 1, 2, 3, 223 });
	const auto decoded = xl_compat::read_pog(rgba.data(), rgba.size(), 2620);
	const auto &bitmap = decoded.at(0);
	require(bitmap.truecolor && bitmap.width == 2 && bitmap.height == 2 && bitmap.frames == 1, "RGBA geometry");
	require(bitmap.pixels == std::vector<unsigned char>({ 5, 7, 9, 255, 120, 88, 128, 0, 0, 0, 0, 0, 1, 2, 3, 223 }), "RGBA key/alpha semantics");
	require(bitmap.transparency(0) == 0 && bitmap.transparency(1) == -1 && bitmap.transparency(2) == 1 &&
	            bitmap.transparency(3) == 0 && bitmap.transparency(4) == 0,
	        "RGBA gameplay transparency");
	for (std::size_t end = 0; end < rgba.size(); ++end)
		reject(std::vector<unsigned char>(rgba.begin(), rgba.begin() + end));
	for (unsigned position : { 0u, 4u, 8u, 12u, 13u, 23u, 24u, 28u }) {
		auto bad = rgba;
		bad[position] = position == 12 || position == 23 || position == 24 ? 0 : 255;
		reject(bad);
	}
	reject(image(1, 1, 128 | 8, { 1, 2, 3, 255 }));
	reject(image(1, 1, 128 | 32, { 1, 2, 3, 255 }));
	reject(image(3, 7, 128, std::vector<unsigned char>(84, 255)));
	reject(image(1, 129, 128, std::vector<unsigned char>(516, 255)));
	reject(image(512, 64, 128, {}));
	const auto large = image(512, 2, 128, std::vector<unsigned char>(512 * 512 * 8, 255));
	const auto strip = xl_compat::read_pog(large.data(), large.size(), 2620).at(0);
	require(strip.width == 512 && strip.height == 1024 && strip.frames == 2 && strip.frame_height() == 512,
	        "large-width source frame encoding");
	for (unsigned alpha : { 224u, 254u, 255u }) {
		const auto opaque = image(1, 1, 128, { 9, 8, 7, static_cast<unsigned char>(alpha) });
		const auto value = xl_compat::read_pog(opaque.data(), opaque.size(), 2620).at(0);
		require(value.pixels == std::vector<unsigned char>({ 9, 8, 7, 255 }) && value.flags == 128, "source RGB conversion threshold");
	}
	const auto plain = image(2, 2, 3, { 1, 254, 255, 7 });
	const auto indexed = xl_compat::read_pog(plain.data(), plain.size(), 2620).at(0);
	require(!indexed.truecolor && indexed.flags == 3 && indexed.pixels == std::vector<unsigned char>({ 1, 254, 255, 7 }), "indexed bytes changed");
	const auto rle = image(2, 2, 8, { 12, 0, 0, 0, 3, 3, 0xe2, 5, 0xe0, 0xe2, 8, 0xe0 });
	require(xl_compat::read_pog(rle.data(), rle.size(), 2620).at(0).pixels.size() == 12, "valid RLE rejected");
	auto bad_rle = rle;
	bad_rle[bad_rle.size() - 3] = 0xe3;
	reject(bad_rle);
	const auto big_rle = image(2, 1, 8 | 32, { 9, 0, 0, 0, 3, 0, 0xe2, 8, 0xe0 });
	require(xl_compat::read_pog(big_rle.data(), big_rle.size(), 2620).at(0).flags == 40, "valid RLE_BIG rejected");
	// Directory rows may share a payload; sequential replacement order is retained
	std::vector<unsigned char> shared(52, 0);
	std::memcpy(shared.data(), "DPOG", 4);
	put32(shared, 4, 1);
	put32(shared, 8, 2);
	shared[12] = shared[14] = 1;
	for (unsigned header : { 16u, 34u }) { shared[header + 9] = shared[header + 10] = 1; }
	shared.push_back(42);
	const auto copies = xl_compat::read_pog(shared.data(), shared.size(), 2620);
	require(copies.size() == 2 && copies[0].pixels[0] == 42 && copies[1].pixels[0] == 42, "shared indexed payload changed");
	std::cout << "POG RGBA/indexed/RLE resources, frame bounds and " << rejected << " invalid-input controls passed\n";
	return 0;
} catch (const std::exception &error) {
	std::cerr << error.what() << '\n';
	return 1;
}
