#ifdef ANDROID

#include <jni.h>

#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <algorithm>
#include <cerrno>
#include <string>
#include <vector>
#include <strings.h>
#include <sys/stat.h>

#include <nlohmann/json.hpp>

extern "C" {
#include "android_save_meta.h"
}

using nlohmann::json;

static bool is_save_slot_name(const char *name)
{
	size_t len;

	if (!name)
		return false;
	len = strlen(name);
	if (len < 4)
		return false;
	return name[len - 4] == '.' &&
	       (name[len - 3] == 's' || name[len - 3] == 'S' ||
	        name[len - 3] == 'm' || name[len - 3] == 'M') &&
	       (name[len - 2] == 'g' || name[len - 2] == 'G') &&
	       name[len - 1] >= '0' && name[len - 1] <= '9';
}

static void collect_save_paths_recursive(const char *dir_path,
                                         int depth,
                                         std::vector<std::string> *paths)
{
	DIR *dp;
	struct dirent *ent;

	if (!dir_path || !paths || depth > 8)
		return;

	dp = opendir(dir_path);
	if (!dp)
		return;
	while ((ent = readdir(dp)) != NULL) {
		char path[ANDROID_SAVE_META_PATH_LEN];
		struct stat st;
		int path_wrote;

		if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, ".."))
			continue;
		path_wrote = snprintf(path, sizeof(path), "%s/%s", dir_path, ent->d_name);
		if (path_wrote <= 0 || path_wrote >= (int) sizeof(path))
			continue;
		if (is_save_slot_name(ent->d_name)) {
			paths->emplace_back(path);
			continue;
		}
		if (stat(path, &st) == 0 && S_ISDIR(st.st_mode))
			collect_save_paths_recursive(path, depth + 1, paths);
	}
	closedir(dp);
}

static void collect_save_paths(const char *files_dir,
                               const char *subdir,
                               std::vector<std::string> *paths)
{
	char dir_path[ANDROID_SAVE_META_PATH_LEN];
	int wrote;

	if (!files_dir || !subdir || !paths)
		return;
	wrote = snprintf(dir_path, sizeof(dir_path), "%s/%s", files_dir, subdir);
	if (wrote <= 0 || wrote >= (int) sizeof(dir_path))
		return;
	collect_save_paths_recursive(dir_path, 0, paths);
}

static std::string sanitize_text(const char *text)
{
	std::string out;

	if (!text)
		return out;

	for (const unsigned char *p = reinterpret_cast<const unsigned char *>(text); *p; ++p) {
		unsigned char ch = *p;

		if (ch >= 32 && ch <= 126)
			out.push_back((char) ch);
		else if (ch == '\t' || ch == '\n' || ch == '\r')
			out.push_back(' ');
		else
			out.push_back('?');
	}
	while (!out.empty() && out.back() == ' ')
		out.pop_back();
	return out;
}

static std::string relative_to_files_dir(const char *files_dir, const char *path)
{
	size_t prefix_len;
	const char *anchor;

	if (!files_dir || !path)
		return std::string();
	prefix_len = strlen(files_dir);
	if (strncmp(files_dir, path, prefix_len) != 0)
		goto find_anchor;
	if (path[prefix_len] == '/')
		return std::string(path + prefix_len + 1);
	if (path[prefix_len] == '\0')
		return std::string();

find_anchor:
	anchor = strstr(path, "/d1x-redux/");
	if (anchor)
		return std::string(anchor + 1);
	anchor = strstr(path, "/d2x-redux/");
	if (anchor)
		return std::string(anchor + 1);
	return std::string();
}

static int slot_from_path(const char *path)
{
	const char *dot;

	if (!path)
		return -1;
	dot = strrchr(path, '.');
	if (!dot || !dot[1] || !dot[2] || !dot[3] || dot[4])
		return -1;
	if ((dot[1] != 's' && dot[1] != 'S' && dot[1] != 'm' && dot[1] != 'M') ||
	    (dot[2] != 'g' && dot[2] != 'G') ||
	    dot[3] < '0' || dot[3] > '9')
		return -1;
	return dot[3] - '0';
}

static bool save_path_is_coop(const char *path)
{
	const char *dot;

	if (!path)
		return false;
	dot = strrchr(path, '.');
	return dot && (dot[1] == 'm' || dot[1] == 'M') &&
	       (dot[2] == 'g' || dot[2] == 'G') &&
	       dot[3] >= '0' && dot[3] <= '9' && dot[4] == '\0';
}

