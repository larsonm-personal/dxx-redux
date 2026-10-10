#include <stdio.h>
#include <string.h>
#include <setjmp.h>

#include "android_file_pair_transaction.h"

enum test_file {
	FILE_PRIMARY_TEMP = 0,
	FILE_PRIMARY,
	FILE_PRIMARY_BACKUP,
	FILE_COMPANION_TEMP,
	FILE_COMPANION,
	FILE_COMPANION_BACKUP,
	FILE_PENDING,
	FILE_COMMITTED,
	FILE_COUNT
};

struct test_context {
	int present[FILE_COUNT];
	int value[FILE_COUNT];
	int rename_count;
	int fail_rename;
	int fail_delete;
};

static int failures;
static jmp_buf interrupted;
static int crash_after, mutations;
static void mutation(void)
{
	if (++mutations == crash_after) longjmp(interrupted, 1);
}
static const char *const names[FILE_COUNT] = {
	"primary.tmp", "primary", "primary.bak", "companion.tmp",
	"companion", "companion.bak", "primary.pending", "primary.committed"
};

static int file_index(const char *path)
{
	int i;

	for (i = 0; i < FILE_COUNT; ++i)
		if (!strcmp(path, names[i]))
			return i;
	return -1;
}

static int test_exists(void *opaque, const char *path)
{
	struct test_context *context = (struct test_context *) opaque;
	int index = file_index(path);

	return index >= 0 && context->present[index];
}

static int test_rename(void *opaque, const char *old_path,
                       const char *new_path)
{
	struct test_context *context = (struct test_context *) opaque;
	int old_index = file_index(old_path);
	int new_index = file_index(new_path);

	context->rename_count++;
	if (context->rename_count == context->fail_rename || old_index < 0 ||
	    new_index < 0 || !context->present[old_index])
		return 0;
	context->present[new_index] = 1;
	context->value[new_index] = context->value[old_index];
	context->present[old_index] = 0;
	mutation();
	return 1;
}

static int test_delete(void *opaque, const char *path)
{
	struct test_context *context = (struct test_context *) opaque;
	int index = file_index(path);

	if (index < 0 || context->fail_delete)
		return 0;
	context->present[index] = 0;
	mutation();
	return 1;
}

static int test_write_state(void *opaque, const char *path, unsigned state)
{
	struct test_context *context = (struct test_context *) opaque;
	int index = file_index(path);
	if (index < 0) return 0;
	context->present[index] = 1;
	context->value[index] = (int) state;
	mutation();
	return 1;
}

static int test_read_state(void *opaque, const char *path, unsigned *state)
{
	struct test_context *context = (struct test_context *) opaque;
	int index = file_index(path);
	if (index < 0 || !context->present[index]) return 0;
	*state = (unsigned) context->value[index];
	return 1;
}

static void expect(int condition, const char *message)
{
	if (!condition) {
		printf("FAIL: %s\n", message);
		failures++;
	}
}

static struct android_file_pair_paths paths(int companion_present)
{
	struct android_file_pair_paths result = {
		names[FILE_PRIMARY_TEMP], names[FILE_PRIMARY],
		names[FILE_PRIMARY_BACKUP], names[FILE_COMPANION_TEMP],
		names[FILE_COMPANION], names[FILE_COMPANION_BACKUP],
		companion_present, names[FILE_PENDING], names[FILE_COMMITTED]
	};

	return result;
}

static struct android_file_pair_ops ops(struct test_context *context)
{
	struct android_file_pair_ops result = {
		context, test_exists, test_rename, test_delete, test_write_state, test_read_state
	};

	return result;
}

static void reset_pair(struct test_context *context, int old_companion,
                       int new_companion)
{
	memset(context, 0, sizeof(*context));
	context->present[FILE_PRIMARY_TEMP] = 1;
	context->value[FILE_PRIMARY_TEMP] = 2;
	context->present[FILE_PRIMARY] = 1;
	context->value[FILE_PRIMARY] = 1;
	context->present[FILE_COMPANION_TEMP] = new_companion;
	context->value[FILE_COMPANION_TEMP] = 20;
	context->present[FILE_COMPANION] = old_companion;
	context->value[FILE_COMPANION] = 10;
}

static void expect_pair(const struct test_context *context, int primary,
                        int companion_present, int companion,
                        const char *message)
{
	expect(context->present[FILE_PRIMARY] &&
	           context->value[FILE_PRIMARY] == primary &&
	           context->present[FILE_COMPANION] == companion_present &&
	           (!companion_present ||
	            context->value[FILE_COMPANION] == companion),
	       message);
	expect(!context->present[FILE_PRIMARY_TEMP] &&
	           !context->present[FILE_PRIMARY_BACKUP] &&
	           !context->present[FILE_COMPANION_TEMP] &&
	           !context->present[FILE_COMPANION_BACKUP],
	       "transaction artifacts removed");
}

