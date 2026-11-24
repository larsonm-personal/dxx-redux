#include "pog_resources.h"
#include "../pog.h"
#include <cstdio>
#include <algorithm>
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
	void *native_side_views = nullptr;
	uint64_t token = 0;
	bool scoped = false;
	std::vector<eclip> effect_definitions;
	std::vector<xl_pog_effect> effects;
	std::vector<std::vector<int>> effect_bitmaps;
	std::vector<vclip> clip_definitions;
	std::vector<xl_pog_clip> clips;
	std::vector<wclip> door_definitions;
	std::vector<bitmap_index> door_textures;
	std::vector<std::pair<int, int>> doors;
	xl_pog_resources()
	{
		static uint64_t next_token;
		if (next_token == UINT64_MAX) throw std::runtime_error("POG owner identity exhausted");
		token = ++next_token;
	}
	~xl_pog_resources()
	{
		xl_pog_free_side_views(native_side_views);
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
	for (const auto &entry : resources->entries)
		if (entry.side_frames) {
			resources->native_side_views = xl_pog_prepare_side_views(resources);
			if (!resources->native_side_views) {
				d_free(resources->native_pixels);
				resources->native_pixels = nullptr;
				return 0;
			}
			break;
		}
	return 1;
}

extern "C" void *xl_pog_take_side_views(xl_pog_resources *resources)
{
	if (!resources) return nullptr;
	auto *views = resources->native_side_views;
	resources->native_side_views = nullptr;
	return views;
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
void append_packed_image(xl_pog_resources &result, const xl_compat::PogBitmap &image, unsigned index, bool scoped_texture)
{
	xl_pog_bitmap entry{};
	entry.index = index;
	entry.width = image.width;
	entry.height = image.height;
	entry.flags = image.flags & (xl_compat::pog_transparent | xl_compat::pog_super | xl_compat::pog_no_lighting |
	                             xl_compat::pog_rle | xl_compat::pog_rle_big | xl_compat::pog_see_through);
	entry.average_index = image.average_index;
	for (unsigned c = 0; c < 3; ++c) entry.average_rgb[c] = image.average_rgb[c];
	entry.rgba = image.truecolor;
	entry.palette_flags = image.palette_flags;
	entry.offset = unsigned(result.pixels.size());
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
	result.entries.push_back(entry);
	result.pixels.insert(result.pixels.end(), image.pixels.begin(), image.pixels.end());
}

size_t add_pixel_size(size_t total, size_t bytes)
{
	if (bytes > 2 * xl_compat::pog_file_limit - total)
		throw std::runtime_error("Decoded bitmap arena exceeds its resource bound");
	return total + bytes;
}

void validate_rgba_target(const xl_compat::PogBitmap &image, unsigned index, unsigned max_bitmaps)
{
	if (!image.truecolor || !index || index >= max_bitmaps ||
	    (image.flags & (xl_compat::pog_rle | xl_compat::pog_rle_big)))
		throw std::runtime_error("Decoded RGBA bitmap has an invalid target or encoding");
	xl_compat::validate_pog_rgba(image);
	if (!image.rgb_opaque) return;
	if (image.flags & (xl_compat::pog_transparent | xl_compat::pog_super | xl_compat::pog_see_through))
		throw std::runtime_error("Opaque RGB bitmap declares transparency");
	for (std::size_t pixel = 3; pixel < image.pixels.size(); pixel += 4)
		if (image.pixels[pixel] != 255) throw std::runtime_error("Opaque RGB bitmap has nonopaque normalized pixels");
}

xl_pog_resources *pack_images(const std::vector<xl_compat::PogBitmap> &images, bool scoped_texture = false)
{
	auto result = std::make_unique<xl_pog_resources>();
	size_t total = 0;
	for (const auto &image : images) total = add_pixel_size(total, image.pixels.size());
	result->pixels.reserve(total);
	result->entries.reserve(images.size());
	for (const auto &image : images) append_packed_image(*result, image, image.index, scoped_texture);
	return result.release();
}
} // namespace

