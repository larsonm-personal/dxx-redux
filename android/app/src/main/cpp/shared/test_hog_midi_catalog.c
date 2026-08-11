#include "hog_midi_catalog.h"

#include <stdio.h>

static int fail(const char *message)
{
	fprintf(stderr, "%s\n", message);
	return 1;
}

static int write_header(FILE *file)
{
	return fwrite("DHF", 1, 3, file) == 3;
}

static int write_entry(FILE *file, const char *name,
                       const unsigned char *data, uint32_t size)
{
	unsigned char header[17] = { 0 };
	size_t name_length = strlen(name);

	if (name_length > 13)
		return 0;
	memcpy(header, name, name_length);
	header[13] = (unsigned char) (size & 0xffu);
	header[14] = (unsigned char) ((size >> 8) & 0xffu);
	header[15] = (unsigned char) ((size >> 16) & 0xffu);
	header[16] = (unsigned char) ((size >> 24) & 0xffu);
	return fwrite(header, 1, sizeof(header), file) == sizeof(header) &&
	       (!size || fwrite(data, 1, size, file) == size);
}

static int create_mixed_hog(const char *path, unsigned int tracks)
{
	static const unsigned char payload[] = { 1, 2, 3 };
	FILE *file = fopen(path, "wb");
	unsigned int index;
	int result = 0;

	if (!file || !write_header(file))
		goto done;
	if (!write_entry(file, "readme.txt", payload, sizeof(payload)))
		goto done;
	for (index = 0; index < tracks; index++) {
		char name[14];
		snprintf(name, sizeof(name), "t%04u.%s", index,
		         index % 2u ? "MID" : "hMp");
		if (!write_entry(file, name, payload, sizeof(payload)))
			goto done;
	}
	result = 1;

done:
	if (file && fclose(file))
		result = 0;
	return result;
}

int main(void)
{
	static const char mixed_path[] = "test_hog_midi_catalog_mixed.hog";
	static const char malformed_path[] = "test_hog_midi_catalog_malformed.hog";
	static const char over_limit_path[] = "test_hog_midi_catalog_limit.hog";
	struct hog_midi_catalog catalog;
	unsigned char *data = NULL;
	int length = 0;
	FILE *file;
	int result = 1;

	remove(mixed_path);
	remove(malformed_path);
	remove(over_limit_path);
	if (!create_mixed_hog(mixed_path, 65u) ||
	    hog_midi_catalog_load(mixed_path, &catalog) != HOG_MIDI_CATALOG_OK)
		goto done;
	if (catalog.count != 65u || strcmp(catalog.entries[0].name, "t0000.hMp") ||
	    strcmp(catalog.entries[1].name, "t0001.MID") ||
	    strcmp(catalog.entries[64].name, "t0064.hMp")) {
		hog_midi_catalog_free(&catalog);
		goto done;
	}
	if (!hog_midi_catalog_read(mixed_path, &catalog, 64u, &data, &length) ||
	    length != 3 || data[0] != 1 || data[2] != 3) {
		hog_midi_catalog_free(&catalog);
		goto done;
	}
	free(data);
	data = NULL;
	hog_midi_catalog_free(&catalog);

	if (!create_mixed_hog(malformed_path, 1u))
		goto done;
	file = fopen(malformed_path, "ab");
	if (!file || fputc('x', file) == EOF || fclose(file))
		goto done;
	if (hog_midi_catalog_load(malformed_path, &catalog) !=
	        HOG_MIDI_CATALOG_MALFORMED ||
	    catalog.entries || catalog.count)
		goto done;

	if (!create_mixed_hog(over_limit_path, HOG_MIDI_MAX_TRACKS + 1u))
		goto done;
	if (hog_midi_catalog_load(over_limit_path, &catalog) != HOG_MIDI_CATALOG_LIMIT ||
	    catalog.entries || catalog.count)
		goto done;

	result = 0;
	puts("PASS");

done:
	free(data);
	remove(mixed_path);
	remove(malformed_path);
	remove(over_limit_path);
	return result ? fail("HOG MIDI catalog test failed") : 0;
}
