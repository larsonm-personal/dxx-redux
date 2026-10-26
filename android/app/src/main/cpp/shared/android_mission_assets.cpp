/* Android mission-owned mounts. See MissionLaunchPublication.kt for schema 1 */
#include "android_mission_assets.h"
#include "../../../../../XL_compat/scoped_texture.h"
#include <nlohmann/json.hpp>
#include <physfs.h>
#include <algorithm>
#include <cstdio>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <stdexcept>
#include <vector>
#include <sys/stat.h>

extern "C" {
#include "android_log.h"
#include "dxxerror.h"
#include "ignorecase.h"
}

namespace
{
struct context {
	std::string alias, descriptor, owner, revision, activation_error, key;
	std::vector<std::string> mounts;
};
std::vector<context> catalog;
std::vector<std::string> mounted;
std::vector<std::string> acquired;
int active = -1, pending = -1;
bool enabled, transitioning;
unsigned generation;
#if defined(__ANDROID__) && defined(INTROSPECT_ON) && defined(DXX_XL_RUNTIME)
bool test_fail_second_acquisition;
#endif

bool same_path(const std::string &a, const std::string &b)
{
	return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](unsigned char x, unsigned char y) {
		       return std::tolower(x) == std::tolower(y);
	       });
}

void detach()
{
	// Keep failed ownership records. Continuing with mixed contexts is unsafe
	while (!mounted.empty()) {
		if (!PHYSFS_unmount(mounted.back().c_str()))
			Error("Cannot release mission assets: %s: %s", mounted.back().c_str(), PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
		mounted.pop_back();
	}
	active = -1;
}

bool probe_mounts(const context &candidate)
{
	for (auto current = candidate.mounts.begin(); current != candidate.mounts.end(); ++current) {
		const auto &path = *current;
		const bool duplicate = std::find(candidate.mounts.begin(), current, path) != current;
		const bool attached = PHYSFS_getMountPoint(path.c_str()) != nullptr;
		const bool owned = std::find(mounted.begin(), mounted.end(), path) != mounted.end();
		// Published contexts are disjoint; only reselection may share owned mounts
		if (duplicate || (attached && (!owned || &candidate != &catalog[active]))) {
			char message[] = "Mission resource mount conflicts with another owner: %s";
			Warning(message, path.c_str());
			return false;
		}
		if (attached) continue;
		// Probe the actual archiver without exposing candidate files at the game root
		if (!PHYSFS_mount(path.c_str(), "__mission_asset_probe", 1)) {
			char message[] = "Cannot read mission resource archive or directory: %s";
			Warning(message, path.c_str());
			return false;
		}
		if (!PHYSFS_unmount(path.c_str()))
			Error("Cannot release mission resource probe: %s", path.c_str());
	}
	return true;
}
} // namespace

void android_mission_assets_shutdown(void)
{
	enabled = false;
}

void android_mission_assets_init(void)
{
	catalog.clear();
	mounted.clear();
	acquired.clear();
	active = pending = -1;
	generation = 0;
	transitioning = enabled = false;
	const char *preview = std::getenv("DXX_ANDROID_LEVEL_PREVIEW_DATA_DIR");
	if (preview && *preview) return;
	const char *write = PHYSFS_getWriteDir();
	if (!write) return;
	std::ifstream input(std::string(write) + "/.mission_assets.json");
	if (!input) return;
	try {
		const auto document = nlohmann::json::parse(input);
		if (document.at("schema") != 1) throw std::runtime_error("unsupported catalog schema");
		for (const auto &item : document.at("entries")) {
			context candidate;
			candidate.alias = item.at("alias").get<std::string>();
			candidate.key = item.at("key").get<std::string>();
			candidate.descriptor = item.at("descriptor").get<std::string>();
			candidate.owner = item.at("owner").get<std::string>();
			candidate.revision = item.at("revision").get<std::string>();
			candidate.activation_error = item.value("activation_error", "");
			candidate.mounts = item.at("mounts").get<std::vector<std::string>>();
			if (candidate.mounts.empty() || candidate.alias.compare(0, 16, "missions/_packs/") != 0)
				throw std::runtime_error("invalid mission context");
			catalog.push_back(std::move(candidate));
		}
		enabled = true;
		debug_log_force(DLOG_GAME, "[ASSETS] catalog missions=%zu", catalog.size());
	} catch (const std::exception &failure) {
		Error("Cannot read mission asset catalog: %s", failure.what());
	}
}