static const char *game_name(uint8_t game_id)
{
	return game_id == ANDROID_SAVE_META_GAME_D1 ? "d1" : "d2";
}

static const char *save_kind_name(uint8_t save_kind)
{
	switch (save_kind) {
		case ANDROID_SAVE_META_KIND_AUTO_MINIMIZE:
			return "auto_minimize";
		case ANDROID_SAVE_META_KIND_AUTO_EXIT:
			return "auto_exit";
		case ANDROID_SAVE_META_KIND_AUTO_PROGRESS:
			return "auto_progress";
		case ANDROID_SAVE_META_KIND_AUTO_ABORT:
			return "auto_abort";
		case ANDROID_SAVE_META_KIND_AUTO_PERIODIC:
			return "auto_periodic";
		default:
			return "manual";
	}
}

static std::string callsign_from_path(const char *path)
{
	const char *slash;
	const char *dot;

	if (!path)
		return std::string();
	slash = strrchr(path, '/');
	path = slash ? slash + 1 : path;
	dot = strrchr(path, '.');
	if (!dot || dot == path)
		return std::string();
	return std::string(path, dot - path);
}

static void copy_meta_text(char *dest, size_t dest_size, const std::string &text)
{
	if (!dest || dest_size == 0)
		return;
	strncpy(dest, text.c_str(), dest_size - 1);
	dest[dest_size - 1] = '\0';
}

static uint64_t file_mtime_seconds(const char *path)
{
	struct stat st;

	if (!path || stat(path, &st) != 0)
		return 0;
	return (uint64_t) st.st_mtime;
}

static uint64_t file_size_bytes(const char *path)
{
	struct stat st;

	if (!path || stat(path, &st) != 0)
		return 0;
	return (uint64_t) st.st_size;
}

static bool is_sentinel_callsign(const std::string &callsign)
{
	return strcasecmp(callsign.c_str(), "coopsave") == 0;
}

static bool read_resume_candidate(const std::string &path,
                                  android_save_meta_candidate *out)
{
	android_save_meta_disk meta;
	std::string callsign;

	if (!out || !android_save_meta_read_path(path.c_str(), &meta))
		return false;
	if (meta.wall_clock_unix_seconds == 0)
		meta.wall_clock_unix_seconds = file_mtime_seconds(path.c_str());
	callsign = sanitize_text(meta.callsign);
	if (callsign.empty()) {
		callsign = callsign_from_path(path.c_str());
		copy_meta_text(meta.callsign, sizeof(meta.callsign), callsign);
	}
	if (callsign.empty() || is_sentinel_callsign(callsign))
		return false;
	memset(out, 0, sizeof(*out));
	strncpy(out->path, path.c_str(), sizeof(out->path) - 1);
	out->meta = meta;
	return true;
}

static bool candidate_is_newer(const android_save_meta_candidate &candidate,
                               const android_save_meta_candidate &best)
{
	auto save_kind_priority = [](uint8_t save_kind) {
		switch (save_kind) {
			case ANDROID_SAVE_META_KIND_AUTO_ABORT:
				return 6;
			case ANDROID_SAVE_META_KIND_AUTO_EXIT:
				return 5;
			case ANDROID_SAVE_META_KIND_AUTO_MINIMIZE:
				return 4;
			case ANDROID_SAVE_META_KIND_AUTO_PROGRESS:
				return 3;
			case ANDROID_SAVE_META_KIND_AUTO_PERIODIC:
				return 2;
			default:
				return 1;
		}
	};
	int candidate_priority;
	int best_priority;

	if (candidate.meta.wall_clock_unix_seconds != best.meta.wall_clock_unix_seconds)
		return candidate.meta.wall_clock_unix_seconds > best.meta.wall_clock_unix_seconds;
	candidate_priority = save_kind_priority(candidate.meta.save_kind);
	best_priority = save_kind_priority(best.meta.save_kind);
	if (candidate_priority != best_priority)
		return candidate_priority > best_priority;
	return strcmp(candidate.path, best.path) < 0;
}

static bool candidate_is_higher_progress(const android_save_meta_candidate &candidate,
                                         const android_save_meta_candidate &best)
{
	if (candidate.meta.level_num != best.meta.level_num)
		return candidate.meta.level_num > best.meta.level_num;
	if (candidate.meta.total_seconds != best.meta.total_seconds)
		return candidate.meta.total_seconds > best.meta.total_seconds;
	if (candidate.meta.level_seconds != best.meta.level_seconds)
		return candidate.meta.level_seconds > best.meta.level_seconds;
	return candidate_is_newer(candidate, best);
}

