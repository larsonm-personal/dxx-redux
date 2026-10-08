#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#endif

enum read_fault { READ_NORMAL,
	              READ_SHORT,
	              READ_ERROR,
	              CLOSE_ERROR };
static enum read_fault fault;
static FILE *payload_file;
static unsigned int opens, closes;
#ifdef _WIN32
static HANDLE payload_handle;
#else
static int payload_descriptor;
#endif

static FILE *payload_open(const char *path, const char *mode)
{
	FILE *file = fopen(path, mode);
	if (file) {
		payload_file = file;
		opens++;
#ifdef _WIN32
		payload_handle = (HANDLE) _get_osfhandle(_fileno(file));
#else
		payload_descriptor = fileno(file);
#endif
	}
	return file;
}

static size_t payload_read(void *buffer, size_t size, size_t count, FILE *file)
{
	return fread(buffer, size, fault == READ_SHORT && count ? count - 1 : count, file);
}

static int payload_error(FILE *file)
{
	return fault == READ_ERROR || ferror(file);
}

static int payload_close(FILE *file)
{
	int result = fclose(file);
	payload_file = NULL;
	closes++;
	return fault == CLOSE_ERROR ? EOF : result;
}

// Interpose only the production header; fixture setup and leak cleanup use real stdio
#define fopen  payload_open
#define fread  payload_read
#define ferror payload_error
#define fclose payload_close
#include "hog_midi_catalog.h"
#undef fopen
#undef fread
#undef ferror
#undef fclose

static int payload_descriptor_closed(void)
{
#ifdef _WIN32
	DWORD flags;
	return !GetHandleInformation(payload_handle, &flags) && GetLastError() == ERROR_INVALID_HANDLE;
#else
	errno = 0;
	return fcntl(payload_descriptor, F_GETFD) == -1 && errno == EBADF;
#endif
}

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

static int read_lifetime_tests(void)
{
	static const char path[] = "test_hog_midi_catalog_read_lifetime.hog";
	struct hog_midi_catalog catalog;
	unsigned char *data;
	int length, loaded, descriptor_closed, result = 1;
	unsigned int mode;
	if (!create_mixed_hog(path, 1u))
		goto done;
	if (hog_midi_catalog_load(path, &catalog) != HOG_MIDI_CATALOG_OK)
		goto done;
	for (mode = READ_NORMAL; mode <= CLOSE_ERROR; mode++) {
		fault = (enum read_fault) mode;
		opens = closes = 0;
		data = (unsigned char *) &catalog;
		length = -1;
		loaded = hog_midi_catalog_read(path, &catalog, 0, &data, &length);
		descriptor_closed = payload_descriptor_closed();
		printf("HOG payload lifetime mode %u: opens=%u closes=%u descriptor_closed=%d loaded=%d length=%d\n",
		       mode, opens, closes, descriptor_closed, loaded, length);
		if (opens != 1 || closes != 1 || !descriptor_closed ||
		    (mode == READ_NORMAL ? (!loaded || !data || length != 3 ||
		                            data[0] != 1 || data[1] != 2 || data[2] != 3)
		                         : (loaded || data || length))) {
			fprintf(stderr, "HOG payload lifetime mode %u failed: opens=%u closes=%u loaded=%d length=%d\n",
			        mode, opens, closes, loaded, length);
			if (loaded) free(data);
			if (payload_file) {
				fclose(payload_file);
				payload_file = NULL;
			}
			hog_midi_catalog_free(&catalog);
			goto done;
		}
		if (loaded) free(data);
	}
	hog_midi_catalog_free(&catalog);
	puts("HOG payload read lifetime passed: success, short read, read error, close error");
	result = 0;
done:
	fault = READ_NORMAL;
	remove(path);
	return result;
}

int main(int argc, char **argv)
{
	if (argc == 2 && !strcmp(argv[1], "--read-lifetime")) return read_lifetime_tests();
	if (argc != 1) return 2;
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
