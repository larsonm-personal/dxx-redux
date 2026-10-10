#include "android_file_pair_transaction.h"

enum { OLD_PRIMARY = 1,
	   OLD_COMPANION = 2,
	   STATE_TAG = 0x44585000 };

static int remove_if_present(const struct android_file_pair_ops *ops, const char *path)
{
	return !path || !ops->exists(ops->context, path) || ops->delete_path(ops->context, path);
}

static int valid(const struct android_file_pair_paths *p, const struct android_file_pair_ops *o)
{
	return p && o && o->exists && o->rename_path && o->delete_path && o->write_state && o->read_state &&
	       p->primary_path && p->primary_backup && p->companion_path && p->companion_backup && p->pending && p->committed;
}

static int restore(const struct android_file_pair_ops *o, const char *path, const char *backup, int existed)
{
	if (!existed) return remove_if_present(o, path);
	/* Missing backup means either the original was never moved or rollback
	 * already restored it before another interruption */
	if (!o->exists(o->context, backup)) return o->exists(o->context, path);
	return remove_if_present(o, path) && o->rename_path(o->context, backup, path);
}

int android_file_pair_recover(const struct android_file_pair_paths *p, const struct android_file_pair_ops *o)
{
	unsigned state;
	if (!valid(p, o)) return 0;
	if (o->exists(o->context, p->committed)) {
		/* Commit is durable before deleting either backup; cleanup is retryable */
		return remove_if_present(o, p->primary_backup) && remove_if_present(o, p->companion_backup) &&
		       remove_if_present(o, p->committed);
	}
	if (o->exists(o->context, p->pending)) {
		if (!o->read_state(o->context, p->pending, &state) || (state & ~3u) != STATE_TAG) return 0;
		if (!restore(o, p->primary_path, p->primary_backup, state & OLD_PRIMARY) ||
		    !restore(o, p->companion_path, p->companion_backup, state & OLD_COMPANION)) return 0;
		return remove_if_present(o, p->pending);
	}
	/* Unrecognized artifacts must not erase the only remaining old bytes */
	return !o->exists(o->context, p->primary_backup) && !o->exists(o->context, p->companion_backup);
}

int android_file_pair_publish(const struct android_file_pair_paths *p, const struct android_file_pair_ops *o)
{
	unsigned state = STATE_TAG;
	if (!valid(p, o) || !p->primary_temp || (p->companion_present && !p->companion_temp) ||
	    !android_file_pair_recover(p, o) || !o->exists(o->context, p->primary_temp) ||
	    (p->companion_present && !o->exists(o->context, p->companion_temp))) return 0;
	if (o->exists(o->context, p->primary_path)) state |= OLD_PRIMARY;
	if (o->exists(o->context, p->companion_path)) state |= OLD_COMPANION;
	/* Persist old presence before the first destructive rename */
	if (!o->write_state(o->context, p->pending, state)) return 0;
	if ((state & OLD_PRIMARY) && !o->rename_path(o->context, p->primary_path, p->primary_backup)) goto rollback;
	if ((state & OLD_COMPANION) && !o->rename_path(o->context, p->companion_path, p->companion_backup)) goto rollback;
	if (p->companion_present && !o->rename_path(o->context, p->companion_temp, p->companion_path)) goto rollback;
	if (!o->rename_path(o->context, p->primary_temp, p->primary_path)) goto rollback;
	if (!o->rename_path(o->context, p->pending, p->committed)) goto rollback;
	/* Failure to clean up cannot invalidate the committed pair; recovery will
	 * finish cleanup before another writer starts */
	android_file_pair_recover(p, o);
	return 1;

rollback:
	/* Keep the pending record if any rollback operation fails */
	android_file_pair_recover(p, o);
	remove_if_present(o, p->primary_temp);
	remove_if_present(o, p->companion_temp);
	return 0;
}