int android_mission_assets_prepare(const char *descriptor, char *resolved, size_t capacity)
{
	if (!descriptor || !resolved || std::strlen(descriptor) >= capacity) return 0;
	std::strcpy(resolved, descriptor);
	if (!enabled) return 1;
	if (transitioning) return 0;
	pending = -1;
	for (size_t i = 0; i < catalog.size(); ++i) {
		if (!same_path(catalog[i].alias, descriptor)) continue;
		pending = static_cast<int>(i);
		const auto &candidate = catalog[i];
		if (!candidate.activation_error.empty()) {
			char message[] = "%s";
			Warning(message, candidate.activation_error.c_str());
			return 0;
		}
		if (candidate.descriptor.size() >= capacity) return 0;
		struct stat info;
		for (const auto &path : candidate.mounts) {
			if (stat(path.c_str(), &info) != 0) {
				char message[] = "Mission resource missing: %s";
				Warning(message, path.c_str());
				return 0;
			}
		}
		const std::string file = candidate.mounts.front() + "/" + candidate.descriptor;
		if (stat(file.c_str(), &info) != 0) {
			char message[] = "Mission descriptor missing: %s";
			Warning(message, file.c_str());
			return 0;
		}
		if (!probe_mounts(candidate)) return 0;
		std::strcpy(resolved, candidate.descriptor.c_str());
		break;
	}
	if (pending < 0 && std::strncmp(descriptor, "missions/_packs/", 16) == 0) return 0;
	// Acquire every fallible candidate mount before freeing live graphics/audio data
	// Reserve ownership bookkeeping before mounting so allocation cannot lose a lease
	if (pending >= 0 && pending != active) {
		const auto &candidate = catalog[pending];
		auto ownership = candidate.mounts;
		std::reverse(ownership.begin(), ownership.end());
#ifdef DXX_XL_DESCRIPTOR_TEST
		extern void xl_test_asset_mounts_probed(void);
		xl_test_asset_mounts_probed();
#endif
		size_t count = 0;
		for (const auto &path : ownership) {
			bool injected_failure = false;
#if defined(__ANDROID__) && defined(INTROSPECT_ON) && defined(DXX_XL_RUNTIME)
			injected_failure = test_fail_second_acquisition && count == 1;
			if (injected_failure) test_fail_second_acquisition = false;
#endif
			if (injected_failure || PHYSFS_getMountPoint(path.c_str()) || !PHYSFS_mount(path.c_str(), nullptr, 0)) {
				while (count) {
					const auto &previous = ownership[--count];
					if (!PHYSFS_unmount(previous.c_str()))
						Error("Cannot release candidate mission assets: %s", previous.c_str());
				}
				char message[] = "Cannot acquire mission assets; previous mission retained: %s";
				Warning(message, path.c_str());
				pending = -1;
				return 0;
			}
			++count;
		}
		acquired.swap(ownership);
	}
	transitioning = true;
	android_mission_asset_reset_before();
	return 1;
}

#if defined(__ANDROID__) && defined(INTROSPECT_ON) && defined(DXX_XL_RUNTIME)
int android_mission_assets_test_acquisition_failure(void)
{
	if (!enabled || transitioning || !PHYSFS_getWriteDir()) return 0;
	const auto search_path = [] {
		std::vector<std::string> result;
		char **paths = PHYSFS_getSearchPath();
		if (!paths) throw std::runtime_error("Cannot inspect mission search path");
		for (char **p = paths; *p; ++p) result.emplace_back(*p);
		PHYSFS_freeList(paths);
		return result;
	};
	const auto before_paths = search_path();
	const auto before_generation = generation;
	const auto before_active = active;
	const auto before_mounted = mounted;
	const std::string root = std::string(PHYSFS_getWriteDir()) + "/../xl-water-runtime-backup/mount-probe";
	context candidate;
	candidate.alias = "missions/_packs/__acquisition_failure__/test.mn2";
	candidate.descriptor = "test.mn2";
	candidate.owner = "automation acquisition failure";
	candidate.mounts = { root, root + "/tail" };
	catalog.push_back(std::move(candidate));
	test_fail_second_acquisition = true;
	char resolved[1024];
	const int accepted = android_mission_assets_prepare(catalog.back().alias.c_str(), resolved, sizeof(resolved));
	const bool injected = !test_fail_second_acquisition;
	test_fail_second_acquisition = false;
	// A missing fault would have begun teardown; never silently report that as success
	if (accepted) Error("Acquisition failure probe unexpectedly began resource reset");
	catalog.pop_back();
	pending = -1;
	return injected && !transitioning && acquired.empty() && generation == before_generation &&
	       active == before_active && mounted == before_mounted && search_path() == before_paths;
}
#endif

