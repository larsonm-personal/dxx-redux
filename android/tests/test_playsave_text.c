#include "playsave_text.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int write_fixture(const char *path, const unsigned char *data, size_t size)
{
	FILE *f = fopen(path, "wb");
	int ok = f && fwrite(data, 1, size, f) == size;

	if (f)
		fclose(f);
	return ok;
}

static unsigned char *read_fixture(const char *path, size_t *size)
{
	FILE *f = fopen(path, "rb");
	long length;
	unsigned char *data;

	if (!f || fseek(f, 0, SEEK_END) || (length = ftell(f)) < 0 ||
	    fseek(f, 0, SEEK_SET)) {
		if (f)
			fclose(f);
		return NULL;
	}
	data = malloc((size_t)length + 1);
	if (!data || fread(data, 1, (size_t)length, f) != (size_t)length) {
		free(data);
		fclose(f);
		return NULL;
	}
	data[length] = 0;
	*size = (size_t)length;
	fclose(f);
	return data;
}

int main(void)
{
	const char *path = "test_playsave_text.tmp";
	const char prefix[] = "[D1X Options]\n[cockpit]\nmode=1\nunknown=keep\n[end]\n";
	const char suffix[] = "[graphics]\nalphaeffects=0\n[end]\n[end]\n";
	const char empty_options[] = "[D1X Options]\n[end]\n";
	const char malformed[] = "[cockpit]\nunknown=keep\n";
	const struct playsave_text_entry entries[] = {
		{"mode=", "mode=2\n"},
		{"robothostagecounts=", "robothostagecounts=1\n"},
		{"bosshealthbar=", "bosshealthbar=0\n"},
		{"mapcheatsaccessible=", "mapcheatsaccessible=1\n"}
	};
	const size_t filler_size = 65537;
	unsigned char *fixture = malloc(sizeof(prefix) - 1 + filler_size + sizeof(suffix));
	unsigned char *result;
	size_t fixture_size, result_size;
	int ok = 0;

	if (!fixture)
		return 1;
	memcpy(fixture, prefix, sizeof(prefix) - 1);
	memset(fixture + sizeof(prefix) - 1, '#', filler_size);
	fixture[sizeof(prefix) - 1 + filler_size - 1] = '\n';
	memcpy(fixture + sizeof(prefix) - 1 + filler_size, suffix, sizeof(suffix));
	fixture_size = sizeof(prefix) - 1 + filler_size + sizeof(suffix) - 1;
	if (!write_fixture(path, fixture, fixture_size) ||
	    !playsave_text_update_section(path, "[D1X Options]", "[cockpit]",
		entries, sizeof(entries) / sizeof(entries[0])))
		goto cleanup;
	result = read_fixture(path, &result_size);
	if (!result)
		goto cleanup;
	if (!strstr((char *)result, "mode=2\nunknown=keep\nrobothostagecounts=1\nbosshealthbar=0\nmapcheatsaccessible=1\n[end]\n") ||
	    strstr((char *)result, "mode=1\n") || result_size <= filler_size ||
	    memcmp(result + result_size - sizeof(suffix) + 1, suffix,
	           sizeof(suffix) - 1)) {
		free(result);
		goto cleanup;
	}
	free(result);

	if (!write_fixture(path, (const unsigned char *)empty_options,
	                   sizeof(empty_options) - 1) ||
	    !playsave_text_update_section(path, "[D1X Options]", "[cockpit]",
		entries, sizeof(entries) / sizeof(entries[0])))
		goto cleanup;
	result = read_fixture(path, &result_size);
	if (!result || !strstr((char *)result,
		"[cockpit]\nmode=2\nrobothostagecounts=1\nbosshealthbar=0\nmapcheatsaccessible=1\n[end]\n[end]\n")) {
		free(result);
		goto cleanup;
	}
	free(result);

	if (!write_fixture(path, (const unsigned char *)malformed,
	                   sizeof(malformed) - 1) ||
	    !playsave_text_update_section(path, "[D1X Options]", "[cockpit]",
		entries, sizeof(entries) / sizeof(entries[0])))
		goto cleanup;
	result = read_fixture(path, &result_size);
	if (!result || !strstr((char *)result,
		"unknown=keep\nmode=2\nrobothostagecounts=1\nbosshealthbar=0\nmapcheatsaccessible=1\n[end]\n")) {
		free(result);
		goto cleanup;
	}
	free(result);
	ok = 1;

cleanup:
	remove(path);
	free(fixture);
	return ok ? 0 : 1;
}
