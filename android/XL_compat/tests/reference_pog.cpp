#include "../pog.h"
#include "../hog.h"
#include "../../app/src/main/cpp/shared/xl_rgba_pixels.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

// Storage/color adapters only. Pixel transforms, frame flags, dimensions and
// point transparency below execute the extracted source bodies
#define USE_OPENMP                0
#define MIN_OPACITY               224
#define BM_FLAG_TRANSPARENT       1
#define BM_FLAG_SUPER_TRANSPARENT 2
#define BM_FLAG_RLE               8
#define BM_FLAG_SEE_THRU          64
#define BM_FLAG_TGA               128
#define BM_FLAGS_TO_COPY          47
#define BM_TYPE_ALT               1
#define SUPER_TRANSP_COLOR        254
#define TRANSPARENCY_COLOR        255
#define ENTER(a, b)
#define RETVAL(x) return (x);
using fix = int32_t;
static int32_t X2I(int32_t x)
{
	return x >> 16;
}
static int32_t I2X(int32_t x)
{
	return x * 65536;
}
template <class T>
static void Swap(T &a, T &b)
{
	std::swap(a, b);
}

struct CRGBColor {
	uint8_t values[3];
	uint8_t &Red()
	{
		return values[0];
	}
	uint8_t &Green()
	{
		return values[1];
	}
	uint8_t &Blue()
	{
		return values[2];
	}
};
struct CRGBAColor {
	uint8_t values[4];
	uint8_t &Red()
	{
		return values[0];
	}
	uint8_t &Green()
	{
		return values[1];
	}
	uint8_t &Blue()
	{
		return values[2];
	}
	uint8_t &Alpha()
	{
		return values[3];
	}
};
struct CFloatVector {
	float values[4];
	float &Red()
	{
		return values[0];
	}
	float &Green()
	{
		return values[1];
	}
	float &Blue()
	{
		return values[2];
	}
	float &Alpha()
	{
		return values[3];
	}
};
struct CFile {
	const std::vector<uint8_t> &bytes;
	size_t offset = 0;
	size_t Read(void *out, size_t element, size_t count)
	{
		const size_t size = element * count;
		if (size > bytes.size() - offset) throw std::runtime_error("source read outside fixture");
		std::memcpy(out, bytes.data() + offset, size);
		offset += size;
		return count;
	}
};
struct CBitmap {
	int16_t width = 0, height = 0, row = 0;
	int bpp = 4, flags = 0;
	std::vector<uint8_t> pixels;
	std::array<int32_t, 4> transparent{}, super{};
	CRGBColor average{};
	int Width() const
	{
		return width;
	}
	int Height() const
	{
		return height;
	}
	int RowSize() const
	{
		return row;
	}
	void SetWidth(int16_t value)
	{
		width = value;
	}
	void SetHeight(int16_t value)
	{
		height = value;
	}
	void SetRowSize(int16_t value)
	{
		row = value;
	}
	int BPP() const
	{
		return bpp;
	}
	void SetBPP(int value)
	{
		bpp = value;
	}
	int Flags() const
	{
		return flags;
	}
	void AddFlags(int value)
	{
		flags |= value;
	}
	void DelFlags(int value)
	{
		flags &= ~value;
	}
	void SetTranspType(int) {}
	int32_t *TransparentFrames()
	{
		return transparent.data();
	}
	int32_t *SuperTranspFrames()
	{
		return super.data();
	}
	uint8_t *Buffer()
	{
		return pixels.empty() ? nullptr : pixels.data();
	}
	size_t Size() const
	{
		return pixels.size();
	}
	size_t Length() const
	{
		return pixels.size();
	}
	void Resize(uint32_t size)
	{
		pixels.resize(size);
	}
	int CreateBuffer()
	{
		pixels.resize(size_t(width) * height * bpp);
		return 1;
	}
	void Read(CFile &file)
	{
		file.Read(pixels.data(), 1, pixels.size());
	}
	void SetAvgColor(CRGBColor value)
	{
		average = value;
	}
	int Type() const
	{
		return BM_TYPE_ALT;
	}
	void *Frames() const
	{
		return nullptr;
	}
};
struct CTGA {
	CBitmap *m_pBm;
	struct Header {
		int Bits() const
		{
			return 32;
		}
	} m_header;
	void ConvertToRGB();
	void SetProperties(int32_t, int32_t, double, bool);
	int32_t ReadData(CFile &, int32_t, double, int32_t, int32_t);
};
static CBitmap *selected_bitmap;
static CBitmap *LoadFaceBitmap(int16_t, int16_t)
{
	return selected_bitmap;
}
static CBitmap *rle_expand_texture(CBitmap *)
{
	throw std::runtime_error("unexpected indexed source input");
}
#include "xl_reference_pog.h"