int main(void)
{
	struct test_context context;
	struct android_file_pair_paths transaction_paths;
	struct android_file_pair_ops transaction_ops;
	int failure_step;

	reset_pair(&context, 1, 1);
	transaction_paths = paths(1);
	transaction_ops = ops(&context);
	expect(android_file_pair_publish(&transaction_paths, &transaction_ops),
	       "new pair commits");
	expect_pair(&context, 2, 1, 20, "new pair is published");

	reset_pair(&context, 1, 0);
	transaction_paths = paths(0);
	transaction_ops = ops(&context);
	expect(android_file_pair_publish(&transaction_paths, &transaction_ops),
	       "absent companion commits");
	expect_pair(&context, 2, 0, 0, "new companion absence is published");

	for (failure_step = 1; failure_step <= 5; ++failure_step) {
		reset_pair(&context, 1, 1);
		context.fail_rename = failure_step;
		transaction_paths = paths(1);
		transaction_ops = ops(&context);
		expect(!android_file_pair_publish(&transaction_paths,
		                                  &transaction_ops),
		       "injected rename failure is reported");
		expect_pair(&context, 1, 1, 10,
		            "injected failure preserves old pair");
	}
	/* A crash can interrupt publication, rollback or cleanup. Keep the fake
	 * filesystem outside the setjmp frame so its mutations survive longjmp */
	for (int old_primary = 0; old_primary <= 1; ++old_primary)
		for (int old_secret = 0; old_secret <= 1; ++old_secret)
			for (int new_secret = 0; new_secret <= 1; ++new_secret)
				for (int fail = 0; fail <= 5; ++fail)
					for (int point = 1; point <= 14; ++point) {
						static struct test_context disk;
						static struct test_context interrupted_disk;
						reset_pair(&disk, old_secret, new_secret);
						disk.present[FILE_PRIMARY] = old_primary;
						disk.fail_rename = fail;
						transaction_paths = paths(new_secret);
						transaction_ops = ops(&disk);
						mutations = 0;
						crash_after = point;
						if (!setjmp(interrupted)) android_file_pair_publish(&transaction_paths, &transaction_ops);
						crash_after = 0;
						disk.fail_rename = 0;
						interrupted_disk = disk;
						for (int recovery_point = 1; recovery_point <= 8; ++recovery_point) {
							disk = interrupted_disk;
							mutations = 0;
							crash_after = recovery_point;
							if (!setjmp(interrupted)) android_file_pair_recover(&transaction_paths, &transaction_ops);
							crash_after = 0;
							expect(android_file_pair_recover(&transaction_paths, &transaction_ops), "restart recovers after any mutation");
							const int old_pair = disk.present[FILE_PRIMARY] == old_primary &&
							                     (!old_primary || disk.value[FILE_PRIMARY] == 1) && disk.present[FILE_COMPANION] == old_secret &&
							                     (!old_secret || disk.value[FILE_COMPANION] == 10);
							const int new_pair = disk.present[FILE_PRIMARY] && disk.value[FILE_PRIMARY] == 2 &&
							                     disk.present[FILE_COMPANION] == new_secret && (!new_secret || disk.value[FILE_COMPANION] == 20);
							expect(old_pair || new_pair, "restart exposes coherent old or new pair");
							disk.present[FILE_PRIMARY_TEMP] = 1;
							disk.value[FILE_PRIMARY_TEMP] = 2;
							disk.present[FILE_COMPANION_TEMP] = new_secret;
							disk.value[FILE_COMPANION_TEMP] = 20;
							expect(android_file_pair_publish(&transaction_paths, &transaction_ops), "next save succeeds after recovery");
						}
					}
	reset_pair(&context, 1, 1);
	context.fail_delete = 1;
	transaction_paths = paths(1);
	transaction_ops = ops(&context);
	expect(android_file_pair_publish(&transaction_paths, &transaction_ops), "cleanup failure retains committed success");
	expect(context.present[FILE_COMMITTED], "cleanup failure retains commit record");
	context.fail_delete = 0;
	expect(android_file_pair_recover(&transaction_paths, &transaction_ops), "healthy cleanup retry succeeds");
	expect_pair(&context, 2, 1, 20, "cleanup retry preserves new pair");
	/* Fail publication and then rollback's attempt to remove the new companion */
	reset_pair(&context, 1, 1);
	context.fail_rename = 4;
	context.fail_delete = 1;
	expect(!android_file_pair_publish(&transaction_paths, &transaction_ops), "publication plus rollback failure is reported");
	expect(context.present[FILE_PENDING] && context.present[FILE_COMPANION_BACKUP], "failed rollback retains its journal and original bytes");
	context.fail_rename = context.fail_delete = 0;
	expect(android_file_pair_recover(&transaction_paths, &transaction_ops), "recovery retries failed rollback");
	expect(context.value[FILE_PRIMARY] == 1 && context.value[FILE_COMPANION] == 10 &&
	           !context.present[FILE_PENDING],
	       "retried rollback restores the old pair");
	context.present[FILE_PRIMARY_TEMP] = context.present[FILE_COMPANION_TEMP] = 1;
	context.value[FILE_PRIMARY_TEMP] = 2;
	context.value[FILE_COMPANION_TEMP] = 20;
	expect(android_file_pair_publish(&transaction_paths, &transaction_ops), "next publication succeeds after failed rollback recovery");
	expect_pair(&context, 2, 1, 20, "new pair replaces the recovered old pair");
	if (!failures) puts("PASS: 5376 publication/recovery interruption cases recover coherent pairs and allow the next save");

	return failures ? 1 : 0;
}
