#ifndef DXX_XL_SCOPED_TEXTURE_RESOURCES_H
#define DXX_XL_SCOPED_TEXTURE_RESOURCES_H
#include "../scoped_texture_image.h"
#include "../scoped_texture.h"
#include "asset_dependencies.h"
#include "pog_resources.h"
#include "target_sources.h"
#include "picosha2.h"
#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace xl_compat
{
struct ScopedTextureImage {
	std::string path, hash;
	PogBitmap bitmap;
};
struct ScopedTextureBinding {
	int bitmap;
	std::size_t image;
};

// Retain authored inputs independently of live registry, paging and GL state
struct ScopedTextureResources {
	std::vector<ScopedTextureImage> images;
	std::vector<ScopedTextureBinding> bindings;
	std::set<int> pog_indices;
	static ScopedTextureResources prepare(TargetSources &sources, const xl_assets::ObjectDependencies &graph,
	                                      int ordinal, const xl_pog_resources *pog = nullptr)
	{
		if (!graph.resource_banks || !graph.require_owned_banks || !ordinal || ordinal < -1000 || ordinal > 1000)
			throw std::runtime_error("Scoped texture preparation requires owned banks and a bounded level ordinal");
		ScopedTextureResources result;
		std::map<std::string, std::size_t> retained;
		auto &pog_indices = result.pog_indices;
		for (int row = 0; row < xl_pog_count(pog); ++row) pog_indices.insert(int(xl_pog_entry(pog, row)->index));
		std::size_t total = 0;
		char level[32];
		std::snprintf(level, sizeof(level), "%s%02d/", ordinal < 0 ? "slevel" : "level", ordinal < 0 ? -ordinal : ordinal);
		for (int id : graph.bitmaps) {
			if (pog_indices.count(id)) continue;
			const char *name = graph.resource_banks->bitmap_name(id);
			if (!name) throw std::runtime_error("Scoped texture dependency lacks its retained bitmap name");
			const std::string bitmap_name(name);
			const auto marker = bitmap_name.find('#');
			std::vector<std::string> names{ bitmap_name };
			if (marker != std::string::npos && bitmap_name.substr(marker) != "#0")
				names.push_back(bitmap_name.substr(0, marker) + "#0");
			bool found = false;
			// FindHiresBitmap exhausts exact-frame level/mission paths before #0
			for (const auto &candidate : names) {
				auto mission_path = marker == std::string::npos ? static_mission_texture_path(sources.archive().c_str(), candidate.c_str()) : animation_mission_texture_path(sources.archive().c_str(), candidate.c_str());
				if (mission_path.empty()) throw std::runtime_error("Scoped texture dependency has an unsupported bitmap name");
				auto level_path = mission_path;
				level_path.insert(level_path.find_last_of('/') + 1, level);
				for (const auto &path : { level_path, mission_path }) {
					auto entry = retained.find(path);
					if (entry == retained.end()) {
						const auto bytes = sources.directory_bytes(path, pog_file_limit + 65536);
						if (!bytes) continue;
						PogBitmap image;
						try {
							image = read_scoped_texture_image(bytes->data(), bytes->size());
						} catch (const std::exception &error) {
							throw std::runtime_error("XL scoped texture decode failed for " + path + ": " + error.what());
						}
						if (image.pixels.size() > 2 * pog_file_limit - total)
							throw std::runtime_error("Scoped texture decoded pixels exceed the resource bound");
						total += image.pixels.size();
						const auto index = result.images.size();
						result.images.push_back({ path, picosha2::hash256_hex_string(*bytes), std::move(image) });
						entry = retained.emplace(path, index).first;
					}
					result.bindings.push_back({ id, entry->second });
					found = true;
					break;
				}
				if (found) break;
			}
		}
		return result;
	}
};
} // namespace xl_compat
#endif