static unsigned comparisons;
static void require(bool value, const char *message)
{
	if (!value) throw std::runtime_error(message);
}
static void put32(std::vector<uint8_t> &bytes, size_t offset, uint32_t value)
{
	for (unsigned c = 0; c < 4; ++c) bytes[offset + c] = uint8_t(value >> (c * 8));
}
static std::vector<uint8_t> fixture(unsigned width, unsigned height, unsigned flags, const std::vector<uint8_t> &pixels)
{
	std::vector<uint8_t> bytes(32);
	std::memcpy(bytes.data(), "DPOG", 4);
	put32(bytes, 4, 1);
	put32(bytes, 8, 1);
	bytes[12] = 1;
	bytes[23] = width & 255;
	bytes[24] = height & 255;
	bytes[25] = uint8_t((width >> 8) | ((height >> 8) << 4));
	bytes[26] = uint8_t(flags);
	bytes.insert(bytes.end(), pixels.begin(), pixels.end());
	return bytes;
}

static void compare_entry(const xl_compat::PogBitmap &got, const uint8_t *header, const std::vector<uint8_t> &source)
{
	struct Header {
		uint8_t width, height, wh_extra, flags;
	} bmh[] = { { header[9], header[10], header[11], header[12] } };
	CBitmap bm;
	int i = 0, bHaveTGA;
#include "xl_reference_pog_dimensions.h"
	require(got.width == unsigned(bm.Width()) && got.height == unsigned(bm.Height()), "source POG dimensions differ");
	CFile input{ source };
	CTGA tga{ &bm };
	require(tga.ReadData(input, -1, 1.0, 0, 1) == 1, "source POG read failed");
	require(got.flags == unsigned(bm.Flags()), "source POG flags differ");
	for (unsigned c = 0; c < 3; ++c)
		require(got.average_rgb[c] == bm.average.values[c], "source POG average color differs");
	for (unsigned frame = 0; frame < 4; ++frame)
		require(got.transparent_frames[frame] == uint32_t(bm.transparent[frame]) &&
		            got.super_frames[frame] == uint32_t(bm.super[frame]),
		        "source POG frame flags differ");
	for (size_t pixel = 0; pixel < got.pixels.size() / 4; ++pixel) {
		for (unsigned channel = 0; channel < 4; ++channel) {
			const auto expected = channel == 3 && bm.BPP() == 3 ? 255 : bm.pixels[pixel * bm.BPP() + channel];
			require(got.pixels[pixel * 4 + channel] == expected, "source POG pixel conversion differs");
		}
	}
	selected_bitmap = &bm;
	// Positive UVs keep multiplication within the source's defined signed range
	for (int orient = 0; orient < 4; ++orient)
		for (int y = 0; y < 7; ++y)
			for (int x = 0; x < 7; ++x) {
				const fix u = x * 8192, v = y * 8192;
				const int uu = orient == 1 ? 65536 - v : orient == 2 ? 65536 - u
				                                     : orient == 3   ? v
				                                                     : u;
				const int vv = orient == 1 ? u : orient == 2 ? 65536 - v
				                             : orient == 3   ? 65536 - u
				                                             : v;
				const auto px = dxx_xl_texture_coordinate(uu, got.width);
				const auto py = dxx_xl_texture_coordinate(vv, got.height);
				require(dxx_xl_rgba_transparency(got.pixels.data() + (size_t(py) * got.width + px) * 4) == PixelTranspType(0, int16_t(orient), 0, u, v),
				        "source POG point transparency differs");
				++comparisons;
			}
	++comparisons;
}

