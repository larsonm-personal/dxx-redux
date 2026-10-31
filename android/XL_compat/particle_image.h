#ifndef DXX_XL_PARTICLE_IMAGE_H
#define DXX_XL_PARTICLE_IMAGE_H
#include "pog.h"
#include <stdexcept>

namespace xl_compat
{
inline constexpr char fire_image_name[] = "smokingfire";
inline constexpr unsigned fire_image_atlas_side = 2;
inline constexpr char smoke_image_name[] = "smoke";
inline constexpr unsigned smoke_image_atlas_side = 8;
// Authored addon images use XL's raw CTGA path. Reuse the existing owned RGBA
// bitmap and alpha/key normalization rather than adding a second pixel policy
inline PogBitmap read_particle_image(const unsigned char *data, std::size_t size)
{
	if (!data || size < 18 || size > pog_file_limit + 65536)
		throw std::runtime_error("XL particle image has an invalid TGA file size");
	const auto word = [&](unsigned offset) { return unsigned(data[offset]) | unsigned(data[offset + 1]) << 8; };
	const unsigned width = word(12), height = word(14), channels = data[16] / 8;
	if (data[1] || data[2] != 2 || (data[16] != 24 && data[16] != 32))
		throw std::runtime_error("XL particle image requires an uncompressed 24-bit or 32-bit true-color TGA");
	if (!width || width > 4096 || !height || height > 32767 || word(8) > width || word(10) > height || data[0] > 127)
		throw std::runtime_error("XL particle image exceeds the source TGA dimensions or identifier range");
	const std::size_t pixels = std::size_t(width) * height, offset = 18u + data[0];
	if (pixels > pog_file_limit / 4 || offset > size || pixels * channels > size - offset)
		throw std::runtime_error("XL particle image pixel payload is oversized or truncated");
	PogBitmap result;
	result.width = width;
	result.height = height;
	result.frames = height / width ? height / width : 1;
	if (result.frames > 128 || (result.frames > 1 && height != width * result.frames))
		throw std::runtime_error("XL particle image has an invalid source frame layout");
	result.truecolor = true;
	result.flags = pog_rgba;
	result.pixels.resize(pixels * 4);
	// CTGA::Load selects raw RGB/order for a nonzero yStart marker. Otherwise
	// ReadData reverses rows and SetProperties swaps B/R; descriptor is unused
	const bool source_order = word(10) != 0;
	for (unsigned y = 0; y < height; ++y)
		for (unsigned x = 0; x < width; ++x) {
			const auto *input = data + offset + (std::size_t(y) * width + x) * channels;
			auto *output = result.pixels.data() + (std::size_t(source_order ? y : height - y - 1) * width + x) * 4;
			output[0] = input[source_order ? 0 : 2];
			output[1] = input[1];
			output[2] = input[source_order ? 2 : 0];
			output[3] = channels == 4 ? input[3] : 255;
		}
	if (channels == 4) prepare_pog_rgba(result);
	else {
		// The 24-bit source path has no alpha or super-transparency key treatment
		std::array<float, 3> sum{};
		unsigned visible = 0;
		for (std::size_t i = 0; i < result.pixels.size(); i += 4)
			if (result.pixels[i] || result.pixels[i + 1] || result.pixels[i + 2]) {
				++visible;
				for (unsigned c = 0; c < 3; ++c) sum[c] += result.pixels[i + c];
			}
		for (unsigned c = 0; c < 3; ++c) result.average_rgb[c] = visible ? static_cast<unsigned char>(sum[c] / float(visible)) : 0;
	}
	return result;
}
} // namespace xl_compat
#endif
