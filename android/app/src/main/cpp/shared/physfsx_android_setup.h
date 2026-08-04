#ifndef PHYSFSX_ANDROID_SETUP_H
#define PHYSFSX_ANDROID_SETUP_H

#define PHYSFSX_ANDROID_PATH_MAX   512
#define PHYSFSX_ANDROID_DETAIL_MAX 256

typedef struct physfsx_android_setup_ops {
	const char *(*get_pref_dir)(const char *organization, const char *application);
	const char *(*get_write_dir)(void);
	int (*set_write_dir)(const char *path);
	int (*make_dir)(const char *path);
	int (*mount)(const char *path, const char *mount_point, int append);
	int (*unmount)(const char *path);
	int (*register_saf_archiver)(void);
	const char *(*get_base_dir)(void);
	int (*add_relative_path)(const char *path, int append);
	const char *(*last_error)(void);
} physfsx_android_setup_ops;

typedef struct physfsx_android_setup_result {
	char operation[64];
	char path[PHYSFSX_ANDROID_PATH_MAX];
	char detail[PHYSFSX_ANDROID_DETAIL_MAX];
} physfsx_android_setup_result;

int physfsx_android_setup_search_paths(const char *game_dir,
                                       const physfsx_android_setup_ops *ops,
                                       physfsx_android_setup_result *result);

#endif