static unsigned compare(const std::vector<uint8_t> &file)
{
	const auto production = xl_compat::read_pog(file.data(), file.size(), 65536);
	const size_t payload = 12 + production.size() * 20;
	unsigned count = 0;
	for (size_t i = 0; i < production.size(); ++i) {
		if (!production[i].truecolor) continue;
		const auto *header = file.data() + 12 + production.size() * 2 + i * 18;
		uint32_t offset = 0;
		for (unsigned c = 0; c < 4; ++c) offset |= uint32_t(header[14 + c]) << (8 * c);
		const auto start = file.begin() + payload + offset;
		compare_entry(production[i], header, std::vector<uint8_t>(start, start + production[i].pixels.size()));
		++count;
	}
	return count;
}

static void original(const char *path)
{
	std::ifstream input(path, std::ios::binary | std::ios::ate);
	require(bool(input), "cannot open original HOG");
	const auto length = input.tellg();
	const auto read = [&](uint64_t offset, unsigned char *out, size_t size) {
		input.seekg(static_cast<std::streamoff>(offset));
		input.read(reinterpret_cast<char *>(out), static_cast<std::streamsize>(size));
		require(bool(input), "original HOG read failed");
	};
	unsigned resources = 0, images = 0;
	for (const auto &entry : xl_compat::index_hog(static_cast<uint64_t>(length), read)) {
		const auto key = xl_compat::hog_key(entry.name);
		if (key.size() < 4 || key.substr(key.size() - 4) != ".pog") continue;
		require(entry.size <= xl_compat::pog_file_limit, "original POG exceeds bound");
		std::vector<uint8_t> bytes(static_cast<size_t>(entry.size));
		read(entry.offset, bytes.data(), bytes.size());
		images += compare(bytes);
		++resources;
	}
	require(resources && images, "original has no true-color POG resources");
	std::cout << "Original POG resources " << resources << ", source-matched RGBA replacements " << images << '\n';
}

int main(int argc, char **argv)
try {
	for (const auto shape : { std::pair{ 4u, 4u }, { 4u, 12u }, { 32u, 32u }, { 257u, 1u }, { 512u, 2u } }) {
		const auto height = shape.first > 256 ? shape.first * shape.second : shape.second;
		for (unsigned alpha : { 0u, 1u, 5u, 6u, 223u, 224u, 254u, 255u }) {
			std::vector<uint8_t> pixels(size_t(shape.first) * height * 4);
			for (size_t i = 0; i < pixels.size(); i += 4) {
				pixels[i] = uint8_t(i * 17 + 9);
				pixels[i + 1] = uint8_t(i * 11 + 51);
				pixels[i + 2] = uint8_t(i * 3 + 72);
				pixels[i + 3] = uint8_t(i % 12 ? alpha : 255);
				if (i % 28 == 4) {
					pixels[i] = 120;
					pixels[i + 1] = 88;
					pixels[i + 2] = 128;
				}
			}
			compare(fixture(shape.first, shape.second, 128 | 4, pixels));
			for (size_t i = 0; i < pixels.size(); i += 4) {
				pixels[i] = 25;
				pixels[i + 1] = 50;
				pixels[i + 2] = 75;
				pixels[i + 3] = uint8_t(i % 12 ? alpha : 255);
			}
			compare(fixture(shape.first, shape.second, 128 | 4, pixels));
		}
	}
	for (int i = 1; i < argc; ++i) original(argv[i]);
	std::cout << "Pinned POG dimensions, pixels, flags and point transparency: " << comparisons << " controls\n";
	return 0;
} catch (const std::exception &error) {
	std::cerr << error.what() << '\n';
	return 1;
}