static bool select_newest_resume_save(const std::vector<std::string> &paths,
                                      android_save_meta_candidate *out)
{
	android_save_meta_candidate best;
	bool found = false;

	if (!out)
		return false;
	memset(&best, 0, sizeof(best));
	for (const auto &path : paths) {
		android_save_meta_candidate candidate;

		if (!read_resume_candidate(path, &candidate))
			continue;
		if (!found || candidate_is_newer(candidate, best)) {
			best = candidate;
			found = true;
		}
	}
	if (!found)
		return false;
	*out = best;
	return true;
}

static bool select_resume_save_by_kind(const std::vector<std::string> &paths,
                                       int save_kind,
                                       int use_progress_order,
                                       android_save_meta_candidate *out)
{
	android_save_meta_candidate best;
	bool found = false;

	if (!out)
		return false;
	memset(&best, 0, sizeof(best));
	for (const auto &path : paths) {
		android_save_meta_candidate candidate;

		if (!read_resume_candidate(path, &candidate))
			continue;
		if (candidate.meta.save_kind != save_kind)
			continue;
		if (!found ||
		    (use_progress_order && candidate_is_higher_progress(candidate, best)) ||
		    (!use_progress_order && candidate_is_newer(candidate, best))) {
			best = candidate;
			found = true;
		}
	}
	if (!found)
		return false;
	*out = best;
	return true;
}

static json resume_candidate_json(const char *files_dir,
                                  const android_save_meta_candidate &candidate)
{
	json out;

	out["path"] = candidate.path;
	out["relative_path"] = relative_to_files_dir(files_dir, candidate.path);
	out["game"] = game_name(candidate.meta.game_id);
	out["save_kind"] = save_kind_name(candidate.meta.save_kind);
	out["save_time_unix_seconds"] = candidate.meta.wall_clock_unix_seconds;
	out["callsign"] = sanitize_text(candidate.meta.callsign);
	out["description"] = sanitize_text(candidate.meta.description);
	out["mission_name"] = sanitize_text(candidate.meta.mission_name);
	out["level_num"] = candidate.meta.level_num;
	out["level_name"] = sanitize_text(candidate.meta.level_name);
	out["level_seconds"] = candidate.meta.level_seconds;
	out["total_seconds"] = candidate.meta.total_seconds;
	out["difficulty_changed"] = candidate.meta.difficulty_changed != 0;
	out["difficulty_min"] = candidate.meta.difficulty_min;
	out["difficulty_max"] = candidate.meta.difficulty_max;
	out["slot"] = slot_from_path(candidate.path);
	out["has_thumbnail"] =
	    candidate.meta.thumbnail_format == ANDROID_SAVE_META_THUMB_RGB6 &&
	    candidate.meta.thumbnail_width == ANDROID_SAVE_META_THUMB_W &&
	    candidate.meta.thumbnail_height == ANDROID_SAVE_META_THUMB_H;
	out["thumbnail_width"] = candidate.meta.thumbnail_width;
	out["thumbnail_height"] = candidate.meta.thumbnail_height;
	out["metadata_backed"] = android_save_meta_is_valid(&candidate.meta) != 0;
	return out;
}

static std::vector<std::string> split_path(const std::string &path)
{
	std::vector<std::string> out;
	size_t start = 0;

	while (start <= path.size()) {
		size_t slash = path.find('/', start);
		size_t end = slash == std::string::npos ? path.size() : slash;
		if (end > start)
			out.emplace_back(path.substr(start, end - start));
		if (slash == std::string::npos)
			break;
		start = slash + 1;
	}
	return out;
}

static std::string game_name_from_relative(const std::string &relative_path)
{
	if (relative_path.rfind("d1x-redux/", 0) == 0 || relative_path == "d1x-redux")
		return "d1";
	if (relative_path.rfind("d2x-redux/", 0) == 0 || relative_path == "d2x-redux")
		return "d2";
	return std::string();
}

