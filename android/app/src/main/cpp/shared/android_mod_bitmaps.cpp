#include "android_mod_bitmaps.h"
#include "android_mod_assets.h"
#include <cstdio>
#include <cstring>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
extern "C" {
#include "gr.h"
#include "bm.h"
#include "piggy.h"
#include "physfsx.h"
#include "console.h"
#include "strutil.h"
#ifdef DXX_BUILD_DESCENT_II
#include "d1_in_d2/d1_in_d2.h"
#endif
}

namespace
{
struct replacement {
	grs_bitmap original{};
	int original_offset = 0;
	unsigned char original_flags = 0, flags = 0, average = 0;
	int width = 0, height = 0;
	std::vector<unsigned char> pixels;
};
std::map<int, replacement> replacements;
unsigned word(const std::vector<unsigned char> &data, size_t offset)
{
	return data.at(offset) | (static_cast<unsigned>(data.at(offset + 1)) << 8);
}
unsigned dword(const std::vector<unsigned char> &data, size_t offset)
{
	return word(data, offset) | (word(data, offset + 2) << 16);
}
std::string path_for(int content_game, const char *pig_name)
{
	if (content_game == 1) {
#ifdef DXX_BUILD_DESCENT_II
		return "d1-mods/patches/d1/bitmaps.pog";
#else
		return "patches/d1/bitmaps.pog";
#endif
	}
	std::string name = pig_name ? pig_name : "";
	name = name.substr(name.find_last_of("/\\") + 1);
	name = name.substr(0, name.find_last_of('.'));
	for (auto &c : name)
		if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
	return "patches/d2/sets/" + name + ".pog";
}
} // namespace

void android_mod_bitmaps_reset(void)
{
	for (auto &[index, entry] : replacements) {
		auto &bitmap = GameBitmaps[index];
		/* A level-specific replacement may already own this slot */
		if (bitmap.bm_data != entry.pixels.data()) continue;
		gr_set_bitmap_data(&bitmap, nullptr);
		bitmap = entry.original;
		piggy_bitmap_set_file_state(index, entry.original_offset, entry.original_flags);
	}
	replacements.clear();
}

void android_mod_bitmaps_overlay_flags(int *flags, int capacity)
{
	for (const auto &[index, entry] : replacements)
		if (index < capacity && GameBitmaps[index].bm_data == entry.pixels.data())
			flags[index] = entry.flags;
}

int android_mod_bitmap_is_owned(int index)
{
	const auto entry = replacements.find(index);
	return entry != replacements.end() && index >= 0 && index < Num_bitmap_files &&
	       GameBitmaps[index].bm_data == entry->second.pixels.data();
}

void android_mod_bitmaps_refresh_palette(void)
{
	for (const auto &[index, entry] : replacements)
		if (GameBitmaps[index].bm_data == entry.pixels.data())
			compute_average_rgb(&GameBitmaps[index], GameBitmaps[index].avg_color_rgb);
}

int android_mod_bitmaps_apply(int content_game, const char *pig_name)
{
	const auto path = path_for(content_game, pig_name);
	PHYSFS_file *file = PHYSFS_openRead(path.c_str());
	if (!file) return 0;
	try {
		const auto length = PHYSFS_fileLength(file);
		if (length < 12 || length > 64 * 1024 * 1024) throw std::runtime_error("POG size out of range");
		std::vector<unsigned char> data(static_cast<size_t>(length));
		if (PHYSFS_readBytes(file, data.data(), data.size()) != length) throw std::runtime_error("truncated POG");
		PHYSFS_close(file);
		file = nullptr;
		if (std::memcmp(data.data(), "DPOG", 4) || dword(data, 4) != 1) throw std::runtime_error("invalid POG header");
		const unsigned count = dword(data, 8);
		const size_t payload = 12 + static_cast<size_t>(count) * 20;
		if (count >= MAX_BITMAP_FILES || payload > data.size()) throw std::runtime_error("invalid POG table");
		std::map<int, replacement> staged;
		size_t consumed = payload;
		for (unsigned i = 0; i < count; ++i) {
			const int index = static_cast<int>(word(data, 12 + i * 2));
			if (index <= 0 || index >= Num_bitmap_files || staged.count(index)) throw std::runtime_error("invalid POG index");
			const size_t header = 12 + count * 2 + i * 18;
			char name[13] = {};
			std::memcpy(name, data.data() + header, 8);
			if (data[header + 8] & 64) {
				const std::string animated = std::string(name) + "#" + std::to_string(data[header + 8] & 63);
				std::snprintf(name, sizeof(name), "%s", animated.c_str());
			}
			const char *runtime_name = piggy_game_bitmap_name(&GameBitmaps[index]);
			if (!runtime_name || d_stricmp(name, runtime_name)) throw std::runtime_error("POG bitmap name does not match installed data");
			auto &entry = staged[index];
			entry.width = data[header + 9] | ((data[header + 11] & 15) << 8);
			entry.height = data[header + 10] | ((data[header + 11] & 240) << 4);
			entry.flags = data[header + 12];
			entry.average = data[header + 13];
			const size_t size = static_cast<size_t>(entry.width) * entry.height;
			const size_t offset = payload + dword(data, header + 14);
			if (!size || size > 16 * 1024 * 1024 || entry.flags & ~7 || offset != consumed || size > data.size() - consumed)
				throw std::runtime_error("invalid uncompressed POG pixels");
			entry.pixels.assign(data.begin() + offset, data.begin() + offset + size);
			consumed += size;
		}
		if (consumed != data.size()) throw std::runtime_error("unused POG payload");
#ifdef DXX_BUILD_DESCENT_II
		/* D1 generations already contain their mission's PG1/DTX replacements */
		if (content_game == 1) {
			for (auto it = staged.begin(); it != staged.end();)
				if (d1_in_d2_bitmap_is_custom(it->first)) it = staged.erase(it);
				else ++it;
		}
#endif
		/* No live state changes until every entry is validated and allocated */
		android_mod_bitmaps_reset();
		replacements.swap(staged);
		for (auto &[index, entry] : replacements) {
			auto &bitmap = GameBitmaps[index];
			entry.original = bitmap;
			entry.original_offset = piggy_bitmap_get_offset(index);
			entry.original_flags = piggy_bitmap_get_file_flags(index);
			gr_set_bitmap_data(&bitmap, nullptr);
#ifdef OGL
			entry.original.gltexture = entry.original.gltexture_mask = nullptr;
#endif
			gr_init_bitmap(&bitmap, 0, 0, 0, entry.width, entry.height, entry.width, entry.pixels.data());
			gr_set_bitmap_flags(&bitmap, entry.flags);
			bitmap.avg_color = entry.average;
			compute_average_rgb(&bitmap, bitmap.avg_color_rgb);
			piggy_bitmap_set_file_state(index, 0, entry.flags);
		}
		android_mod_asset_loaded("bitmap_patch", path.c_str(), 0, 0);
		con_printf(CON_NORMAL, "DXA indexed bitmaps: applied %zu from %s\n", replacements.size(), path.c_str());
		return static_cast<int>(replacements.size());
	} catch (const std::exception &ex) {
		if (file) PHYSFS_close(file);
		con_printf(CON_URGENT, "DXA indexed bitmaps rejected %s: %s\n", path.c_str(), ex.what());
		return -1;
	}
}
