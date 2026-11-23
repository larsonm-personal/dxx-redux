#ifndef DXX_XL_SCOPED_TEXTURE_RESOURCES_H
#define DXX_XL_SCOPED_TEXTURE_RESOURCES_H
#ifdef min
#pragma push_macro("min")
#undef min
#define DXX_XL_RESTORE_MIN
#endif
#ifdef max
#pragma push_macro("max")
#undef max
#define DXX_XL_RESTORE_MAX
#endif
#include <cstdio>
#include <deque>
#include <future>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "../scoped_texture_image.h"
#include "../scoped_texture.h"
#include "asset_dependencies.h"
#include "pog_resources.h"
#include "target_sources.h"
#include "picosha2.h"
#ifdef DXX_XL_RESTORE_MAX
#pragma pop_macro("max")
#undef DXX_XL_RESTORE_MAX
#endif
#ifdef DXX_XL_RESTORE_MIN
#pragma pop_macro("min")
#undef DXX_XL_RESTORE_MIN
#endif

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
	long long (*now)() = nullptr;
	long long headers_us = 0, reads_us = 0, decode_us = 0, shrink_us = 0, hash_us = 0;
	std::size_t parallel_images = 0;
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
		const auto now = [timing] { return timing ? timing->now() : 0LL; };
		if (timing && !timing->now) throw std::runtime_error("Scoped texture timing requires a clock");
		const auto headers_started = now();
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
		struct Decoded {
			ScopedTextureImage image;
			long long decode_us = 0, shrink_us = 0, hash_us = 0;
		};
		const auto decode = [target = result.texture_target, clock = timing ? timing->now : nullptr](Source source, std::vector<unsigned char> bytes) {
			const auto tick = [clock] { return clock ? clock() : 0LL; };
			Decoded decoded;
			decoded.image.path = std::move(source.path);
			try {
				const auto started = tick();
				decoded.image.bitmap = read_scoped_texture_image(bytes.data(), bytes.size());
				decoded.decode_us = tick() - started;
				if (decoded.image.bitmap.width != source.width || decoded.image.bitmap.height != source.height)
					throw std::runtime_error("Source dimensions changed after size selection");
				const auto shrunk = tick();
				shrink_scoped_texture(decoded.image.bitmap, scoped_texture_factor(source.width, source.height, target));
				decoded.shrink_us = tick() - shrunk;
			} catch (const std::exception &error) {
				throw std::runtime_error("XL scoped texture decode failed for " + decoded.image.path + ": " + error.what());
			}
			const auto hashed = tick();
			// PicoSHA2's container overload copies the entire input into its
			// working buffer. Bound each batch while preserving the same SHA-256.
			picosha2::hash256_one_by_one digest;
			for (std::size_t offset = 0; offset < bytes.size();) {
				const auto count = (std::min) (std::size_t(65536), bytes.size() - offset);
				digest.process(bytes.data() + offset, bytes.data() + offset + count);
				offset += count;
			}
			digest.finish();
			decoded.image.hash = picosha2::get_hash_hex_string(digest);
			decoded.hash_us = tick() - hashed;
			return decoded;
		};
		std::deque<std::future<Decoded>> pending;
		const auto retain = [&](Decoded decoded) {
			if (timing) {
				timing->decode_us += decoded.decode_us;
				timing->shrink_us += decoded.shrink_us;
				timing->hash_us += decoded.hash_us;
			}
			result.images.push_back(std::move(decoded.image));
		};
		const auto collect = [&] {
			auto decoded = pending.front().get();
			pending.pop_front();
			retain(std::move(decoded));
		};
		for (const auto &source : inputs) {
			const auto started = timing ? now() : 0;
			auto bytes = sources.directory_bytes(source.path, pog_file_limit + 65536);
			if (timing) timing->reads_us += now() - started;
			if (!bytes) throw std::runtime_error("Incoming scoped texture disappeared: " + source.path);
			// Keep at most two small images in flight. Large authored TGAs stay
			// serial so the existing source and decoded-pixel bounds remain useful.
			if (bytes->size() > 8 * 1024 * 1024) {
				while (!pending.empty()) collect();
				retain(decode(source, std::move(*bytes)));
			} else {
				if (pending.size() == 2) collect();
				pending.push_back(std::async(std::launch::async, decode, source, std::move(*bytes)));
				if (timing) ++timing->parallel_images;
			}
		}
		while (!pending.empty()) collect();
		return result;
	}
};
} // namespace xl_compat
#endif
