/* Movie dependencies are interpreted beside the engine, never in the launcher */
#pragma once

#include <algorithm>
#include <string>
#include <vector>

extern "C" {
#include "mission.h"
#include "physfsx.h"
#include "strutil.h"
#include "text.h"
#ifdef DXX_BUILD_DESCENT_II
const char *endlevel_movie_filename(int level);
const char *level_intro_movie_filename(int level);
const char *endgame_movie_filename(void);
#endif
}

namespace mission_movie_scan
{
/* Match briefing $R's single-character movie selector and line comments */
inline std::vector<std::string> briefing_movies(const std::string &text)
{
	std::vector<std::string> names;
	bool line_start = true;
	for (size_t i = 0; i < text.size(); ++i) {
		if (line_start && text[i] == ';') {
			while (i < text.size() && text[i] != '\n') ++i;
		}
		if (i >= text.size()) break;
		if (text[i] == '$' && i + 2 < text.size() && text[i + 1] == 'R') {
			const unsigned char selector = text[i + 2];
			if (selector > ' ' && selector < 127) {
				std::string name = "rba.mve";
				name[2] = selector;
				names.push_back(name);
			}
			i += 2;
		} else if (text[i] == '$' && i + 1 < text.size()) {
			++i;
		}
		line_start = text[i] == '\n';
	}
	return names;
}

inline bool readable(std::string name)
{
	std::vector<char> path(name.begin(), name.end());
	path.push_back(0);
	PHYSFSEXT_locateCorrectCase(path.data());
	PHYSFS_File *file = PHYSFS_openRead(path.data());
	if (!file) return false;
	const bool nonempty = PHYSFS_fileLength(file) > 0;
	PHYSFS_close(file);
	return nonempty;
}

template <typename Json>
Json collect()
{
	Json result = { { "status", "not_applicable" }, { "required", Json::array() }, { "missing", Json::array() }, { "unavailable_libraries", Json::array() }, { "problems", Json::array() } };
#ifdef DXX_BUILD_DESCENT_II
	if (!Current_mission) {
		result["status"] = "unavailable";
		return result;
	}
	if (EMULATING_D1) return result;
	std::vector<std::string> names;
	for (const char *briefing : { Briefing_text_filename, Ending_text_filename }) {
		if (!*briefing) continue;
		char path[PATH_MAX];
		snprintf(path, sizeof(path), "%s", briefing);
		PHYSFSEXT_locateCorrectCase(path);
		PHYSFS_File *file = PHYSFS_openRead(path);
		if (!file) continue; // Missions need not provide an optional briefing or ending
		const auto size = PHYSFS_fileLength(file);
		std::string text;
		bool valid = size >= 0 && size <= 4 * 1024 * 1024;
		if (valid) {
			text.resize(static_cast<size_t>(size));
			valid = !size || PHYSFS_readBytes(file, &text[0], size) == size;
		}
		PHYSFS_close(file);
		if (!valid) {
			result["problems"].push_back(std::string("Could not read briefing: ") + briefing);
			continue;
		}
		const std::string filename(path);
		if (!text.empty() && filename.size() >= 4 && !d_stricmp(filename.c_str() + filename.size() - 4, ".txb"))
			decode_text(&text[0], static_cast<int>(text.size()));
		text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
		const auto movies = briefing_movies(text);
		names.insert(names.end(), movies.begin(), movies.end());
	}
	for (int level = 1; level <= Last_level; ++level) {
		if (const char *name = level_intro_movie_filename(level)) names.push_back(std::string(name) + ".mve");
		if (PLAYING_BUILTIN_MISSION)
			if (const char *name = endlevel_movie_filename(level)) names.push_back(name);
	}
	if (const char *name = endgame_movie_filename()) names.push_back(std::string(name) + ".mve");
	std::sort(names.begin(), names.end());
	names.erase(std::unique(names.begin(), names.end()), names.end());
	/* Metadata workers do not play movies. Mount the same preferred/fallback
	 * libraries for this scan only, and release exactly the mounts we added */
	struct library_mounts {
		std::vector<std::string> paths;
		~library_mounts()
		{
			for (auto p = paths.rbegin(); p != paths.rend(); ++p) PHYSFS_unmount(p->c_str());
		}
	} mounted;
	std::vector<std::string> libraries;
	if (PLAYING_BUILTIN_MISSION) libraries = { "intro", "other" };
	libraries.push_back("robots");
#ifdef D2_OEM
	libraries.push_back("oem");
#endif
	if (Current_mission->enhanced) libraries.push_back(Current_mission_filename);
	for (const auto &library : libraries) {
		bool available = false;
		for (const char *resolution : { "-h.mvl", "-l.mvl" }) {
			char path[PATH_MAX], real[PATH_MAX];
			snprintf(path, sizeof(path), "%s%s", library.c_str(), resolution);
			PHYSFSEXT_locateCorrectCase(path);
			if (!PHYSFSX_getRealPath(path, real)) continue;
			if (PHYSFS_getMountPoint(real)) {
				available = true;
				break;
			}
			if (PHYSFS_mount(real, nullptr, 0)) {
				mounted.paths.emplace_back(real);
				available = true;
				break;
			}
		}
		if (!available) result["unavailable_libraries"].push_back(library + "-h.mvl / " + library + "-l.mvl");
	}
	for (const auto &name : names) {
		result["required"].push_back(name);
		if (!readable(name)) result["missing"].push_back(name);
	}
	result["status"] = result["problems"].empty() ? "ok" : "partial";
#endif
	return result;
}
} // namespace mission_movie_scan
