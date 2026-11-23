#ifndef DXX_XL_SCOPED_TEXTURE_RESOURCES_H
#define DXX_XL_SCOPED_TEXTURE_RESOURCES_H
#include "../scoped_texture_image.h"
#include "../scoped_texture.h"
#include "asset_dependencies.h"
#include "pog_resources.h"
#include "target_sources.h"
#include "picosha2.h"
#include <cstdio>
#include <chrono>
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
struct ScopedTexturePreparationTiming {
	long long headers_us = 0, reads_us = 0, decode_us = 0, shrink_us = 0, hash_us = 0;
};

// Retain authored inputs independently of live registry, paging and GL state
struct ScopedTextureResources {
	std::vector<ScopedTextureImage> images;
	std::vector<ScopedTextureBinding> bindings;
	std::set<int> pog_indices;
	unsigned texture_target = 0;
	static ScopedTextureResources prepare(TargetSources &sources, const xl_assets::ObjectDependencies &graph,
	                                      int ordinal, const xl_pog_resources *pog = nullptr,
	                                      ScopedTexturePreparationTiming *timing = nullptr)
	{
		const auto now = [] { return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(); };
		const auto headers_started = timing ? now() : 0;
		if (!graph.resource_banks || !graph.require_owned_banks || !ordinal || ordinal < -1000 || ordinal > 1000)
			throw std::runtime_error("Scoped texture preparation requires owned banks and a bounded level ordinal");
		ScopedTextureResources result;
		std::map<std::string, std::size_t> retained;
		auto &pog_indices = result.pog_indices;
		for (int row = 0; row < xl_pog_count(pog); ++row) pog_indices.insert(int(xl_pog_entry(pog, row)->index));
		// Read only headers until the complete incoming level has a fitting source
		// resolution. Bindings sharing one file occupy one image in the arena.
		struct Source {
			std::string path;
			unsigned width, height;
		};
		std::vector<Source> inputs;
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
						const auto header = sources.directory_prefix(path, 18, pog_file_limit + 65536);
						if (!header) continue;
						if (header->size() != 18) throw std::runtime_error("XL scoped texture header truncated: " + path);
						const unsigned width = (*header)[12] | unsigned((*header)[13]) << 8;
						const unsigned height = (*header)[14] | unsigned((*header)[15]) << 8;
						if (!width || width > 4096 || !height || height > 32767 || std::uint64_t(width) * height * 4 > pog_file_limit)
							throw std::runtime_error("XL scoped texture dimensions exceed their bound: " + path);
						const auto index = inputs.size();
						inputs.push_back({ path, width, height });
						entry = retained.emplace(path, index).first;
					}
					result.bindings.push_back({ id, entry->second });
					found = true;
					break;
				}
				if (found) break;
			}
		}
		for (const unsigned target : { 0u, 256u, 128u, 64u }) {
			std::uint64_t total = xl_pog_pixel_bytes(pog);
			for (const auto &source : inputs) {
				const unsigned factor = scoped_texture_factor(source.width, source.height, target);
				total += std::uint64_t(source.width / factor) * (source.height / factor) * 4;
			}
			if (total <= 2 * pog_file_limit) {
				result.texture_target = target;
				break;
			}
			if (target == 64) throw std::runtime_error("Scoped texture decoded pixels exceed the resource bound");
		}
		if (timing) timing->headers_us = now() - headers_started;
		for (const auto &source : inputs) {
			const auto started = timing ? now() : 0;
			const auto bytes = sources.directory_bytes(source.path, pog_file_limit + 65536);
			if (timing) timing->reads_us += now() - started;
			if (!bytes) throw std::runtime_error("Incoming scoped texture disappeared: " + source.path);
			PogBitmap image;
			try {
				const auto decoded = timing ? now() : 0;
				image = read_scoped_texture_image(bytes->data(), bytes->size());
				if (timing) timing->decode_us += now() - decoded;
				if (image.width != source.width || image.height != source.height)
					throw std::runtime_error("Source dimensions changed after size selection");
				const auto shrunk = timing ? now() : 0;
				shrink_scoped_texture(image, scoped_texture_factor(image.width, image.height, result.texture_target));
				if (timing) timing->shrink_us += now() - shrunk;
			} catch (const std::exception &error) {
				throw std::runtime_error("XL scoped texture decode failed for " + source.path + ": " + error.what());
			}
			const auto hashed = timing ? now() : 0;
			// PicoSHA2's container overload copies the entire input into its
			// working buffer. Bound each batch while preserving the same SHA-256.
			picosha2::hash256_one_by_one digest;
			for (std::size_t offset = 0; offset < bytes->size();) {
				const auto count = (std::min) (std::size_t(65536), bytes->size() - offset);
				digest.process(bytes->data() + offset, bytes->data() + offset + count);
				offset += count;
			}
			digest.finish();
			auto hash = picosha2::get_hash_hex_string(digest);
			if (timing) timing->hash_us += now() - hashed;
			result.images.push_back({ source.path, std::move(hash), std::move(image) });
		}
		return result;
	}
};
} // namespace xl_compat
#endif
