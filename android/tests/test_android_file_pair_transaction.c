#include <stdio.h>
#include <string.h>

#include "android_file_pair_transaction.h"

enum test_file {
	FILE_PRIMARY_TEMP = 0,
	FILE_PRIMARY,
	FILE_PRIMARY_BACKUP,
	FILE_COMPANION_TEMP,
	FILE_COMPANION,
	FILE_COMPANION_BACKUP,
	FILE_COUNT
};

struct test_context {
	int present[FILE_COUNT];
	int value[FILE_COUNT];
	int rename_count;
	int fail_rename;
};

static int failures;
static const char *const names[FILE_COUNT] = {
	"primary.tmp", "primary", "primary.bak", "companion.tmp",
	"companion", "companion.bak"
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
	return 1;
}

static int test_delete(void *opaque, const char *path)
{
	struct test_context *context = (struct test_context *) opaque;
	int index = file_index(path);

	if (index < 0)
		return 0;
	context->present[index] = 0;
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
		companion_present
	};

	return result;
}

static struct android_file_pair_ops ops(struct test_context *context)
{
	struct android_file_pair_ops result = {
		context, test_exists, test_rename, test_delete
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

	for (failure_step = 1; failure_step <= 4; ++failure_step) {
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

	return failures ? 1 : 0;
}