static void save_set_parts_from_relative(const std::string &relative_path,
                                         std::string *scope,
                                         std::string *pilot,
                                         std::string *mission)
{
	std::vector<std::string> parts = split_path(relative_path);

	if (scope)
		scope->clear();
	if (pilot)
		pilot->clear();
	if (mission)
		mission->clear();
	if (parts.size() >= 7 && parts[1] == "Players" && parts[2] == "save_sets" &&
	    parts[3] == "single") {
		if (scope)
			*scope = "single";
		if (pilot)
			*pilot = parts[4];
		if (mission)
			*mission = parts[5];
		return;
	}
	if (parts.size() >= 6 && parts[1] == "Players" && parts[2] == "save_sets" &&
	    parts[3] == "coop") {
		if (scope)
			*scope = "coop";
		if (pilot)
			*pilot = "coopsave";
		if (mission)
			*mission = parts[4];
	}
}

static bool path_is_under_game_root(const char *files_dir, const char *path)
{
	char prefix[ANDROID_SAVE_META_PATH_LEN];
	int wrote;
	size_t prefix_len;

	if (!files_dir || !path)
		return false;
	for (const char *root : { "d1x-redux", "d2x-redux" }) {
		wrote = snprintf(prefix, sizeof(prefix), "%s/%s/", files_dir, root);
		if (wrote <= 0 || wrote >= (int) sizeof(prefix))
			continue;
		prefix_len = strlen(prefix);
		if (strncmp(path, prefix, prefix_len) == 0)
			return true;
	}
	return false;
}

static json save_explorer_slot_json(const char *files_dir, const std::string &path)
{
	android_save_meta_candidate candidate;
	android_save_meta_disk meta;
	std::string relative_path = relative_to_files_dir(files_dir, path.c_str());
	std::string path_game = game_name_from_relative(relative_path);
	std::string scope;
	std::string pilot;
	std::string mission;
	bool meta_valid = android_save_meta_read_path(path.c_str(), &meta) != 0;
	bool loadable = read_resume_candidate(path, &candidate);
	uint64_t modified = file_mtime_seconds(path.c_str());
	json out;

	save_set_parts_from_relative(relative_path, &scope, &pilot, &mission);
	if (scope.empty())
		scope = save_path_is_coop(path.c_str()) ? "coop" : "single";
	if (pilot.empty())
		pilot = callsign_from_path(path.c_str());
	if (meta_valid) {
		if (meta.wall_clock_unix_seconds == 0)
			meta.wall_clock_unix_seconds = modified;
		std::string callsign = sanitize_text(meta.callsign);
		if (callsign.empty())
			callsign = callsign_from_path(path.c_str());
		if (pilot.empty())
			pilot = callsign;
		if (mission.empty())
			mission = sanitize_text(meta.mission_name);
		if (path_game.empty())
			path_game = game_name(meta.game_id);
	}

	if (loadable) {
		out = resume_candidate_json(files_dir, candidate);
	} else {
		out["path"] = path;
		out["relative_path"] = relative_path;
		out["game"] = path_game.empty() ? "unknown" : path_game;
		out["save_kind"] = meta_valid ? save_kind_name(meta.save_kind) : "unknown";
		out["save_time_unix_seconds"] = meta_valid ? meta.wall_clock_unix_seconds : modified;
		out["callsign"] = meta_valid ? sanitize_text(meta.callsign) : callsign_from_path(path.c_str());
		out["description"] = meta_valid ? sanitize_text(meta.description) : callsign_from_path(path.c_str());
		out["mission_name"] = meta_valid ? sanitize_text(meta.mission_name) : mission;
		out["level_num"] = meta_valid ? meta.level_num : 0;
		out["level_name"] = meta_valid ? sanitize_text(meta.level_name) : "";
		out["level_seconds"] = meta_valid ? meta.level_seconds : 0;
		out["total_seconds"] = meta_valid ? meta.total_seconds : 0;
		out["difficulty_changed"] = meta_valid && meta.difficulty_changed != 0;
		out["difficulty_min"] = meta_valid ? meta.difficulty_min : 0;
		out["difficulty_max"] = meta_valid ? meta.difficulty_max : 0;
		out["slot"] = slot_from_path(path.c_str());
		out["has_thumbnail"] =
		    meta_valid &&
		    meta.thumbnail_format == ANDROID_SAVE_META_THUMB_RGB6 &&
		    meta.thumbnail_width == ANDROID_SAVE_META_THUMB_W &&
		    meta.thumbnail_height == ANDROID_SAVE_META_THUMB_H;
		out["thumbnail_width"] = meta_valid ? meta.thumbnail_width : 0;
		out["thumbnail_height"] = meta_valid ? meta.thumbnail_height : 0;
		out["metadata_backed"] = meta_valid;
	}
	out["scope"] = scope;
	out["pilot"] = pilot;
	out["mission_key"] = mission;
	out["loadable"] = loadable;
	out["orphan"] = !meta_valid || !loadable;
	out["orphan_reason"] =
	    meta_valid ? (loadable ? "" : "not_loadable_from_launcher") : "missing_or_invalid_metadata";
	out["size_bytes"] = file_size_bytes(path.c_str());
	out["modified_unix_seconds"] = modified;
	return out;
}

