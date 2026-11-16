#include "pog_resources.h"
#include "../pog.h"
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <cstring>
#include "picosha2.h"
#include "asset_dependencies.h"
#include "scoped_texture_playback.h"
extern "C" {
#include "bm.h"
#include "u_mem.h"
}

struct xl_pog_resources {
	std::vector<xl_pog_bitmap> entries;
	std::vector<unsigned char> pixels;
	unsigned char *native_pixels = nullptr;
	uint64_t token = 0;
	bool scoped = false;
	std::vector<eclip> effect_definitions;
	std::vector<xl_pog_effect> effects;
	std::vector<std::vector<int>> effect_bitmaps;
	std::vector<vclip> clip_definitions;
	std::vector<xl_pog_clip> clips;
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

namespace
{
xl_pog_resources *pack_images(const std::vector<xl_compat::PogBitmap> &images, bool scoped_texture = false)
{
	auto result = std::make_unique<xl_pog_resources>();
	size_t total = 0;
	for (const auto &image : images) {
		if (image.pixels.size() > 2 * xl_compat::pog_file_limit - total)
			throw std::runtime_error("Decoded bitmap arena exceeds its resource bound");
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
		entry.rgb_opaque = image.rgb_opaque;
		entry.physics_height = scoped_texture ? image.height : image.frame_height();
		entry.scoped_texture = scoped_texture;
		for (unsigned word = 0; word < 4; ++word) {
			entry.transparent_frames[word] = image.transparent_frames[word];
			entry.super_frames[word] = image.super_frames[word];
		}
		result->entries.push_back(entry);
		result->pixels.insert(result->pixels.end(), image.pixels.begin(), image.pixels.end());
	}
	return result.release();
}
} // namespace

xl_pog_resources *xl_pog_prepare_rgba(const std::vector<xl_compat::PogBitmap> &images, unsigned max_bitmaps, bool scoped_texture)
{
	if (max_bitmaps < 2 || images.size() >= max_bitmaps)
		throw std::runtime_error("Decoded RGBA bitmap count exceeds its target bank");
	for (const auto &image : images) {
		if (!image.truecolor || !image.index || image.index >= max_bitmaps ||
		    (image.flags & (xl_compat::pog_rle | xl_compat::pog_rle_big)))
			throw std::runtime_error("Decoded RGBA bitmap has an invalid target or encoding");
		xl_compat::validate_pog_rgba(image);
		if (image.rgb_opaque) {
			if (image.flags & (xl_compat::pog_transparent | xl_compat::pog_super | xl_compat::pog_see_through))
				throw std::runtime_error("Opaque RGB bitmap declares transparency");
			for (std::size_t pixel = 3; pixel < image.pixels.size(); pixel += 4)
				if (image.pixels[pixel] != 255) throw std::runtime_error("Opaque RGB bitmap has nonopaque normalized pixels");
		}
	}
	return pack_images(images, scoped_texture);
}

xl_pog_resources *xl_pog_prepare_scoped(const xl_compat::ScopedTextureResources &textures,
                                        const xl_assets::ObjectDependencies &tables, const xl_pog_resources *pog)
{
	const auto playback = xl_compat::ScopedTexturePlayback::prepare(textures, tables);
	std::set<int> protected_indices;
	if (pog)
		for (const auto &entry : pog->entries) protected_indices.insert(int(entry.index));
	if (protected_indices != textures.pog_indices) throw std::runtime_error("Scoped texture plan has stale POG ownership");
	std::vector<xl_compat::PogBitmap> images;
	std::map<std::size_t, std::size_t> image_rows;
	for (const auto &binding : playback.bindings) {
		if (protected_indices.count(binding.bitmap)) throw std::runtime_error("Scoped texture conflicts with POG ownership");
		if (image_rows.emplace(binding.image, images.size()).second) {
			images.push_back(textures.images.at(binding.image).bitmap);
			images.back().index = binding.bitmap;
		}
	}
	std::unique_ptr<xl_pog_resources> result(xl_pog_prepare_rgba(images, tables.resource_banks->bitmap_count(), true));
	// Native aliases have independent display views but share one retained image
	const auto unique_entries = result->entries;
	result->entries.clear();
	for (const auto &binding : playback.bindings) {
		auto entry = unique_entries.at(image_rows.at(binding.image));
		entry.index = unsigned(binding.bitmap);
		result->entries.push_back(entry);
	}
	if (pog) {
		if (pog->pixels.size() > 2 * xl_compat::pog_file_limit - result->pixels.size())
			throw std::runtime_error("Combined scoped texture arena exceeds its resource bound");
		const auto offset = unsigned(result->pixels.size());
		for (auto entry : pog->entries) {
			entry.offset += offset;
			result->entries.push_back(entry);
		}
		result->pixels.insert(result->pixels.end(), pog->pixels.begin(), pog->pixels.end());
	}
	result->effect_definitions.assign(tables.effect_data, tables.effect_data + tables.effect_count);
	result->clip_definitions.assign(tables.clip_data, tables.clip_data + tables.clip_count);
	for (const auto &animation : playback.clips) {
		const auto &clip = tables.clip_data[animation.clip];
		result->clips.push_back({ animation.clip, animation.bitmaps.front(),
		                          int(textures.images[animation.image].bitmap.frames), clip.frame_time, clip.play_time });
	}
	for (const auto &clock : playback.effects) {
		result->effects.push_back({ clock.effect, clock.frame_time, tables.effect_data[clock.effect].changing_wall_texture,
		                            unsigned(clock.bitmaps.size()), textures.images[clock.image].bitmap.frames });
		result->effect_bitmaps.push_back(clock.bitmaps);
	}
	result->scoped = true;
	return result.release();
}

extern "C" int xl_pog_scoped_owner(const xl_pog_resources *resources)
{
	return resources && resources->scoped;
}
extern "C" int xl_pog_clip_count(const xl_pog_resources *resources)
{
	return resources ? int(resources->clips.size()) : 0;
}
extern "C" const xl_pog_clip *xl_pog_clip_entry(const xl_pog_resources *resources, int row)
{
	return resources && row >= 0 && std::size_t(row) < resources->clips.size() ? &resources->clips[row] : nullptr;
}
extern "C" int xl_pog_effect_count(const xl_pog_resources *resources)
{
	return resources ? int(resources->effects.size()) : 0;
}
extern "C" const xl_pog_effect *xl_pog_effect_entry(const xl_pog_resources *resources, int row)
{
	return resources && row >= 0 && std::size_t(row) < resources->effects.size() ? &resources->effects[row] : nullptr;
}
extern "C" int xl_pog_effect_bitmap(const xl_pog_resources *resources, int row, int frame)
{
	return resources && row >= 0 && std::size_t(row) < resources->effect_bitmaps.size() && frame >= 0 &&
	               std::size_t(frame) < resources->effect_bitmaps[row].size()
	           ? resources->effect_bitmaps[row][frame]
	           : -1;
}
extern "C" int xl_pog_scope_matches_live(const xl_pog_resources *resources)
{
	if (!resources || !resources->scoped) return 1;
	if (Num_vclips != int(resources->clip_definitions.size())) return 0;
	for (const auto &animation : resources->clips) {
		const auto &a = resources->clip_definitions[animation.clip], &b = Vclip[animation.clip];
		if (a.num_frames != b.num_frames || a.frame_time != b.frame_time || a.play_time != b.play_time || a.flags != b.flags) return 0;
		for (int frame = 0; frame < a.num_frames; ++frame)
			if (a.frames[frame].index != b.frames[frame].index) return 0;
	}
	if (Num_effects != int(resources->effect_definitions.size())) return 0;
	for (int id = 0; id < Num_effects; ++id) {
		const auto &a = resources->effect_definitions[id], &b = Effects[id];
		if (a.vc.num_frames != b.vc.num_frames || a.vc.frame_time != b.vc.frame_time ||
		    a.changing_wall_texture != b.changing_wall_texture || a.changing_object_texture != b.changing_object_texture || a.crit_clip != b.crit_clip) return 0;
		// Native bmread marks unused effect slots with -1, not an empty clock
		if (a.vc.num_frames == -1 && a.changing_wall_texture == -1 && a.changing_object_texture == -1) continue;
		if (a.vc.num_frames < 0 || a.vc.num_frames > VCLIP_MAX_FRAMES) return 0;
		for (int frame = 0; frame < a.vc.num_frames; ++frame)
			if (a.vc.frames[frame].index != b.vc.frames[frame].index) return 0;
	}
	return 1;
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
		return pack_images(images);
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
