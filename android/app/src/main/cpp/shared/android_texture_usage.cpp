/* Android texture provenance from successful uploads and live GL residency */
#include "android_texture_usage.h"
#ifdef OGL
#include <nlohmann/json.hpp>
#include <map>
#include <algorithm>
#include <string>
#include <vector>
extern "C" {
#include "gr.h"
#include "piggy.h"
#include "ogl_init.h"
#include "physfs.h"
#include "android_visual_policy.h"
}

namespace
{
std::vector<std::string> sources;
std::string snapshot;
} // namespace

unsigned int android_texture_source_id(const char *filename)
{
	const char *source = android_texture_source(filename);
	if (!source) return 0;
	for (size_t i = 0; i < sources.size(); ++i)
		if (sources[i] == source) return static_cast<unsigned int>(i + 1);
	sources.emplace_back(source);
	return static_cast<unsigned int>(sources.size());
}

const char *android_texture_source_name(unsigned int id)
{
	return id && id <= sources.size() ? sources[id - 1].c_str() : "";
}

const char *android_texture_usage_json(void)
{
	struct counts {
		int loaded = 0, mipmapped = 0, adapted = 0, max_mips = 0, width = 0, height = 0;
	};
	std::map<unsigned int, counts> packs;
	int loaded = 0, mipmapped = 0, adapted = 0;
	uint64_t additive_draws = 0;
	nlohmann::json additive_sprites = nlohmann::json::array();
	for (int i = 0; i < Num_bitmap_files; ++i) {
		const auto *texture = GameBitmaps[i].gltexture;
		if (!texture || !texture->handle || !texture->replacement_source || texture->replacement_source > sources.size()) continue;
		auto &pack = packs[texture->replacement_source];
		if (texture->sprite_additive) {
			const char *name = piggy_game_bitmap_name(&GameBitmaps[i]);
			additive_draws += texture->additive_draws;
			additive_sprites.push_back({ { "bitmap", i }, { "name", name ? name : "" }, { "source", sources[texture->replacement_source - 1] }, { "width", texture->w }, { "draws", texture->additive_draws } });
		}
		++pack.loaded;
		++loaded;
		if (texture->has_mipmaps) {
			++pack.mipmapped;
			++mipmapped;
		}
		if (texture->mip_base_level) {
			++pack.adapted;
			++adapted;
		}
		pack.max_mips = (std::max) (pack.max_mips, texture->authored_mip_count);
		pack.width = (std::max) (pack.width, texture->w);
		pack.height = (std::max) (pack.height, texture->h);
	}
	nlohmann::json entries = nlohmann::json::array();
	for (const auto &entry : packs) {
		const auto &source = sources[entry.first - 1];
		const auto &count = entry.second;
		entries.push_back({ { "source", source }, { "name", source.substr(source.find_last_of("/\\") + 1) }, { "loaded", count.loaded }, { "mipmapped", count.mipmapped }, { "adapted", count.adapted }, { "max_mips", count.max_mips }, { "width", count.width }, { "height", count.height } });
	}
	nlohmann::json mounted = nlohmann::json::array();
	char **paths = PHYSFS_getSearchPath();
	if (paths) {
		for (char **p = paths; *p; ++p) {
			std::string path(*p);
			if (path.size() >= 4 && path.compare(path.size() - 4, 4, ".dxa") == 0) mounted.push_back(path);
		}
		PHYSFS_freeList(paths);
	}
	GLint max_size = 0;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_size);
	const bool allowed = android_visual_replacements_allowed();
	snapshot = nlohmann::json({ { "status", !allowed ? "blocked_multiplayer" : loaded ? "active"
	                                                                                  : "none_loaded" },
	                            { "loaded", loaded },
	                            { "mipmapped", mipmapped },
	                            { "adapted", adapted },
	                            { "additive_sprite_draws", additive_draws },
	                            { "additive_sprites", additive_sprites },
	                            { "max_texture_size", max_size },
	                            { "packs", entries },
	                            { "mounted_mods", mounted } })
	               .dump();
	return snapshot.c_str();
}
#else
const char *android_texture_source_name(unsigned int)
{
	return "";
}
unsigned int android_texture_source_id(const char *)
{
	return 0;
}
const char *android_texture_usage_json(void)
{
	return "{}";
}
#endif
