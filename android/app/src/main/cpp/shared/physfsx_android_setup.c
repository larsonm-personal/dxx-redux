#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "physfsx_android_setup.h"

#define MAX_MOD_MOUNTS      64
#define MAX_REQUIRED_MOUNTS (MAX_MOD_MOUNTS + 4)

static int copy_text(char *destination, size_t size, const char *source)
{
	int written = snprintf(destination, size, "%s", source ? source : "");
	return written >= 0 && (size_t) written < size;
}

static int format_path(char *destination, size_t size, const char *format,
                       const char *first, const char *second)
{
	int written = snprintf(destination, size, format, first, second);
	return written >= 0 && (size_t) written < size;
}

static void set_failure(physfsx_android_setup_result *result, const char *operation,
                        const char *path, const char *detail)
{
	copy_text(result->operation, sizeof(result->operation), operation);
	copy_text(result->path, sizeof(result->path), path);
	copy_text(result->detail, sizeof(result->detail), detail && detail[0] ? detail : "unknown error");
}

static void set_physfs_failure(physfsx_android_setup_result *result, const char *operation,
                               const char *path, const physfsx_android_setup_ops *ops)
{
	set_failure(result, operation, path, ops->last_error ? ops->last_error() : NULL);
}

static int read_required_line(FILE *file, char *line, size_t size,
                              physfsx_android_setup_result *result, const char *operation,
                              const char *path)
{
	char *newline;
	if (!fgets(line, (int) size, file)) {
		set_failure(result, operation, path, ferror(file) ? strerror(errno) : "file is empty");
		return 0;
	}
	newline = strpbrk(line, "\r\n");
	if (newline)
		*newline = '\0';
	else if (!feof(file)) {
		set_failure(result, operation, path, "line exceeds the supported path length");
		return 0;
	}
	if (!line[0]) {
		set_failure(result, operation, path, "path is empty");
		return 0;
	}
	return 1;
}

static void rollback(const physfsx_android_setup_ops *ops,
                     char mounted[MAX_REQUIRED_MOUNTS][PHYSFSX_ANDROID_PATH_MAX],
                     int mounted_count, const char *previous_write_dir)
{
	while (mounted_count > 0)
		ops->unmount(mounted[--mounted_count]);
	ops->set_write_dir(previous_write_dir && previous_write_dir[0] ? previous_write_dir : NULL);
}

static int record_mount(const char *path, const char *mount_point, int append,
                        const char *operation, const physfsx_android_setup_ops *ops,
                        physfsx_android_setup_result *result,
                        char mounted[MAX_REQUIRED_MOUNTS][PHYSFSX_ANDROID_PATH_MAX],
                        int *mounted_count)
{
	if (!ops->mount(path, mount_point, append)) {
		set_physfs_failure(result, operation, path, ops);
		return 0;
	}
	if (*mounted_count >= MAX_REQUIRED_MOUNTS ||
	    !copy_text(mounted[*mounted_count], PHYSFSX_ANDROID_PATH_MAX, path)) {
		ops->unmount(path);
		set_failure(result, operation, path, "required mount tracking capacity exceeded");
		return 0;
	}
	(*mounted_count)++;
	return 1;
}

