#ifndef ANDROID_FILE_PAIR_TRANSACTION_H
#define ANDROID_FILE_PAIR_TRANSACTION_H

struct android_file_pair_paths {
	const char *primary_temp;
	const char *primary_path;
	const char *primary_backup;
	const char *companion_temp;
	const char *companion_path;
	const char *companion_backup;
	int companion_present;
	const char *pending;
	const char *committed;
};

struct android_file_pair_ops {
	void *context;
	int (*exists)(void *context, const char *path);
	int (*rename_path)(void *context, const char *old_path,
	                   const char *new_path);
	int (*delete_path)(void *context, const char *path);
	int (*write_state)(void *context, const char *path, unsigned state);
	int (*read_state)(void *context, const char *path, unsigned *state);
};

/* Call under the same file lock as publication, including before readers */
int android_file_pair_recover(const struct android_file_pair_paths *paths,
                              const struct android_file_pair_ops *ops);
int android_file_pair_publish(const struct android_file_pair_paths *paths,
                              const struct android_file_pair_ops *ops);

#endif
