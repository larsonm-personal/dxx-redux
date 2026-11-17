#ifndef DXX_XL_SCOPED_TEXTURE_IMAGE_H
#define DXX_XL_SCOPED_TEXTURE_IMAGE_H
#include "particle_image.h"
#include <algorithm>

namespace xl_compat
{
// Explicit compatibility extension for authored scoped textures. The pinned
// CTGA raw reader does not decode type 10; ordinary particle images keep its policy
inline PogBitmap read_scoped_texture_image(const unsigned char *data, std::size_t size)
{
	if (!data || size < 18 || data[2] != 10) return read_particle_image(data, size);
	const auto word = [&](unsigned at) { return unsigned(data[at]) | unsigned(data[at + 1]) << 8; };
	const unsigned width = word(12), height = word(14), channels = data[16] / 8;
	if (size > pog_file_limit + 65536 || data[1] || (channels != 3 && channels != 4) || data[16] != channels * 8 ||
	    !width || width > 4096 || !height || height > 32767 || data[0] > 127 || word(8) || word(10) ||
	    (data[17] & 0xc0) || ((data[17] & 15) != 0 && (channels != 4 || (data[17] & 15) != 8)))
		throw std::runtime_error("XL scoped RLE TGA has an unsupported header");
	const std::size_t pixels = std::size_t(width) * height;
	std::size_t cursor = 18u + data[0];
	if (pixels > pog_file_limit / 4 || cursor > size)
		throw std::runtime_error("XL scoped RLE TGA is oversized or truncated");
	const unsigned frames = height / width ? height / width : 1;
	if (frames > 128 || (frames > 1 && height != width * frames))
		throw std::runtime_error("XL scoped RLE TGA has an invalid frame layout");
	// Decode standard TGA orientation to top-left RGB(A), then apply the existing
	// owned bitmap's alpha/key/average policy via its explicit source-order marker
	std::vector<unsigned char> raw(18 + pixels * channels, 0);
	raw[2] = 2;
	raw[10] = 1;
	std::copy(data + 12, data + 17, raw.begin() + 12);
	for (std::size_t decoded = 0; decoded < pixels;) {
		if (cursor == size) throw std::runtime_error("XL scoped RLE TGA is missing a packet");
		const unsigned packet = data[cursor++], count = (packet & 127) + 1;
		const std::size_t payload = std::size_t(packet & 128 ? 1 : count) * channels;
		if (count > pixels - decoded || payload > size - cursor || ((packet & 128) && count > width - decoded % width))
			throw std::runtime_error("XL scoped RLE TGA packet exceeds its pixel or input bounds");
		for (unsigned i = 0; i < count; ++i) {
			const auto ordinal = decoded + i;
			const unsigned source_x = unsigned(ordinal % width), source_y = unsigned(ordinal / width);
			const unsigned x = data[17] & 16 ? width - source_x - 1 : source_x;
			const unsigned y = data[17] & 32 ? source_y : height - source_y - 1;
			const auto *input = data + cursor + (packet & 128 ? 0 : i * channels);
			auto *output = raw.data() + 18 + (std::size_t(y) * width + x) * channels;
			output[0] = input[2];
			output[1] = input[1];
			output[2] = input[0];
			if (channels == 4) output[3] = input[3];
		}
		cursor += payload;
		decoded += count;
	}
	return read_particle_image(raw.data(), raw.size());
}
} // namespace xl_compat
#endif
