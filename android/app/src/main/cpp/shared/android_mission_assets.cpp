/* Android mission-owned mounts. See MissionLaunchPublication.kt for schema 1 */
#include "android_mission_assets.h"
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
}

namespace
{
struct context {
	std::string alias, descriptor, owner, revision, activation_error, key;
	std::vector<std::string> mounts;
};
std::vector<context> catalog;
std::vector<std::string> mounted;
int active = -1, pending = -1;
bool enabled, transitioning;
unsigned generation;

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
} // namespace

void android_mission_assets_shutdown(void)
{
	enabled = false;
}

void android_mission_assets_init(void)
{
	catalog.clear();
	mounted.clear();
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
		std::strcpy(resolved, candidate.descriptor.c_str());
		break;
	}
	if (pending < 0 && std::strncmp(descriptor, "missions/_packs/", 16) == 0) return 0;
	transitioning = true;
	android_mission_asset_reset_before();
	return 1;
}

int android_mission_assets_activate(void)
{
	if (!enabled) return 1;
	detach();
	if (pending >= 0) {
		const auto &candidate = catalog[pending];
		for (auto path = candidate.mounts.rbegin(); path != candidate.mounts.rend(); ++path) {
			if (PHYSFS_getMountPoint(path->c_str()) || !PHYSFS_mount(path->c_str(), nullptr, 0)) {
				char message[] = "Cannot activate mission assets: %s";
				Warning(message, path->c_str());
				detach();
				transitioning = false;
				android_mission_asset_reset_baseline();
				return 0;
			}
			mounted.push_back(*path);
		}
		active = pending;
	}
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

unsigned android_mission_assets_generation(void)
{
	return generation;
}

const char *android_mission_assets_key(void)
{
	return active < 0 ? "" : catalog[active].key.c_str();
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
