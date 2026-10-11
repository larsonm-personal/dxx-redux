#include "android_d1_mod_patches.h"
#include "android_mod_assets.h"
#include "input_demo_codec.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <climits>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
extern "C" {
#include "bm.h"
#include "console.h"
#include "gameseg.h"
#include "physfsx.h"
#include "wall.h"
#include "textures.h"
#ifdef DXX_BUILD_DESCENT_II
#include "d1_in_d2/d1_in_d2.h"
#include "d1_in_d2/d1_in_d2_assets.h"
#endif
}

using nlohmann::json;
namespace
{
const char properties_path[] = "patches/d1/properties_patch.rfc6902.json";
const char level_path[] = "patches/d1/level_surface_patch.rfc6902.json";

std::string patch_path(const char *path)
{
#ifdef DXX_BUILD_DESCENT_II
	return std::string("d1-mods/") + path;
#else
	return path;
#endif
}

bool enabled()
{
#ifdef DXX_BUILD_DESCENT_II
	return d1_in_d2_has_native_assets() != 0;
#else
	return true;
#endif
}

bool read_text(const std::string &path, std::string &text, size_t limit)
{
	PHYSFS_file *file = PHYSFS_openRead(path.c_str());
	if (!file) return false;
	try {
		const auto length = PHYSFS_fileLength(file);
		if (length < 0 || static_cast<unsigned long long>(length) > limit)
			throw std::runtime_error("file exceeds patch input limit");
		text.resize(static_cast<size_t>(length));
		if (length && PHYSFS_readBytes(file, &text[0], length) != length)
			throw std::runtime_error("truncated patch input");
		PHYSFS_close(file);
		return true;
	} catch (...) {
		PHYSFS_close(file);
		throw;
	}
}

int integer(const json &value, int low, int high)
{
	if (!value.is_number_integer()) throw std::runtime_error("patch requires integer");
	const auto number = value.get<long long>();
	if (number < low || number > high) throw std::runtime_error("patch integer out of range");
	return static_cast<int>(number);
}

json surface(int segment_index, int side_index)
{
	const auto &segment = Segments[segment_index];
	const auto &side = segment.sides[side_index];
	const unsigned overlay = static_cast<unsigned short>(side.tmap_num2);
	json uvls = json::array();
	for (const auto &uvl : side.uvls)
		uvls.push_back({ { "u", uvl.u / 32 }, { "v", uvl.v / 32 }, { "l", uvl.l / 2 } });
	return { { "segment", segment_index }, { "side", side_index }, { "child", segment.children[side_index] }, { "wall", side.wall_num }, { "rawTmap1", side.tmap_num | (overlay ? 0x8000 : 0) }, { "tmap1", side.tmap_num }, { "hasTmap2", overlay != 0 }, { "rawTmap2", overlay }, { "tmap2", overlay & 0x3fff }, { "orient", overlay & 0xc000 }, { "uvls", uvls } };
}

/* Test the unchanged source before staging any writes, as with composed HAM patches */
json stage_patch(const json &original, const json &operations)
{
	std::map<std::string, json> tested;
	for (const auto &op : operations) {
		const auto kind = op.at("op").get<std::string>();
		const auto path = op.at("path").get<std::string>();
		if (kind == "test") {
			if (original.at(json::json_pointer(path)) != op.at("value"))
				throw std::runtime_error("source test failed at " + path + ": expected " + op.at("value").dump() + ", found " + original.at(json::json_pointer(path)).dump());
			tested[path] = op.at("value");
		} else if (kind != "replace") {
			throw std::runtime_error("unsupported D1 patch operation");
		}
	}
	json staged = original;
	for (const auto &op : operations) {
		if (op.at("op") == "test") continue;
		const auto path = op.at("path").get<std::string>();
		if (!tested.count(path)) throw std::runtime_error("replacement has no source test");
		staged.at(json::json_pointer(path)) = op.at("value");
	}
	return staged;
}
} // namespace

