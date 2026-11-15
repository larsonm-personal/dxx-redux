#include "pog_resources.h"
#include "../pog.h"
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <cstring>
#include "asset_dependencies.h"
extern "C" {
#include "bm.h"
#include "u_mem.h"
}

struct xl_pog_resources {
	std::vector<xl_pog_bitmap> entries;
	std::vector<unsigned char> pixels;
	unsigned char *native_pixels = nullptr;
	uint64_t token = 0;
	xl_pog_resources()
	{
		static uint64_t next_token;
		if (next_token == UINT64_MAX) throw std::runtime_error("POG owner identity exhausted");
		token = ++next_token;
	}
	~xl_pog_resources()
	{
		if (native_pixels) d_free(native_pixels);
	}
};

extern "C" uint64_t xl_pog_owner_token(const xl_pog_resources *resources)
{
	return resources ? resources->token : 0;
}

extern "C" int xl_pog_prepare_storage(xl_pog_resources *resources)
{
	if (!resources || resources->native_pixels) return 1;
	resources->native_pixels = static_cast<unsigned char *>(d_malloc(resources->pixels.empty() ? 1 : resources->pixels.size()));
	if (!resources->native_pixels) return 0;
	if (!resources->pixels.empty()) std::memcpy(resources->native_pixels, resources->pixels.data(), resources->pixels.size());
	return 1;
}

extern "C" unsigned char *xl_pog_take_storage(xl_pog_resources *resources, size_t *size)
{
	if (size) *size = resources ? resources->pixels.size() : 0;
	if (!resources) return nullptr;
	auto *pixels = resources->native_pixels;
	resources->native_pixels = nullptr;
	return pixels;
}

extern "C" xl_pog_resources *xl_pog_prepare(PHYSFS_file *file, unsigned max_bitmaps, char *error, size_t error_size)
{
	try {
		if (error && error_size) error[0] = 0;
		if (!file) throw std::runtime_error("Missing POG source");
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
			                             xl_compat::pog_rle | xl_compat::pog_rle_big | xl_compat::pog_see_through);
			entry.average_index = image.average_index;
			for (unsigned c = 0; c < 3; ++c) entry.average_rgb[c] = image.average_rgb[c];
			entry.rgba = image.truecolor;
			entry.palette_flags = image.palette_flags;
			entry.offset = unsigned(result->pixels.size());
			entry.size = unsigned(image.pixels.size());
			entry.frames = image.frames;
			entry.frame_height = image.frame_height();
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

namespace
{
struct PogClockTables {
	int effect_count = Num_effects, wall_clip_count = Num_wall_anims, clip_count = Num_vclips;
	int texture_count = NumTextures, object_bitmap_count = N_ObjBitmaps;
	const eclip *effect_data = Effects;
	const wclip *wall_clip_data = WallAnims;
	const vclip *clip_data = Vclip;
	const bitmap_index *texture_data = Textures, *object_bitmaps = ObjBitmaps;
	PogClockTables() = default;
	explicit PogClockTables(const xl_assets::ObjectDependencies &tables)
	    : effect_count(tables.effect_count), wall_clip_count(tables.wall_clip_count), clip_count(tables.clip_count),
	      texture_count(tables.texture_count), object_bitmap_count(tables.object_bitmap_count),
	      effect_data(tables.effect_data), wall_clip_data(tables.wall_clip_data), clip_data(tables.clip_data),
	      texture_data(tables.texture_data), object_bitmaps(tables.object_bitmaps)
	{
	}
};

int strip_clock(unsigned bitmap, const PogClockTables &tables)
{
	int i, frame;
	if (tables.effect_count < 0 || tables.effect_count > MAX_EFFECTS || tables.wall_clip_count < 0 || tables.wall_clip_count > MAX_WALL_ANIMS ||
	    tables.clip_count < 0 || tables.clip_count > VCLIP_MAXNUM || tables.texture_count < 0 || tables.texture_count > MAX_TEXTURES ||
	    tables.object_bitmap_count < 0 || tables.object_bitmap_count > MAX_OBJ_BITMAPS) return -1;
	for (i = 0; i < tables.effect_count; ++i) {
		const eclip *effect = &tables.effect_data[i];
		const int wall = effect->changing_wall_texture, object = effect->changing_object_texture;
		if (wall == -1 && object == -1) continue;
		if (effect->vc.num_frames < 1 || effect->vc.num_frames > VCLIP_MAX_FRAMES) return -1;
		for (frame = 0; frame < effect->vc.num_frames; ++frame)
			if (effect->vc.frames[frame].index == bitmap) return 1;
		if (wall >= 0 && wall < tables.texture_count && tables.texture_data[wall].index == bitmap) return 1;
		if (object >= 0 && object < tables.object_bitmap_count && tables.object_bitmaps[object].index == bitmap) return 1;
	}
	for (i = 0; i < tables.wall_clip_count; ++i) {
		const wclip *clip = &tables.wall_clip_data[i];
		if (clip->num_frames <= 0) continue;
		if (clip->num_frames > MAX_CLIP_FRAMES) return -1;
		for (frame = 0; frame < clip->num_frames; ++frame) {
			const int texture = clip->frames[frame];
			if (texture >= 0 && texture < tables.texture_count && tables.texture_data[texture].index == bitmap) return 1;
		}
	}
	for (i = 0; i < tables.clip_count; ++i) {
		const vclip *clip = &tables.clip_data[i];
		if (clip->num_frames <= 0) continue;
		if (clip->num_frames > VCLIP_MAX_FRAMES) return -1;
		for (frame = 0; frame < clip->num_frames; ++frame)
			if (clip->frames[frame].index == bitmap) return 1;
	}
	return 0;
}
} // namespace

int xl_pog_target_strip_clock(unsigned bitmap, const xl_assets::ObjectDependencies &tables)
{
	return strip_clock(bitmap, PogClockTables(tables));
}

extern "C" int xl_pog_strip_clock(unsigned bitmap)
{
	return strip_clock(bitmap, PogClockTables{});
}
