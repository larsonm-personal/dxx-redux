#ifndef DXX_FLYOUT_METADATA_HPP
#define DXX_FLYOUT_METADATA_HPP

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

extern "C" {
#include "gameseg.h"
#include "mission.h"
#include "physfsx.h"
#include "segment.h"
#include "switch.h"
#include "text.h"
#include "wall.h"
#ifdef DXX_BUILD_DESCENT_II
#include "piggy.h"
const char *endlevel_movie_filename(int level);
#endif
}

namespace flyout_metadata {
using json = nlohmann::ordered_json;

inline json unavailable(const char *reason)
{
	return { { "kind", "none" }, { "seconds", nullptr }, { "status", reason } };
}

inline uint32_t little32(const unsigned char *p)
{
	return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}

/* Scan opcodes, not compressed image/audio payloads. No multiplayer timing cap */
inline json movie(const char *name)
{
	json result = { { "kind", "video" }, { "seconds", nullptr }, { "status", "missing_movie" }, { "file", name } };
	PHYSFS_file *file = PHYSFSX_openReadBuffered(name);
	if (!file) return result;
	unsigned char header[26], chunk[4], opcode[4], timer[6];
	const auto end = PHYSFS_fileLength(file);
	uint64_t frame_us = 0, total_us = 0, frames = 0, records = 0;
	bool valid = PHYSFS_readBytes(file, header, sizeof(header)) == sizeof(header) &&
	             !memcmp(header, "Interplay MVE File\x1a", 19);
	while (valid && PHYSFS_tell(file) < end) {
		if (PHYSFS_readBytes(file, chunk, 4) != 4) { valid = false; break; }
		const auto chunk_end = PHYSFS_tell(file) + (chunk[0] | unsigned(chunk[1]) << 8);
		if (chunk_end > end) { valid = false; break; }
		while (valid && PHYSFS_tell(file) < chunk_end) {
			if (++records > 1000000 || PHYSFS_tell(file) + 4 > chunk_end ||
			    PHYSFS_readBytes(file, opcode, 4) != 4) { valid = false; break; }
			const unsigned size = opcode[0] | unsigned(opcode[1]) << 8;
			const auto next = PHYSFS_tell(file) + size;
			if (next > chunk_end) { valid = false; break; }
			if (opcode[2] == 2) {
				if (size < 6 || PHYSFS_readBytes(file, timer, 6) != 6) { valid = false; break; }
				frame_us = uint64_t(little32(timer)) * (timer[4] | unsigned(timer[5]) << 8);
				if (!frame_us) { valid = false; break; }
			}
			if (opcode[2] == 7) {
				if (!frame_us || total_us > UINT64_MAX - frame_us) { valid = false; break; }
				total_us += frame_us;
				++frames;
			}
			if (!PHYSFS_seek(file, next)) valid = false;
		}
	}
	PHYSFS_close(file);
	result["status"] = valid && frames ? "measured" : "invalid_movie";
	if (valid && frames) {
		result["seconds"] = std::round(total_us / 1000.0) / 1000.0;
		result["frames"] = frames;
	}
	return result;
}

inline std::string with_extension(const char *level, const char *extension)
{
	std::string name = level ? level : "";
	const auto dot = name.find_last_of('.');
	if (dot == std::string::npos) return {};
	return name.substr(0, dot + 1) + extension;
}

/* Mirror the engine's END/TXB lookup and level-one fallback without loading graphics */
inline json presentation(const char *level_file, int level_num)
{
	json result = { { "status", "missing_endlevel_data" } };
	PHYSFS_file *file = nullptr;
	bool binary = false;
	std::string name;
	for (int attempt = 0; attempt < 2 && !file; ++attempt) {
		if (attempt && (level_num == 1 || !Current_mission || Last_level < 1 || !Level_names)) break;
		const char *source = attempt ? Level_names[0] : level_file;
		if (!source || !source[0]) break;
		name = with_extension(source, "end");
		if (name.empty()) break;
		file = PHYSFSX_openReadBuffered(name.c_str());
		if (!file) {
			name = with_extension(source, "txb");
			if (Current_mission && (name == Briefing_text_filename || name == Ending_text_filename)) break;
			file = PHYSFSX_openReadBuffered(name.c_str());
			binary = file != nullptr;
		}
		if (file) result["file"] = name;
	}
	if (!file) return result;
	std::vector<std::string> values;
	char line[256];
	while (values.size() <= 8 && PHYSFSX_fgets(line, sizeof(line), file)) {
		if (binary) decode_text_line(line);
		std::string value(line);
		value = value.substr(0, value.find(';'));
		const auto first = value.find_first_not_of(" \t\r\n");
		if (first != std::string::npos)
			values.push_back(value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1));
	}
	PHYSFS_close(file);
	if (values.size() != 8) { result["status"] = "invalid_endlevel_data"; return result; }
	json missing = json::array();
	for (int index : { 0, 1, 4 })
		if (!PHYSFSX_exists(values[index].c_str(), 1)) missing.push_back(values[index]);
#ifdef DXX_BUILD_DESCENT_II
	if (Piggy_hamfile_version >= 3) {
		PHYSFS_file *pig = PHYSFSX_openReadBuffered("descent.pig");
		const bool pig_models = pig && PHYSFS_fileLength(pig) == D1_PIGSIZE;
		if (!pig) {
			for (const char *bitmap : { "steel1.bbm", "rbot061.bbm", "rbot062.bbm", "rbot063.bbm" })
				if (!PHYSFSX_exists(bitmap, 1)) missing.push_back(bitmap);
		}
		if (pig) PHYSFS_close(pig);
		if (!pig_models && !PHYSFSX_exists("exit.ham", 1) &&
		    !(PHYSFSX_exists("exit01.pof", 1) && PHYSFSX_exists("exit01d.pof", 1)))
			missing.push_back("exit models");
	}
#endif
	result["status"] = missing.empty() ? "present" : "missing_assets";
	result["asset_check"] = "presence_only";
	if (!missing.empty()) result["missing_assets"] = missing;
	return result;
}