int android_d1_mod_apply_properties(void)
{
	if (!enabled()) return 0;
	const auto path = patch_path(properties_path);
	try {
		std::string text;
		if (!read_text(path, text, 2 * 1024 * 1024)) return 0;
		const auto operations = json::parse(text);
		if (!operations.is_array()) throw std::runtime_error("patch must be an array");
		json original = { { "sections", { { "wclips", json::object() } } } };
		for (int i = 0; i < Num_wall_anims; ++i)
			original["sections"]["wclips"][std::to_string(i)] = { { "PlayTime", WallAnims[i].play_time } };
		for (const auto &op : operations) {
			const auto target = op.at("path").get<std::string>();
			bool supported = false;
			for (int i = 0; i < Num_wall_anims; ++i)
				supported |= target == "/sections/wclips/" + std::to_string(i) + "/PlayTime";
			if (!supported) throw std::runtime_error("unsupported D1 property path");
		}
		const auto staged = stage_patch(original, operations);
		std::vector<int> times;
		for (int i = 0; i < Num_wall_anims; ++i) {
			const auto &row = staged.at("sections").at("wclips").at(std::to_string(i));
			if (row.size() != 1) throw std::runtime_error("unsupported wall animation field");
			times.push_back(integer(row.at("PlayTime"), 0, INT_MAX));
			if (times.back() != WallAnims[i].play_time &&
			    (WallAnims[i].num_frames <= 0 || times.back() / WallAnims[i].num_frames == 0))
				throw std::runtime_error("wall frame duration is zero");
		}
		int applied = 0;
		for (int i = 0; i < Num_wall_anims; ++i) {
			applied += WallAnims[i].play_time != times[i];
			WallAnims[i].play_time = times[i];
		}
		android_mod_asset_loaded("metadata", path.c_str(), 0, 0);
		con_printf(CON_NORMAL, "DXA D1 properties: applied %d changes\n", applied);
		return applied;
	} catch (const std::exception &ex) {
		con_printf(CON_URGENT, "DXA D1 properties rejected: %s\n", ex.what());
		return -1;
	}
}

int android_d1_mod_apply_level(const char *filename)
{
	if (!enabled() || !filename) return 0;
	const auto path = patch_path(level_path);
	try {
		std::string text;
		if (!read_text(path, text, 2 * 1024 * 1024)) return 0;
		std::string name = filename;
		name = name.substr(name.find_last_of("/\\") + 1);
		for (auto &c : name)
			if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
		const std::string prefix = "/levels/" + name + "/";
		const auto all = json::parse(text);
		if (!all.is_array()) throw std::runtime_error("patch must be an array");
		json operations = json::array(), rows = json::object();
		std::map<std::string, std::pair<int, int>> indices;
		bool has_source_hash = false;
		for (const auto &op : all) {
			const auto target = op.at("path").get<std::string>();
			if (target.compare(0, prefix.size(), prefix) != 0) continue;
			operations.push_back(op);
			if (target == prefix + "sha256" && op.at("op") == "test") {
				has_source_hash = true;
				continue;
			}
			const auto &value = op.at("value");
			const int seg = integer(value.at("segment"), 0, Num_segments - 1);
			const int side = integer(value.at("side"), 0, 5);
			const auto key = std::to_string(seg) + ":" + std::to_string(side);
			if (target != prefix + "surfaces/" + key) throw std::runtime_error("unsupported surface path");
			indices[key] = { seg, side };
			rows[key] = surface(seg, side);
		}
		if (operations.empty()) return 0;
		if (!has_source_hash) throw std::runtime_error("level patch lacks source hash test");
		std::string bytes, hash, error;
		if (!read_text(filename, bytes, 16 * 1024 * 1024) ||
		    !input_demo_sha256_hex(reinterpret_cast<const unsigned char *>(bytes.data()), bytes.size(), &hash, &error))
			throw std::runtime_error("cannot hash level source");
		const json original = { { "levels", { { name, { { "sha256", hash }, { "surfaces", rows } } } } } };
		const auto staged = stage_patch(original, operations);
		const auto &changed = staged.at("levels").at(name).at("surfaces");
		for (const auto &[key, index] : indices) {
			auto row = changed.at(key);
			const int primary = integer(row.at("tmap1"), 0, NumTextures - 1);
			const int overlay = integer(row.at("tmap2"), 0, NumTextures - 1);
			const int orient = integer(row.at("orient"), 0, 0xc000);
			if (orient & 0x3fff) throw std::runtime_error("invalid overlay orientation bits");
			const int raw_overlay = overlay | orient;
			if (row.at("rawTmap1") != (primary | (raw_overlay ? 0x8000 : 0)) ||
			    row.at("rawTmap2") != raw_overlay || row.at("hasTmap2") != (raw_overlay != 0))
				throw std::runtime_error("inconsistent surface texture encoding");
			for (const auto field : { "rawTmap1", "tmap1", "hasTmap2", "rawTmap2", "tmap2", "orient" })
				row[field] = rows.at(key).at(field);
			if (row != rows.at(key)) throw std::runtime_error("surface patch may only change textures");
		}
		int applied = 0;
		for (const auto &[key, index] : indices) {
			const auto &row = changed.at(key);
			auto &side = Segments[index.first].sides[index.second];
			applied += row != rows.at(key);
			side.tmap_num = row.at("tmap1").get<short>();
			side.tmap_num2 = static_cast<short>(row.at("rawTmap2").get<unsigned short>());
		}
		android_mod_asset_loaded("metadata", path.c_str(), 0, 0);
		con_printf(CON_NORMAL, "DXA D1 level %s: applied %d surface changes\n", name.c_str(), applied);
		return applied;
	} catch (const std::exception &ex) {
		con_printf(CON_URGENT, "DXA D1 level patch rejected: %s\n", ex.what());
		return -1;
	}
}