int android_mission_assets_activate(void)
{
	if (!enabled) return 1;
	// Only a successful prepare authorizes destructive context replacement
	if (!transitioning) return 0;
	if (pending != active) {
		detach();
		mounted.swap(acquired);
		active = pending;
	}
	pending = -1;
	transitioning = false;
	android_mission_asset_reset_baseline();
	++generation;
	debug_log_force(DLOG_GAME, "[ASSETS] activated generation=%u owner='%s' mounts=%zu", generation, android_mission_assets_owner(), mounted.size());
	return 1;
}

void android_mission_assets_free_begin(void)
{
	if (enabled && !transitioning) android_mission_asset_reset_before();
}

void android_mission_assets_free_end(void)
{
	if (!enabled || transitioning) return;
	detach();
	++generation;
	android_mission_asset_reset_baseline();
	debug_log_force(DLOG_GAME, "[ASSETS] restored base generation=%u", generation);
}

int android_mission_assets_discover(const char *descriptor)
{
	if (!enabled) return 1;
	const char *origin = PHYSFS_getRealDir(descriptor);
	if (!origin) return 1;
	for (const auto &path : mounted)
		if (path == origin) return 0;
	return 1;
}

const char *android_mission_assets_owner(void)
{
	return active < 0 ? "base" : catalog[active].owner.c_str();
}

int android_mission_assets_descriptor_source(const char *alias, char *path, size_t capacity)
{
	if (!enabled) return 0;
	for (const auto &candidate : catalog) {
		if (!same_path(candidate.alias, alias)) continue;
		const auto source = candidate.mounts.front() + "/" + candidate.descriptor;
		if (source.size() >= capacity) return 0;
		std::memcpy(path, source.c_str(), source.size() + 1);
		return 1;
	}
	return 0;
}

unsigned android_mission_assets_generation(void)
{
	return generation;
}

int android_mission_assets_find_owned_file(const char *path, char *resolved, size_t capacity)
{
	if (!enabled || active < 0 || transitioning || !path || !resolved || !capacity) return 0;
	const std::string requested(path);
	if (requested.empty() || requested.size() >= capacity || requested.size() >= 4096) return 0;
	std::size_t start = 0;
	for (std::size_t i = 0; i <= requested.size(); ++i) {
		if (i < requested.size() && requested[i] != '/') {
			const unsigned char c = requested[i];
			if (c < 32 || c == 127 || c == '\\' || c == ':') return 0;
			continue;
		}
		const auto part = requested.substr(start, i - start);
		if (part.empty() || part == "." || part == "..") return 0;
		start = i + 1;
	}
	std::vector<char> corrected(requested.begin(), requested.end());
	corrected.push_back(0);
	if (PHYSFSEXT_locateCorrectCase(corrected.data()) != 0) return 0;
	const char *origin = PHYSFS_getRealDir(corrected.data());
	if (!origin || std::find(mounted.begin(), mounted.end(), origin) == mounted.end()) return 0;
	PHYSFS_Stat info;
	if (!PHYSFS_stat(corrected.data(), &info) || info.filetype != PHYSFS_FILETYPE_REGULAR) return 0;
	std::memcpy(resolved, corrected.data(), corrected.size());
	return 1;
}

const char *android_mission_assets_key(void)
{
	return active < 0 ? "" : catalog[active].key.c_str();
}

const char *android_mission_assets_context_json(void)
{
	static std::string snapshot;
	snapshot.clear();
	if (active >= 0) {
		const auto &item = catalog[active];
		snapshot = nlohmann::json({ { "key", item.key }, { "revision", item.revision }, { "descriptor", item.descriptor }, { "mounts", item.mounts } }).dump();
	}
	return snapshot.c_str();
}

int android_mission_assets_resolve_key(const char *key, char *mission_path, size_t capacity)
{
	for (const auto &candidate : catalog) {
		if (candidate.key != key) continue;
		const auto path = candidate.alias.substr(9, candidate.alias.size() - 13);
		if (path.size() >= capacity) return 0;
		std::strcpy(mission_path, path.c_str());
		return 1;
	}
	return 0;
}

int android_mission_assets_find_static_texture(const char *hog, const char *bitmap, char *resolved, size_t capacity)
{
	const auto path = xl_compat::static_mission_texture_path(hog, bitmap);
	return !path.empty() && android_mission_assets_find_owned_file(path.c_str(), resolved, capacity);
}

int android_mission_assets_find_animation_texture(const char *hog, const char *bitmap, char *resolved, size_t capacity)
{
	const auto path = xl_compat::animation_mission_texture_path(hog, bitmap);
	return !path.empty() && android_mission_assets_find_owned_file(path.c_str(), resolved, capacity);
}
