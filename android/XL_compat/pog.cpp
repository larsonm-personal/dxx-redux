#include "pog.h"
#include "../app/src/main/cpp/shared/bounded_rle.h"
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>

namespace xl_compat
{
namespace
{
constexpr unsigned copied_flags = pog_transparent | pog_super | pog_no_lighting | pog_rle | pog_rle_big;

unsigned u16(const unsigned char *p)
{
	return unsigned(p[0]) | unsigned(p[1]) << 8;
}

std::uint32_t u32(const unsigned char *p)
{
	return std::uint32_t(p[0]) | std::uint32_t(p[1]) << 8 | std::uint32_t(p[2]) << 16 | std::uint32_t(p[3]) << 24;
}

bool span(std::size_t size, std::size_t offset, std::size_t count)
{
	return offset <= size && count <= size - offset;
}

[[noreturn]] void invalid(unsigned row, const char *reason)
{
	throw std::runtime_error("POG bitmap " + std::to_string(row) + ": " + reason);
}
} // namespace

void prepare_pog_rgba(PogBitmap &bitmap)
{
	if (!bitmap.width || bitmap.width > 4096 || !bitmap.height || bitmap.height > 32767 || bitmap.frames > 128 || !bitmap.frames ||
	    bitmap.frames != (bitmap.height / bitmap.width ? bitmap.height / bitmap.width : 1) ||
	    (bitmap.frames > 1 && bitmap.height != bitmap.width * bitmap.frames) ||
	    bitmap.pixels.size() != std::size_t(bitmap.width) * bitmap.height * 4)
		throw std::runtime_error("POG RGBA dimensions or storage are invalid");
	bitmap.flags = (bitmap.source_flags & pog_no_lighting) | pog_rgba;
	bitmap.transparent_frames.fill(0);
	bitmap.super_frames.fill(0);
	unsigned alpha_count = 0;
	float alpha_sum = 0, visible = 0;
	std::array<float, 3> color{};
	const unsigned threshold = bitmap.width * bitmap.width / 1000;
	const auto frame_pixels = std::size_t(bitmap.width) * bitmap.frame_height();
	// CTGA::SetProperties accumulates nAlpha across frames, but counts super
	// pixels per frame. Preserve that distinction, including the strict threshold
	for (unsigned frame = 0; frame < bitmap.frames; ++frame) {
		unsigned super_count = 0;
		for (std::size_t i = 0; i < frame_pixels; ++i) {
			auto *pixel = bitmap.pixels.data() + (frame * frame_pixels + i) * 4;
			if (pixel[0] == 120 && pixel[1] == 88 && pixel[2] == 128) {
				++super_count;
				pixel[3] = 0;
			} else if (!pixel[3]) {
				pixel[0] = pixel[1] = pixel[2] = 0;
			}
			if (pixel[3] < 224) {
				alpha_sum += pixel[3];
				++alpha_count;
			}
			const float alpha = float(pixel[3]) / 255.0f;
			visible += alpha;
			for (unsigned c = 0; c < 3; ++c) color[c] += float(pixel[c]) * alpha;
		}
		if (alpha_count > threshold) {
			if (!frame) {
				bitmap.flags |= pog_transparent;
				if (alpha_sum / float(alpha_count) <= 5.0f) bitmap.flags |= pog_see_through;
			}
			bitmap.transparent_frames[frame / 32] |= std::uint32_t(1) << (frame % 32);
		}
		if (super_count > threshold) {
			if (!frame) bitmap.flags |= pog_super;
			bitmap.super_frames[frame / 32] |= std::uint32_t(1) << (frame % 32);
		}
	}
	// The source converts to RGB when no pixel is below MIN_OPACITY. Keeping
	// four channels with opaque alpha preserves the resulting pixel semantics
	if (!alpha_count)
		for (std::size_t i = 3; i < bitmap.pixels.size(); i += 4) bitmap.pixels[i] = 255;
	for (unsigned c = 0; c < 3; ++c)
		bitmap.average_rgb[c] = visible > 0 ? static_cast<unsigned char>(color[c] / visible) : 0;
}

int PogBitmap::transparency(std::size_t pixel) const
{
	if (!truecolor || pixel >= pixels.size() / 4) return 0;
	const auto *p = pixels.data() + pixel * 4;
	if (p[0] == 120 && p[1] == 88 && p[2] == 128) return -1;
	return p[3] == 0 ? 1 : 0;
}

std::vector<PogBitmap> read_pog(const unsigned char *data, std::size_t size, unsigned max_bitmaps)
{
	if (!data || size < 12 || size > pog_file_limit || max_bitmaps < 2 ||
	    std::memcmp(data, "DPOG", 4) || u32(data + 4) != 1)
		throw std::runtime_error("POG header or file size is invalid");
	const auto count = u32(data + 8);
	if (count >= max_bitmaps || count > (size - 12) / 20)
		throw std::runtime_error("POG bitmap directory is invalid");
	const std::size_t headers = 12 + std::size_t(count) * 2;
	const std::size_t payload = headers + std::size_t(count) * 18;
	std::size_t total = 0;
	std::vector<PogBitmap> result;
	result.reserve(count);
	for (unsigned i = 0; i < count; ++i) {
		const auto *header = data + headers + std::size_t(i) * 18;
		PogBitmap bitmap;
		bitmap.index = u16(data + 12 + std::size_t(i) * 2);
		if (!bitmap.index || bitmap.index >= max_bitmaps) invalid(i, "index is outside the bitmap registry");
		bitmap.width = header[9] | unsigned(header[11] & 15) << 8;
		bitmap.source_flags = header[12];
		bitmap.flags = header[12] & copied_flags;
		bitmap.average_index = header[13];
		bitmap.truecolor = (header[12] & pog_rgba) != 0;
		bitmap.height = bitmap.truecolor && bitmap.width > 256 ? bitmap.width * header[10]
		                                                       : header[10] | unsigned(header[11] >> 4) << 8;
		if (!bitmap.width || !bitmap.height) invalid(i, "dimensions are empty");
		if (bitmap.height > 32767) invalid(i, "height exceeds the signed source bitmap range");
		const auto offset = u32(header + 14);
		if (offset > size - payload) invalid(i, "pixel offset is outside the file");
		const std::size_t start = payload + offset;
		std::size_t bytes = std::size_t(bitmap.width) * bitmap.height;
		if (bitmap.truecolor) {
			if (bitmap.flags & (pog_rle | pog_rle_big)) invalid(i, "RGBA pixels cannot use indexed RLE flags");
			bitmap.frames = bitmap.height / bitmap.width;
			if (!bitmap.frames) bitmap.frames = 1;
			if (bitmap.frames > 128 || (bitmap.frames > 1 && bitmap.height % bitmap.width))
				invalid(i, "RGBA animation requires complete frames within the source's 128-frame masks");
			if (bytes > pog_file_limit / 4) invalid(i, "RGBA pixel storage exceeds the resource bound");
			bytes *= 4;
		} else if (bitmap.flags & pog_rle) {
			if (!span(size, start, 4)) invalid(i, "RLE size is missing");
			bytes = u32(data + start);
			if (bytes > pog_file_limit || !span(size, start, bytes) ||
			    !bounded_rle_validate_bitmap(data + start, bytes, int(bitmap.width), int(bitmap.height),
			                                 bitmap.flags & pog_rle_big ? 2 : 1))
				invalid(i, "indexed RLE rows are invalid");
		}
		if (!span(size, start, bytes)) invalid(i, "pixel data is truncated");
		if (bytes > 2 * pog_file_limit - total) invalid(i, "total decoded storage exceeds the resource bound");
		total += bytes;
		bitmap.pixels.assign(data + start, data + start + bytes);
		if (bitmap.truecolor) prepare_pog_rgba(bitmap);
		result.push_back(std::move(bitmap));
	}
	return result;
}
} // namespace xl_compat
