/* Only archive identity: this is not an attestation of mods, saves or engine state */
#include "input_demo_assets.h"
#include "input_demo_recorder.h"
#include "input_demo_replay.h"
#include "picosha2.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <sys/stat.h>
extern "C" {
#include "physfsx.h"
#include "mission.h"
#include "gameseq.h"
#ifdef DXX_BUILD_DESCENT_II
#include "d1_in_d2/d1_in_d2.h"
#endif
}
namespace
{
using json = nlohmann::ordered_json;
struct source {
	std::string path, origin, name, digest;
	uint64_t size = 0;
	bool archive = false;
};
std::map<std::string, source> sources;
std::string captured;
bool extra_ham_loaded;
std::string source_path(const std::string &path)
{
	std::string result = path;
#ifdef _WIN32
	// PHYSFS preserves mount spelling; Windows callers use both separators
	std::replace(result.begin(), result.end(), '\\', '/');
#endif
	return result;
}
std::string basename(const std::string &path)
{
	return path.substr(path.find_last_of("/\\") + 1);
}
// Read the same handle used by the loader, restoring its cursor before returning
bool hash_stream(PHYSFS_file *file, std::string &digest, uint64_t &size)
{
	if (!file) return false;
	const auto position = PHYSFS_tell(file), length = PHYSFS_fileLength(file);
	if (position < 0 || length < 0 || !PHYSFS_seek(file, 0)) return false;
	picosha2::hash256_one_by_one hash;
	unsigned char buffer[65536];
	bool valid = true;
	for (PHYSFS_sint64 left = length; valid && left > 0;) {
		const auto bytes = static_cast<PHYSFS_uint32>(left < static_cast<PHYSFS_sint64>(sizeof(buffer)) ? left : sizeof(buffer));
		valid = PHYSFS_read(file, buffer, 1, bytes) == bytes;
		if (valid) hash.process(buffer, buffer + bytes);
		left -= bytes;
	}
	valid = PHYSFS_seek(file, position) && valid;
	if (!valid) return false;
	hash.finish();
	digest = picosha2::get_hash_hex_string(hash);
	size = length;
	return true;
}
bool hash_archive(const std::string &path, std::string &digest, uint64_t &size)
{
	FILE *file = std::fopen(path.c_str(), "rb");
	if (!file) return false;
	picosha2::hash256_one_by_one hash;
	unsigned char buffer[65536];
	size = 0;
	for (size_t bytes; (bytes = std::fread(buffer, 1, sizeof(buffer), file)) != 0;) {
		hash.process(buffer, buffer + bytes);
		size += bytes;
	}
	const bool valid = !std::ferror(file);
	const bool closed = std::fclose(file) == 0;
	if (!valid || !closed) return false;
	hash.finish();
	digest = picosha2::get_hash_hex_string(hash);
	return true;
}
bool unchanged(const source &entry)
{
	if (entry.digest.empty()) return false;
	std::string digest;
	uint64_t size = 0;
	bool ok;
	if (entry.archive) ok = hash_archive(entry.path, digest, size);
	else {
		const char *origin = PHYSFS_getRealDir(entry.path.c_str());
		if (!origin || entry.origin != origin) return false;
		PHYSFS_file *file = PHYSFS_openRead(entry.path.c_str());
		ok = hash_stream(file, digest, size);
		if (file) ok = PHYSFS_close(file) && ok;
	}
	return ok && digest == entry.digest && size == entry.size;
}
} // namespace
void input_demo_assets_extra_ham_loaded(void)
{
	extra_ham_loaded = true;
	input_demo_recorder_invalidate_assets("additional mission HAM was loaded during recording");
}
void input_demo_assets_forget(const char *role)
{
	sources.erase(role);
}
void input_demo_assets_note_file(const char *role, const char *path, PHYSFS_file *file)
{
	input_demo_recorder_set_asset_provider(input_demo_assets_capture);
	source entry;
	char resolved[PATH_MAX];
	std::snprintf(resolved, sizeof(resolved), "%s", path ? path : "");
	if (path) PHYSFSEXT_locateCorrectCase(resolved);
	entry.path = resolved;
	entry.name = basename(entry.path);
	const char *origin = path ? PHYSFS_getRealDir(resolved) : nullptr;
	if (origin) entry.origin = origin;
	if (origin && !hash_stream(file, entry.digest, entry.size)) entry.digest.clear();
	input_demo_recorder_note_asset(role, entry.digest.c_str(), entry.size);
	sources[role] = entry;
	if (!std::strcmp(role, "d2_ham")) extra_ham_loaded = false;
}
void input_demo_assets_note_archive(const char *role, const char *path)
{
	input_demo_recorder_set_asset_provider(input_demo_assets_capture);
	const auto archive_path = source_path(path);
	// A repeated mount does not reload already resident bytes
	if (sources.count(role) && sources[role].path == archive_path) {
		if (input_demo_recorder_is_active() && !unchanged(sources[role])) input_demo_recorder_invalidate_assets("archive source changed during recording");
		return;
	}
	source entry;
	entry.archive = true;
	entry.path = archive_path;
	entry.name = basename(path);
	if (!hash_archive(entry.path, entry.digest, entry.size)) entry.digest.clear();
	input_demo_recorder_note_asset(role, entry.digest.c_str(), entry.size);
	sources[role] = entry;
}
int input_demo_assets_mount(const char *role, const char *path, int append)
{
	char resolved[PATH_MAX], actual[PATH_MAX];
	std::snprintf(resolved, sizeof(resolved), "%s", path);
	PHYSFSEXT_locateCorrectCase(resolved);
	if (!PHYSFSX_getRealPath(resolved, actual)) return 0;
	const bool already_mounted = PHYSFS_getMountPoint(actual) != nullptr;
	if (!PHYSFS_addToSearchPath(actual, append)) return 0;
	if (!already_mounted) input_demo_assets_forget(role);
	input_demo_assets_note_archive(role, actual);
	return 1;
}
const char *input_demo_assets_capture(void)
{
	bool d1 = true, companion = false;
	const char *mode = "native_d1";
#ifdef DXX_BUILD_DESCENT_II
	d1 = d1_in_d2_use_d1_gameplay() != 0;
	mode = d1 ? "d1_in_d2" : "native_d2";
	unsigned char identity[64];
	companion = d1 && d1_in_d2_capture_asset_identity(identity) == 64;
#endif
	json root = { { "status", "complete" }, { "mode", mode }, { "files", json::array() } };
	auto add = [&](const char *role) {
		const auto it = sources.find(role);
		if (it == sources.end() || !unchanged(it->second)) {
			root["status"] = "unknown";
			if (!root.contains("error")) root["error"] = std::string("missing, changed or unreadable source: ") + role;
			return;
		}
		const auto &entry = it->second;
		json file = { { "role", role }, { "name", entry.name }, { "size", entry.size }, { "sha256", entry.digest } };
		if (!entry.archive) {
			struct stat info;
			if (stat(entry.origin.c_str(), &info) != 0 || (info.st_mode & S_IFMT) != S_IFDIR) {
				// Only a verified HOG already in this snapshot can own a member fingerprint
				for (const auto &container : root["files"]) {
					const auto found = sources.find(container["role"].get<std::string>());
					if (found != sources.end() && found->second.archive && found->second.path == source_path(entry.origin)) file["container"] = container["role"];
				}
				if (!file.contains("container")) {
					root["status"] = "unknown";
					root["error"] = std::string("untracked archive source for ") + role;
					return;
				}
			}
		}
		root["files"].push_back(file);
	};
	if (d1) add("d1_hog");
	if (!d1 || companion) add("d2_hog");
	if (sources.count("mission_hog")) add("mission_hog");
	if (d1) add("d1_pig");
	if (!d1 || companion) {
		add("d2_pig");
		add("d2_ham");
	}
	if (!Current_mission || !Current_level_num ||
	    (Current_level_num > 0 && (!Level_names || Current_level_num > Last_level)) ||
	    (Current_level_num < 0 && (!Secret_level_names || Current_level_num < Last_secret_level))) {
		root["status"] = "unknown";
		root["error"] = "no loaded mission level";
	} else {
		char level[PATH_MAX];
		std::snprintf(level, sizeof(level), "%s", Current_level_num > 0 ? Level_names[Current_level_num - 1] : Secret_level_names[-Current_level_num - 1]);
		PHYSFSEXT_locateCorrectCase(level);
		const char *owner = PHYSFS_getRealDir(level);
		const char *role = sources.count("mission_hog") ? "mission_hog" : d1 ? "d1_hog"
		                                                                     : "d2_hog";
		if (!owner || !sources.count(role) || sources[role].path != source_path(owner)) {
			root["status"] = "unknown";
			root["error"] = "level source is outside the recorded HOG";
		}
	}
	if (!d1 && extra_ham_loaded) {
		root["status"] = "unknown";
		root["error"] = "additional mission HAM is not fingerprinted";
	}
	captured = root.dump();
	return captured.c_str();
}
int input_demo_assets_check_replay(char *error, unsigned int capacity)
{
	const char *expected_text = input_demo_replay_asset_fingerprints();
	if (!expected_text || !*expected_text) return 1;
	const auto expected = json::parse(expected_text);
	if (expected["status"] != "complete") return 1;
	const auto actual = json::parse(input_demo_assets_capture());
	const bool imported = expected["mode"] == "native_d1" && actual["mode"] == "d1_in_d2";
	bool valid = actual["status"] == "complete" && (imported || expected["mode"] == actual["mode"]) &&
	             (imported || expected["files"].size() == actual["files"].size());
	for (const auto &entry : expected["files"]) {
		bool found = false;
		for (const auto &candidate : actual["files"])
			if (entry["role"] == candidate["role"] && entry["size"] == candidate["size"] && entry["sha256"] == candidate["sha256"]) found = true;
		valid = valid && found;
	}
	if (valid && imported && expected["files"].size() != actual["files"].size())
		std::printf("Input demo D1-in-D2: D1 fingerprints match; additional loaded D2 dependencies were not recorded\n");
	if (!valid && error && capacity) std::snprintf(error, capacity, "recorded HOG/PIG/HAM fingerprints do not match loaded sources");
	return valid;
}
