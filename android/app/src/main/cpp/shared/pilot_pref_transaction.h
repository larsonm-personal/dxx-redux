#ifndef PILOT_PREF_TRANSACTION_H
#define PILOT_PREF_TRANSACTION_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*pilot_pref_patch_fn)(const char *path, void *context);

struct pilot_pref_patch_target {
	const char *path;
	pilot_pref_patch_fn patch;
	void *context;
};

/*
 * Apply a set of individually atomic pilot-file patches as one recoverable
 * operation.  Returns the target count on success, -1 if a patch failed and
 * the original files were restored, or -2 if restoring an original failed.
 */
int pilot_pref_patch_transaction(const struct pilot_pref_patch_target *targets,
                                 size_t target_count);

#ifdef __cplusplus
}
#endif

#endif
