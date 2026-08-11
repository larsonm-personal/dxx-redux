#include "music_track_json.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static int serialize(char *buffer, size_t capacity, size_t count, const char *name)
{
	music_track_json_writer writer;
	size_t i;
	music_track_json_begin(&writer, buffer, capacity);
	for (i = 0; i < count; ++i)
		music_track_json_add(&writer, (int) i, name);
	return music_track_json_finish(&writer);
}

int main(void)
{
	static const size_t catalog_sizes[] = { 100, 128, 256, 1024 };
	char empty[3];
	char guarded[64];
	char escaped[128];
	char *large;
	int required;
	size_t catalog_index;

	assert(serialize(empty, sizeof(empty), 0, "") == 2);
	assert(strcmp(empty, "[]") == 0);

	required = serialize(NULL, 0, 1, "quote\" slash\\ line\n");
	assert(required > 0 && (size_t) required < sizeof(escaped));
	assert(serialize(escaped, (size_t) required + 1, 1, "quote\" slash\\ line\n") == required);
	assert(strcmp(escaped, "[{\"index\":0,\"name\":\"quote\\\" slash\\\\ line\\n\"}]") == 0);

	memset(guarded, 'G', sizeof(guarded));
	assert(serialize(guarded + 8, (size_t) required, 1, "quote\" slash\\ line\n") == -1);
	assert(guarded[8] == '\0');
	assert(guarded[7] == 'G');
	assert(guarded[8 + required] == 'G');

	for (catalog_index = 0; catalog_index < sizeof(catalog_sizes) / sizeof(catalog_sizes[0]); ++catalog_index) {
		required = serialize(NULL, 0, catalog_sizes[catalog_index], "a very long \\\" track name.ogg");
		assert(required > 0);
		if (catalog_sizes[catalog_index] == 1024)
			assert(required > 32768);
		large = (char *) malloc((size_t) required + 1);
		assert(large);
		assert(serialize(large, (size_t) required + 1, catalog_sizes[catalog_index],
		                 "a very long \\\" track name.ogg") == required);
		assert(large[0] == '[' && large[required - 1] == ']' && large[required] == '\0');
		free(large);
	}
	return 0;
}