inline double distance(const vms_vector &a, const vms_vector &b)
{
	const double x = (double(a.x) - b.x) / 65536.0;
	const double y = (double(a.y) - b.y) / 65536.0;
	const double z = (double(a.z) - b.z) / 65536.0;
	return std::sqrt(x * x + y * y + z * z);
}

/* Start at the exit trigger crossing, then follow opposite sides as the engine does */
inline json route(int wall_num)
{
	const auto &wall = Walls[wall_num];
	json result = { { "trigger", wall.trigger }, { "segment", wall.segnum }, { "side", wall.sidenum },
	                { "seconds", nullptr }, { "status", "invalid_exit" } };
	if (wall.segnum < 0 || wall.segnum > Highest_segment_index || wall.sidenum < 0 || wall.sidenum >= 6) return result;
	vms_vector position, target;
	compute_center_point_on_side(&position, &Segments[wall.segnum], wall.sidenum);
	int previous = wall.segnum, segment = Segments[previous].children[wall.sidenum];
	double units = 0;
	std::vector<bool> seen(Highest_segment_index + 1, false);
	for (int count = 0; count <= Highest_segment_index; ++count) {
		if (segment < 0 || segment > Highest_segment_index) return result;
		if (seen[segment]) { result["status"] = "cyclic_tunnel"; return result; }
		seen[segment] = true;
		int entry = -1;
		for (int side = 5; side >= 0; --side)
			if (Segments[segment].children[side] == previous) { entry = side; break; }
		if (entry < 0) { result["status"] = "disconnected_tunnel"; return result; }
		const int side = Side_opposite[entry];
		compute_center_point_on_side(&target, &Segments[segment], side);
		units += distance(position, target);
		const int next = Segments[segment].children[side];
		if (next == -2) {
			/* Match FLY_SPEED and SHORT_SEQUENCE: path / 50 plus 2 + 3 seconds outside */
			result["seconds"] = std::round((units / 50.0 + 5.0) * 1000.0) / 1000.0;
			result["status"] = "estimated";
			result["tunnel_units"] = std::round(units * 1000.0) / 1000.0;
			result["tunnel_segments"] = count + 1;
			return result;
		}
		previous = segment;
		segment = next;
		position = target;
	}
	result["status"] = "cyclic_tunnel";
	return result;
}

inline json collect(int level_num, const char *level_file)
{
	json video = { { "status", "not_applicable_d1" } };
	bool campaign_ending = false;
#ifdef DXX_BUILD_DESCENT_II
	video["status"] = "not_selected_for_level";
	if (Current_mission && PLAYING_BUILTIN_MISSION && level_num > 0 && level_num <= Last_level &&
	    level_file && !strcmp(level_file, Level_names[level_num - 1])) {
		campaign_ending = !is_SHAREWARE && !is_D2_OEM && level_num == Last_level;
		if (campaign_ending) video["status"] = "campaign_ending_instead";
		else if (const char *name = endlevel_movie_filename(level_num)) video = movie(name);
	}
#endif
	json routes = json::array();
	double seconds = -1;
	for (int i = 0; i < Num_walls; ++i) {
		const int trigger = Walls[i].trigger;
		if (trigger < 0 || trigger >= Num_triggers) continue;
#ifdef DXX_BUILD_DESCENT_II
		if (Triggers[trigger].type != TT_EXIT) continue;
#else
		if (!(Triggers[trigger].flags & TRIGGER_EXIT)) continue;
#endif
		auto path = route(i);
		if (path["seconds"].is_number()) seconds = (std::max)(seconds, path["seconds"].get<double>());
		routes.push_back(path);
	}

	const auto data = presentation(level_file, level_num);
	json result = { { "kind", "in_engine" }, { "seconds", nullptr },
	                { "status", seconds >= 0 ? "estimated" : "invalid_tunnel" } };
	if (seconds >= 0) result["seconds"] = seconds;
	if (data["status"] != "present") result["status"] = data["status"];
	if (data["status"] == "missing_endlevel_data")
		result["status"] = seconds >= 0 ? "tunnel_present_exterior_missing" : "no_valid_tunnel_exterior_missing";
	if (routes.empty()) result["status"] = "no_normal_exit_trigger";
	if (campaign_ending) result = unavailable("campaign_ending_instead");
	else if (video.contains("seconds") && (video["status"] != "missing_movie" || data["status"] != "present")) {
		result["kind"] = "video";
		result["status"] = video["status"];
		result["seconds"] = video["seconds"];
	}
	result["movie"] = video;
	result["exit_trigger"] = routes.empty() ? "absent" : "present";
	result["tunnel"] = { { "status", routes.empty() ? "no_exit_trigger" : seconds >= 0 ? "present" : "no_valid_route" },
	                     { "seconds", seconds >= 0 ? json(seconds) : json(nullptr) } };
	result["presentation"] = data;
	/* Only publish geometry estimates for an available in_engine presentation */
	if (result["status"] != "estimated") {
		result["tunnel"].erase("seconds");
		for (auto &path : routes) {
			path.erase("seconds");
			path.erase("tunnel_units");
			if (path["status"] == "estimated") path["status"] = "present";
		}
		if (result["status"] != "measured") {
			result["kind"] = "none";
			result["seconds"] = nullptr;
		}
	}
	result["routes"] = routes;
	return result;
}
}
#endif