int physfsx_android_setup_search_paths(const char *game_dir,
                                       const physfsx_android_setup_ops *ops,
                                       physfsx_android_setup_result *result)
{
	const char *pref;
	const char *preview_setdir = getenv("DXX_ANDROID_LEVEL_PREVIEW_DATA_DIR");
	const char *write_dir;
	char previous_write_dir[PHYSFSX_ANDROID_PATH_MAX] = "";
	char game_path[PHYSFSX_ANDROID_PATH_MAX];
	char active_path_file[PHYSFSX_ANDROID_PATH_MAX];
	char setdir[PHYSFSX_ANDROID_PATH_MAX] = "";
	char saf_path[PHYSFSX_ANDROID_PATH_MAX];
	char mod_path_file[PHYSFSX_ANDROID_PATH_MAX];
	char mod_paths[MAX_MOD_MOUNTS][PHYSFSX_ANDROID_PATH_MAX];
	char mod_mounts[MAX_MOD_MOUNTS][64];
	char mounted[MAX_REQUIRED_MOUNTS][PHYSFSX_ANDROID_PATH_MAX];
	int mounted_count = 0;
	int mod_count = 0;
	FILE *file;

	if (!result)
		return 0;
	memset(result, 0, sizeof(*result));
	if (!game_dir || !game_dir[0] || !ops || !ops->get_pref_dir ||
	    !ops->get_write_dir || !ops->set_write_dir || !ops->make_dir ||
	    !ops->mount || !ops->unmount || !ops->register_saf_archiver) {
		set_failure(result, "validate setup", game_dir, "invalid setup arguments");
		return 0;
	}
	pref = ops->get_pref_dir("com.dxxredux", game_dir);
	if (!pref || !pref[0]) {
		set_physfs_failure(result, "resolve preferred directory", game_dir, ops);
		return 0;
	}
	write_dir = ops->get_write_dir();
	if (write_dir && !copy_text(previous_write_dir, sizeof(previous_write_dir), write_dir)) {
		set_failure(result, "capture previous write directory", write_dir, "path is too long");
		return 0;
	}
	if (!format_path(game_path, sizeof(game_path), "%s%s/", pref, game_dir) ||
	    !format_path(active_path_file, sizeof(active_path_file), "%s%s", game_path,
	                 ".active_set_path") ||
	    !format_path(mod_path_file, sizeof(mod_path_file), "%s%s", game_path,
	                 ".active_mod_paths")) {
		set_failure(result, "construct setup path", pref, "path is too long");
		return 0;
	}

	if (!ops->set_write_dir(pref)) {
		set_physfs_failure(result, "select preferred write directory", pref, ops);
		return 0;
	}
	if (!ops->make_dir(game_dir)) {
		set_physfs_failure(result, "create per-game write directory", game_path, ops);
		rollback(ops, mounted, mounted_count, previous_write_dir);
		return 0;
	}
	if (!ops->set_write_dir(game_path)) {
		set_physfs_failure(result, "select per-game write directory", game_path, ops);
		rollback(ops, mounted, mounted_count, previous_write_dir);
		return 0;
	}
	if (!record_mount(game_path, NULL, 1, "mount per-game directory", ops, result,
	                  mounted, &mounted_count) ||
	    !record_mount(pref, NULL, 1, "mount preferred directory", ops, result,
	                  mounted, &mounted_count)) {
		rollback(ops, mounted, mounted_count, previous_write_dir);
		return 0;
	}
	if (!ops->register_saf_archiver()) {
		set_physfs_failure(result, "register SAF archiver", "SAF", ops);
		rollback(ops, mounted, mounted_count, previous_write_dir);
		return 0;
	}

	if (preview_setdir && preview_setdir[0]) {
		if (!copy_text(setdir, sizeof(setdir), preview_setdir)) {
			set_failure(result, "read preview set path", preview_setdir, "path is too long");
			rollback(ops, mounted, mounted_count, previous_write_dir);
			return 0;
		}
	} else {
		file = fopen(active_path_file, "r");
		if (file) {
			int read_ok = read_required_line(file, setdir, sizeof(setdir), result,
			                                 "read active set path", active_path_file);
			int close_ok = fclose(file) == 0;
			if (!read_ok || !close_ok) {
				if (read_ok)
					set_failure(result, "close active set path", active_path_file, strerror(errno));
				rollback(ops, mounted, mounted_count, previous_write_dir);
				return 0;
			}
		} else if (errno != ENOENT) {
			set_failure(result, "open active set path", active_path_file, strerror(errno));
			rollback(ops, mounted, mounted_count, previous_write_dir);
			return 0;
		}
	}
	if (setdir[0] && !record_mount(setdir, NULL, 0, "mount active set", ops, result,
	                               mounted, &mounted_count)) {
		rollback(ops, mounted, mounted_count, previous_write_dir);
		return 0;
	}

	if (setdir[0]) {
		if (!format_path(saf_path, sizeof(saf_path), "%s%s", setdir,
		                 "/.saf_manifest.json")) {
			set_failure(result, "construct SAF manifest path", setdir, "path is too long");
			rollback(ops, mounted, mounted_count, previous_write_dir);
			return 0;
		}
	} else if (!format_path(saf_path, sizeof(saf_path), "%s%s", pref,
	                        ".saf_manifest.json")) {
		set_failure(result, "construct SAF manifest path", pref, "path is too long");
		rollback(ops, mounted, mounted_count, previous_write_dir);
		return 0;
	}
	file = fopen(saf_path, "r");
	if (file) {
		if (fclose(file) != 0) {
			set_failure(result, "close SAF manifest", saf_path, strerror(errno));
			rollback(ops, mounted, mounted_count, previous_write_dir);
			return 0;
		}
		if (!record_mount(saf_path, NULL, 1, "mount SAF manifest", ops, result,
		                  mounted, &mounted_count)) {
			rollback(ops, mounted, mounted_count, previous_write_dir);
			return 0;
		}
	} else if (errno != ENOENT) {
		set_failure(result, "open SAF manifest", saf_path, strerror(errno));
		rollback(ops, mounted, mounted_count, previous_write_dir);
		return 0;
	}

	if (!preview_setdir || !preview_setdir[0]) {
		file = fopen(mod_path_file, "r");
		if (file) {
			while (1) {
				char line[PHYSFSX_ANDROID_PATH_MAX];
				char *mount_point;
				if (!fgets(line, sizeof(line), file))
					break;
				{
					char *newline = strpbrk(line, "\r\n");
					if (newline)
						*newline = '\0';
					else if (!feof(file)) {
						set_failure(result, "read enabled mod paths", mod_path_file,
						            "line exceeds the supported path length");
						fclose(file);
						rollback(ops, mounted, mounted_count, previous_write_dir);
						return 0;
					}
				}
				if (!line[0])
					continue;
				if (mod_count >= MAX_MOD_MOUNTS) {
					set_failure(result, "read enabled mod paths", mod_path_file,
					            "more than 64 required mount entries");
					fclose(file);
					rollback(ops, mounted, mounted_count, previous_write_dir);
					return 0;
				}
				mount_point = strchr(line, '\t');
				if (mount_point)
					*mount_point++ = '\0';
				if (!line[0] ||
				    !copy_text(mod_paths[mod_count], sizeof(mod_paths[mod_count]), line) ||
				    !copy_text(mod_mounts[mod_count], sizeof(mod_mounts[mod_count]),
				               mount_point)) {
					set_failure(result, "read enabled mod paths", mod_path_file,
					            "invalid mod path or mount point");
					fclose(file);
					rollback(ops, mounted, mounted_count, previous_write_dir);
					return 0;
				}
				mod_count++;
			}
			{
				int read_failed = ferror(file);
				int close_failed = fclose(file) != 0;
				if (read_failed || close_failed) {
					set_failure(result, "read enabled mod paths", mod_path_file, strerror(errno));
					rollback(ops, mounted, mounted_count, previous_write_dir);
					return 0;
				}
			}
		} else if (errno != ENOENT) {
			set_failure(result, "open enabled mod paths", mod_path_file, strerror(errno));
			rollback(ops, mounted, mounted_count, previous_write_dir);
			return 0;
		}
		while (mod_count > 0) {
			int index = --mod_count;
			const char *mount_point = mod_mounts[index][0] ? mod_mounts[index] : NULL;
			if (!record_mount(mod_paths[index], mount_point, 0, "mount enabled mod", ops,
			                  result, mounted, &mounted_count)) {
				rollback(ops, mounted, mounted_count, previous_write_dir);
				return 0;
			}
		}
	}

	/* These roots are optional compatibility fallbacks after required content. */
	if (ops->get_base_dir && ops->get_base_dir())
		ops->mount(ops->get_base_dir(), NULL, 1);
	if (ops->add_relative_path)
		ops->add_relative_path("data", 1);
	return 1;
}
