#ifdef ANDROID

#include <jni.h>

#include <cstdio>
#include <cstring>
#include <dirent.h>
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
	       (name[len - 3] == 's' || name[len - 3] == 'S') &&
	       (name[len - 2] == 'g' || name[len - 2] == 'G') &&
	       name[len - 1] >= '0' && name[len - 1] <= '9';
}

static void collect_save_paths(const char *files_dir,
                               const char *subdir,
                               std::vector<std::string> *paths)
{
	char dir_path[ANDROID_SAVE_META_PATH_LEN];
	const char *subdirs[] = { "", "/Players" };
	int d;

	if (!files_dir || !subdir || !paths)
		return;

	for (d = 0; d < 2; d++) {
		DIR *dp;
		struct dirent *ent;
		int wrote = snprintf(dir_path, sizeof(dir_path), "%s/%s%s", files_dir, subdir, subdirs[d]);

		if (wrote <= 0 || wrote >= (int) sizeof(dir_path))
			continue;
		dp = opendir(dir_path);
		if (!dp)
			continue;
		while ((ent = readdir(dp)) != NULL) {
			char path[ANDROID_SAVE_META_PATH_LEN];
			int path_wrote;

			if (!is_save_slot_name(ent->d_name))
				continue;
			path_wrote = snprintf(path, sizeof(path), "%s/%s", dir_path, ent->d_name);
			if (path_wrote <= 0 || path_wrote >= (int) sizeof(path))
				continue;
			paths->emplace_back(path);
		}
		closedir(dp);
	}
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
	if ((dot[1] != 's' && dot[1] != 'S') ||
	    (dot[2] != 'g' && dot[2] != 'G') ||
	    dot[3] < '0' || dot[3] > '9')
		return -1;
	return dot[3] - '0';
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
				return 5;
			case ANDROID_SAVE_META_KIND_AUTO_EXIT:
				return 4;
			case ANDROID_SAVE_META_KIND_AUTO_MINIMIZE:
				return 3;
			case ANDROID_SAVE_META_KIND_AUTO_PROGRESS:
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
