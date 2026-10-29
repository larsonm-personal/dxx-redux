#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace xl_compat
{
// Serialized bitmap flags are shared with the native engine and pinned XL
inline constexpr unsigned pog_transparent = 1, pog_super = 2, pog_no_lighting = 4;
inline constexpr unsigned pog_rle = 8, pog_rle_big = 32, pog_see_through = 64, pog_rgba = 128;
inline constexpr std::size_t pog_file_limit = 64u * 1024u * 1024u;

struct PogBitmap {
	unsigned index = 0, width = 0, height = 0, frames = 1;
	unsigned flags = 0, source_flags = 0, average_index = 0;
	std::array<std::uint32_t, 4> transparent_frames{}, super_frames{};
	std::array<unsigned char, 3> average_rgb{};
	bool truecolor = false;
	// Indexed records retain their validated encoded bytes. True-color records
	// retain RGBA in source row/channel order, with XL's alpha/key normalization
	std::vector<unsigned char> pixels;

	unsigned frame_height() const
	{
		return frames > 1 ? width : height;
	}
	int transparency(std::size_t pixel) const;
};

std::vector<PogBitmap> read_pog(const unsigned char *data, std::size_t size, unsigned max_bitmaps);
void prepare_pog_rgba(PogBitmap &bitmap);
} // namespace xl_compat