static json list_save_explorer_slots(const char *files_dir)
{
	std::vector<std::string> paths;
	std::vector<json> slots;
	json out;

	collect_save_paths(files_dir, "d1x-redux", &paths);
	collect_save_paths(files_dir, "d2x-redux", &paths);
	for (const auto &path : paths)
		slots.emplace_back(save_explorer_slot_json(files_dir, path));
	std::sort(slots.begin(), slots.end(), [](const json &a, const json &b) {
		uint64_t a_time = a.value("save_time_unix_seconds", 0ULL);
		uint64_t b_time = b.value("save_time_unix_seconds", 0ULL);
		if (a_time != b_time)
			return a_time > b_time;
		return a.value("relative_path", std::string()) < b.value("relative_path", std::string());
	});
	out["slots"] = json::array();
	for (const auto &slot : slots)
		out["slots"].push_back(slot);
	return out;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_dxxredux_app_ResumeSaveBridge_nativeFindNewestSave(JNIEnv *env,
                                                            jobject /* thiz */,
                                                            jstring jfilesDir)
{
	android_save_meta_candidate candidate;
	const char *files_dir;
	json out;
	std::vector<std::string> paths;

	if (!jfilesDir)
		return NULL;
	files_dir = env->GetStringUTFChars(jfilesDir, NULL);
	if (!files_dir)
		return NULL;

	collect_save_paths(files_dir, "d1x-redux", &paths);
	collect_save_paths(files_dir, "d2x-redux", &paths);
	if (paths.empty()) {
		env->ReleaseStringUTFChars(jfilesDir, files_dir);
		return NULL;
	}

	if (!select_newest_resume_save(paths, &candidate)) {
		env->ReleaseStringUTFChars(jfilesDir, files_dir);
		return NULL;
	}

	out = resume_candidate_json(files_dir, candidate);
	env->ReleaseStringUTFChars(jfilesDir, files_dir);

	std::string dumped = out.dump();
	return env->NewStringUTF(dumped.c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_dxxredux_app_ResumeSaveBridge_nativeFindSaveOptions(JNIEnv *env,
                                                             jobject /* thiz */,
                                                             jstring jfilesDir)
{
	android_save_meta_candidate candidate;
	const char *files_dir;
	json out;
	std::vector<std::string> paths;
	bool found = false;

	if (!jfilesDir)
		return NULL;
	files_dir = env->GetStringUTFChars(jfilesDir, NULL);
	if (!files_dir)
		return NULL;

	collect_save_paths(files_dir, "d1x-redux", &paths);
	collect_save_paths(files_dir, "d2x-redux", &paths);
	if (paths.empty()) {
		env->ReleaseStringUTFChars(jfilesDir, files_dir);
		return NULL;
	}

	if (select_newest_resume_save(paths, &candidate)) {
		out["latest_overall"] = resume_candidate_json(files_dir, candidate);
		found = true;
	}
	if (select_resume_save_by_kind(paths, ANDROID_SAVE_META_KIND_AUTO_PROGRESS, 1, &candidate)) {
		out["highest_progress"] = resume_candidate_json(files_dir, candidate);
		found = true;
	}
	if (select_resume_save_by_kind(paths, ANDROID_SAVE_META_KIND_AUTO_EXIT, 0, &candidate)) {
		out["last_exit"] = resume_candidate_json(files_dir, candidate);
		found = true;
	}
	if (select_resume_save_by_kind(paths, ANDROID_SAVE_META_KIND_AUTO_ABORT, 0, &candidate)) {
		out["last_abort"] = resume_candidate_json(files_dir, candidate);
		found = true;
	}
	if (select_resume_save_by_kind(paths, ANDROID_SAVE_META_KIND_AUTO_MINIMIZE, 0, &candidate)) {
		out["last_minimize"] = resume_candidate_json(files_dir, candidate);
		found = true;
	}
	env->ReleaseStringUTFChars(jfilesDir, files_dir);
	if (!found)
		return NULL;

	std::string dumped = out.dump();
	return env->NewStringUTF(dumped.c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_dxxredux_app_SaveExplorerBridge_nativeListSaveSlots(JNIEnv *env,
                                                             jobject /* thiz */,
                                                             jstring jfilesDir)
{
	const char *files_dir;
	json out;

	if (!jfilesDir)
		return NULL;
	files_dir = env->GetStringUTFChars(jfilesDir, NULL);
	if (!files_dir)
		return NULL;
	out = list_save_explorer_slots(files_dir);
	env->ReleaseStringUTFChars(jfilesDir, files_dir);

	std::string dumped = out.dump();
	return env->NewStringUTF(dumped.c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_dxxredux_app_SaveExplorerBridge_nativeDeleteSaveSlot(JNIEnv *env,
                                                              jobject /* thiz */,
                                                              jstring jfilesDir,
                                                              jstring jpath,
                                                              jlong expectedTimestamp,
                                                              jint expectedSlot)
{
	android_save_meta_disk meta;
	const char *files_dir;
	const char *path;
	uint64_t current_time;
	int current_slot;
	bool meta_valid;
	json out;

	if (!jfilesDir || !jpath) {
		out["deleted"] = false;
		out["reason"] = "missing_path";
		std::string dumped = out.dump();
		return env->NewStringUTF(dumped.c_str());
	}
	files_dir = env->GetStringUTFChars(jfilesDir, NULL);
	if (!files_dir)
		return NULL;
	path = env->GetStringUTFChars(jpath, NULL);
	if (!path) {
		env->ReleaseStringUTFChars(jfilesDir, files_dir);
		return NULL;
	}

	if (!path_is_under_game_root(files_dir, path)) {
		out["deleted"] = false;
		out["reason"] = "outside_game_roots";
		goto done;
	}

	current_slot = slot_from_path(path);
	if (expectedSlot >= 0 && current_slot != expectedSlot) {
		out["deleted"] = false;
		out["reason"] = "slot_changed";
		goto done;
	}

	meta_valid = android_save_meta_read_path(path, &meta) != 0;
	current_time = meta_valid ? meta.wall_clock_unix_seconds : file_mtime_seconds(path);
	if (current_time == 0)
		current_time = file_mtime_seconds(path);
	if (expectedTimestamp > 0 && current_time != (uint64_t) expectedTimestamp) {
		out["deleted"] = false;
		out["reason"] = "timestamp_changed";
		goto done;
	}

	if (remove(path) != 0) {
		out["deleted"] = false;
		out["reason"] = errno == ENOENT ? "already_deleted" : "delete_failed";
		goto done;
	}
	out["deleted"] = true;
	out["reason"] = "";

done:
	env->ReleaseStringUTFChars(jpath, path);
	env->ReleaseStringUTFChars(jfilesDir, files_dir);
	std::string dumped = out.dump();
	return env->NewStringUTF(dumped.c_str());
}

extern "C" JNIEXPORT jbyteArray JNICALL
Java_com_dxxredux_app_ResumeSaveBridge_nativeReadThumbnailRgb6(JNIEnv *env,
                                                               jobject /* thiz */,
                                                               jstring jpath)
{
	android_save_meta_disk meta;
	const char *path;
	jbyteArray out;

	if (!jpath)
		return NULL;
	path = env->GetStringUTFChars(jpath, NULL);
	if (!path)
		return NULL;
	if (!android_save_meta_read_path(path, &meta)) {
		env->ReleaseStringUTFChars(jpath, path);
		return NULL;
	}
	env->ReleaseStringUTFChars(jpath, path);
	if (meta.thumbnail_format != ANDROID_SAVE_META_THUMB_RGB6 ||
	    meta.thumbnail_width != ANDROID_SAVE_META_THUMB_W ||
	    meta.thumbnail_height != ANDROID_SAVE_META_THUMB_H)
		return NULL;
	out = env->NewByteArray((jsize) sizeof(meta.thumbnail_rgb6));
	if (!out)
		return NULL;
	env->SetByteArrayRegion(out, 0, (jsize) sizeof(meta.thumbnail_rgb6),
	                        reinterpret_cast<const jbyte *>(meta.thumbnail_rgb6));
	return out;
}

#endif
