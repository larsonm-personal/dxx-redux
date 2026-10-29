#include "pog_resources.h"
#include "../pog.h"
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

struct xl_pog_resources {
	std::vector<xl_pog_bitmap> entries;
	std::vector<unsigned char> pixels;
};

extern "C" xl_pog_resources *xl_pog_prepare(PHYSFS_file *file, unsigned max_bitmaps, char *error, size_t error_size)
{
	try {
		if (error && error_size) error[0] = 0;
		const auto size = PHYSFS_fileLength(file);
		if (size < 12 || size > PHYSFS_sint64(xl_compat::pog_file_limit) || !PHYSFS_seek(file, 0))
			throw std::runtime_error("POG file size or seek is invalid");
		std::vector<unsigned char> bytes(static_cast<size_t>(size));
		if (PHYSFS_readBytes(file, bytes.data(), bytes.size()) != size)
			throw std::runtime_error("POG file is truncated");
		auto images = xl_compat::read_pog(bytes.data(), bytes.size(), max_bitmaps);
		auto result = std::make_unique<xl_pog_resources>();
		size_t total = 0;
		for (const auto &image : images) {
			if (image.truecolor && image.frames > 1)
				throw std::runtime_error("POG bitmap " + std::to_string(image.index) + " requires " +
				                         std::to_string(image.frames) + " true-color animation frames; their runtime animation is not implemented");
			total += image.pixels.size();
		}
		result->pixels.reserve(total);
		result->entries.reserve(images.size());
		for (const auto &image : images) {
			xl_pog_bitmap entry{};
			entry.index = image.index;
			entry.width = image.width;
			entry.height = image.height;
			entry.flags = image.flags & (xl_compat::pog_transparent | xl_compat::pog_super | xl_compat::pog_no_lighting |
			                             xl_compat::pog_rle | xl_compat::pog_rle_big);
			entry.average_index = image.average_index;
			for (unsigned c = 0; c < 3; ++c) entry.average_rgb[c] = image.average_rgb[c];
			entry.rgba = image.truecolor;
			entry.offset = unsigned(result->pixels.size());
			entry.size = unsigned(image.pixels.size());
			result->entries.push_back(entry);
			result->pixels.insert(result->pixels.end(), image.pixels.begin(), image.pixels.end());
		}
		return result.release();
	} catch (const std::exception &exception) {
		if (error && error_size) std::snprintf(error, error_size, "%s", exception.what());
		return nullptr;
	}
}

extern "C" void xl_pog_free(xl_pog_resources *resources)
{
	delete resources;
}

extern "C" int xl_pog_count(const xl_pog_resources *resources)
{
	return resources ? int(resources->entries.size()) : 0;
}

extern "C" const xl_pog_bitmap *xl_pog_entry(const xl_pog_resources *resources, int row)
{
	return resources && row >= 0 && size_t(row) < resources->entries.size() ? &resources->entries[row] : nullptr;
}

extern "C" const unsigned char *xl_pog_pixels(const xl_pog_resources *resources, size_t *size)
{
	if (size) *size = resources ? resources->pixels.size() : 0;
	return resources ? resources->pixels.data() : nullptr;
}