xl_pog_resources *xl_pog_prepare_rgba(const std::vector<xl_compat::PogBitmap> &images, unsigned max_bitmaps, bool scoped_texture)
{
	if (max_bitmaps < 2 || images.size() >= max_bitmaps)
		throw std::runtime_error("Decoded RGBA bitmap count exceeds its target bank");
	for (const auto &image : images) validate_rgba_target(image, image.index, max_bitmaps);
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
	std::vector<const xl_compat::ScopedTextureBinding *> images;
	std::map<std::size_t, std::size_t> image_rows;
	const unsigned max_bitmaps = tables.resource_banks->bitmap_count();
	size_t total = pog ? pog->pixels.size() : 0;
	for (const auto &binding : playback.bindings) {
		if (protected_indices.count(binding.bitmap)) throw std::runtime_error("Scoped texture conflicts with POG ownership");
		if (image_rows.emplace(binding.image, images.size()).second) {
			const auto &image = textures.images.at(binding.image).bitmap;
			validate_rgba_target(image, binding.bitmap, max_bitmaps);
			total = add_pixel_size(total, image.pixels.size());
			images.push_back(&binding);
		}
	}
	if (max_bitmaps < 2 || images.size() >= max_bitmaps)
		throw std::runtime_error("Decoded RGBA bitmap count exceeds its target bank");
	auto result = std::make_unique<xl_pog_resources>();
	result->pixels.reserve(total);
	result->entries.reserve(playback.bindings.size() + (pog ? pog->entries.size() : 0));
	for (const auto *binding : images)
		append_packed_image(*result, textures.images.at(binding->image).bitmap, binding->bitmap, true);
	// Native aliases have independent display views but share one retained image
	const auto unique_entries = result->entries;
	result->entries.clear();
	for (const auto &binding : playback.bindings) {
		auto entry = unique_entries.at(image_rows.at(binding.image));
		entry.index = unsigned(binding.bitmap);
		for (const auto &door : playback.doors)
			if (std::find(door.bitmaps.begin(), door.bitmaps.end(), binding.bitmap) != door.bitmaps.end()) entry.side_frames = 1;
		result->entries.push_back(entry);
	}
	if (pog) {
		result->effects = pog->effects;
		result->effect_bitmaps = pog->effect_bitmaps;
		result->doors = pog->doors;
		const auto offset = unsigned(result->pixels.size());
		for (auto entry : pog->entries) {
			entry.offset += offset;
			result->entries.push_back(entry);
		}
		result->pixels.insert(result->pixels.end(), pog->pixels.begin(), pog->pixels.end());
	}
	result->effect_definitions.assign(tables.effect_data, tables.effect_data + tables.effect_count);
	result->clip_definitions.assign(tables.clip_data, tables.clip_data + tables.clip_count);
	result->door_definitions.assign(tables.wall_clip_data, tables.wall_clip_data + tables.wall_clip_count);
	result->door_textures.assign(tables.texture_data, tables.texture_data + tables.texture_count);
	for (const auto &door : playback.doors)
		result->doors.emplace_back(door.clip, int(textures.images[door.image].bitmap.frames));
	for (const auto &animation : playback.clips) {
		const auto &clip = tables.clip_data[animation.clip];
		result->clips.push_back({ animation.clip, animation.bitmaps.front(),
		                          int(textures.images[animation.image].bitmap.frames), clip.frame_time, clip.play_time });
	}
	for (const auto &clock : playback.effects) {
		result->effects.push_back({ clock.effect, clock.frame_time, tables.effect_data[clock.effect].changing_object_texture,
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
extern "C" int xl_pog_prepared_door_frames(const xl_pog_resources *resources, int clip)
{
	if (resources)
		for (const auto &door : resources->doors)
			if (door.first == clip) return door.second;
	return 0;
}
extern "C" int xl_pog_clip_count(const xl_pog_resources *resources)
{
	return resources ? int(resources->clips.size()) : 0;
}
extern "C" const xl_pog_clip *xl_pog_clip_entry(const xl_pog_resources *resources, int row)
{
	return resources && row >= 0 && std::size_t(row) < resources->clips.size() ? &resources->clips[row] : nullptr;
}
extern "C" size_t xl_pog_pixel_bytes(const xl_pog_resources *resources)
{
	return resources ? resources->pixels.size() : 0;
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
	if (!resources || (!resources->scoped && resources->effects.empty() && resources->doors.empty())) return 1;
	if (!resources->doors.empty() && (Num_wall_anims != int(resources->door_definitions.size()) ||
	                                  NumTextures != int(resources->door_textures.size()))) return 0;
	for (const auto &door : resources->doors) {
		const auto &a = resources->door_definitions[door.first], &b = WallAnims[door.first];
		if (a.num_frames != b.num_frames || a.play_time != b.play_time || a.flags != b.flags ||
		    a.open_sound != b.open_sound || a.close_sound != b.close_sound) return 0;
		for (int frame = 0; frame < a.num_frames; ++frame) {
			if (a.frames[frame] != b.frames[frame]) return 0;
			const int texture = a.frames[frame];
			if (resources->door_textures[texture].index != Textures[texture].index) return 0;
		}
	}
	if (Num_vclips != int(resources->clip_definitions.size())) return 0;
	for (const auto &animation : resources->clips) {
		const auto &a = resources->clip_definitions[animation.clip], &b = Vclip[animation.clip];
		if (a.num_frames != b.num_frames || a.frame_time != b.frame_time || a.play_time != b.play_time || a.flags != b.flags) return 0;
		for (int frame = 0; frame < a.num_frames; ++frame)
			if (a.frames[frame].index != b.frames[frame].index) return 0;
	}
	if (Num_effects != int(resources->effect_definitions.size())) return 0;
	for (const auto &clock : resources->effects)
		if (clock.object_texture >= N_ObjBitmaps) return 0;
	for (int id = 0; id < Num_effects; ++id) {
		const auto &a = resources->effect_definitions[id], &b = Effects[id];
		if (a.vc.num_frames != b.vc.num_frames || a.vc.frame_time != b.vc.frame_time ||
		    a.changing_wall_texture != b.changing_wall_texture || a.changing_object_texture != b.changing_object_texture ||
		    a.crit_clip != b.crit_clip || a.dest_eclip != b.dest_eclip || ((a.flags ^ b.flags) & EF_CRITICAL)) return 0;
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

extern "C" int xl_pog_non_effect_clock(unsigned bitmap)
{
	PogClockTables live;
	live.effect_count = 0;
	return strip_clock(bitmap, live);
}

std::vector<xl_compat::ScopedTextureDoor> xl_pog_bind_animation(xl_pog_resources &resources, const xl_assets::ObjectDependencies &tables, int bitmap_count)
{
	std::map<int, std::size_t> selected;
	std::set<int> clocked;
	for (std::size_t i = 0; i < resources.entries.size(); ++i) {
		const auto &entry = resources.entries[i];
		selected[entry.index] = i;
		if (!entry.rgba || entry.frames < 2) continue;
		const int clock = xl_pog_target_strip_clock(entry.index, tables);
		if (clock < 0) throw std::runtime_error("POG: native bitmap animation tables exceed supported bounds");
		if (clock) clocked.insert(entry.index);
	}
	if (clocked.empty()) return {};
	std::map<int, int> owners;
	const auto frames = [&](std::size_t row) { return resources.entries[row].frames; };
	const auto effects = xl_compat::ScopedTexturePlayback::texture_effects(tables, bitmap_count, selected, {}, frames, owners);
	// The original loads replacement strips for all wall clocks, including clips
	// that have no wall in this level. Bind only a retained first-frame strip;
	// wall clips used by this level still require their actual door/blastable use.
	auto wall_tables = tables;
	for (int id = 0; id < tables.wall_clip_count; ++id) {
		if (tables.wall_clips.count(id)) continue;
		const auto &clip = tables.wall_clip_data[id];
		if (clip.num_frames < 1 || clip.num_frames > MAX_CLIP_FRAMES) continue;
		const int texture = clip.frames[0];
		if (texture < 0 || texture >= tables.texture_count) continue;
		if (!clocked.count(tables.texture_data[texture].index)) continue;
		wall_tables.wall_clips.insert(id);
		wall_tables.door_clips.insert(id);
	}
	const auto doors = xl_compat::ScopedTexturePlayback::wall_effects(wall_tables, bitmap_count, selected, {}, frames, owners);
	std::map<int, xl_pog_bitmap> wall_aliases;
	PogClockTables sprite_or_effect(tables);
	sprite_or_effect.wall_clip_count = 0;
	for (const auto &door : doors) {
		resources.doors.emplace_back(door.clip, door.frames);
		for (int bitmap : door.bitmaps) {
			if (strip_clock(bitmap, sprite_or_effect)) throw std::runtime_error("POG: wall clip shares a sprite or texture effect clock");
			auto entry = resources.entries[door.image];
			entry.index = bitmap;
			entry.side_frames = 1;
			wall_aliases.emplace(bitmap, entry);
		}
	}
	for (int bitmap : clocked)
		if (!owners.count(bitmap) && !wall_aliases.count(bitmap))
			throw std::runtime_error("POG: RGBA bitmap " + std::to_string(bitmap) + " has clocked animation frames without a qualified owner");
	PogClockTables other(tables);
	other.effect_count = 0;
	for (const auto &clock : effects) {
		const auto source = resources.entries.at(clock.image);
		resources.effects.push_back({ clock.effect, clock.frame_time, tables.effect_data[clock.effect].changing_object_texture,
		                              unsigned(clock.bitmaps.size()), source.frames });
		resources.effect_bitmaps.push_back(clock.bitmaps);
		for (int bitmap : clock.bitmaps) {
			if (strip_clock(bitmap, other)) throw std::runtime_error("POG: texture effect frames share a door or vclip clock");
			auto entry = source;
			entry.index = bitmap;
			entry.effect_clock = 1;
			const auto row = selected.emplace(bitmap, resources.entries.size());
			if (row.second) resources.entries.push_back(entry);
			else resources.entries[row.first->second] = entry;
		}
	}
	for (const auto &alias : wall_aliases) {
		const auto row = selected.emplace(alias.first, resources.entries.size());
		if (row.second) resources.entries.push_back(alias.second);
		else resources.entries[row.first->second] = alias.second;
	}
	// Earlier rows targeting a door alias are replaced by the selected strip.
	resources.entries.erase(std::remove_if(resources.entries.begin(), resources.entries.end(),
	                                       [&](const xl_pog_bitmap &entry) { return !entry.side_frames && wall_aliases.count(entry.index); }),
	                        resources.entries.end());
	resources.effect_definitions.assign(tables.effect_data, tables.effect_data + tables.effect_count);
	resources.clip_definitions.assign(tables.clip_data, tables.clip_data + tables.clip_count);
	if (!doors.empty()) {
		resources.door_definitions.assign(tables.wall_clip_data, tables.wall_clip_data + tables.wall_clip_count);
		resources.door_textures.assign(tables.texture_data, tables.texture_data + tables.texture_count);
	}
	return doors;
}
