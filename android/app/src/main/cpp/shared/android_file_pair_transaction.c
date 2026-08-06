#include "android_file_pair_transaction.h"

static void delete_if_present(const struct android_file_pair_ops *ops,
                              const char *path)
{
	if (path && ops->exists(ops->context, path))
		ops->delete_path(ops->context, path);
}

int android_file_pair_publish(const struct android_file_pair_paths *paths,
                              const struct android_file_pair_ops *ops)
{
	int old_primary;
	int old_companion;
	int primary_backed_up = 0;
	int companion_backed_up = 0;
	int primary_published = 0;
	int companion_published = 0;

	if (!paths || !ops || !ops->exists || !ops->rename_path ||
	    !ops->delete_path || !paths->primary_temp || !paths->primary_path ||
	    !paths->primary_backup || !paths->companion_path ||
	    !paths->companion_backup ||
	    (paths->companion_present && !paths->companion_temp) ||
	    !ops->exists(ops->context, paths->primary_temp) ||
	    (paths->companion_present &&
	     !ops->exists(ops->context, paths->companion_temp)))
		return 0;
	if (ops->exists(ops->context, paths->primary_backup) ||
	    ops->exists(ops->context, paths->companion_backup)) {
		delete_if_present(ops, paths->primary_temp);
		delete_if_present(ops, paths->companion_temp);
		return 0;
	}

	old_primary = ops->exists(ops->context, paths->primary_path);
	old_companion = ops->exists(ops->context, paths->companion_path);
	if (old_primary) {
		if (!ops->rename_path(ops->context, paths->primary_path,
		                      paths->primary_backup))
			goto rollback;
		primary_backed_up = 1;
	}
	if (old_companion) {
		if (!ops->rename_path(ops->context, paths->companion_path,
		                      paths->companion_backup))
			goto rollback;
		companion_backed_up = 1;
	}
	if (paths->companion_present) {
		if (!ops->rename_path(ops->context, paths->companion_temp,
		                      paths->companion_path))
			goto rollback;
		companion_published = 1;
	}
	if (!ops->rename_path(ops->context, paths->primary_temp,
	                      paths->primary_path))
		goto rollback;
	primary_published = 1;

	delete_if_present(ops, paths->primary_backup);
	delete_if_present(ops, paths->companion_backup);
	delete_if_present(ops, paths->companion_temp);
	return 1;

rollback:
	if (primary_published)
		delete_if_present(ops, paths->primary_path);
	if (companion_published)
		delete_if_present(ops, paths->companion_path);
	if (primary_backed_up)
		ops->rename_path(ops->context, paths->primary_backup,
		                 paths->primary_path);
	if (companion_backed_up)
		ops->rename_path(ops->context, paths->companion_backup,
		                 paths->companion_path);
	delete_if_present(ops, paths->primary_temp);
	delete_if_present(ops, paths->companion_temp);
	return 0;
}
