#ifndef HOG_MIDI_CATALOG_H
#define HOG_MIDI_CATALOG_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define hog_catalog_strcasecmp _stricmp
#else
#include <strings.h>
#define hog_catalog_strcasecmp strcasecmp
#endif

#define HOG_MIDI_ENTRY_MAX_BYTES (64u * 1024u * 1024u)
#define HOG_MIDI_MAX_TRACKS      4096u

enum hog_midi_catalog_status {
	HOG_MIDI_CATALOG_OK = 1,
	HOG_MIDI_CATALOG_IO_ERROR = -1,
	HOG_MIDI_CATALOG_MALFORMED = -2,
	HOG_MIDI_CATALOG_LIMIT = -3,
	HOG_MIDI_CATALOG_ALLOCATION = -4
};

struct hog_midi_entry {
	char name[14];
	uint32_t size;
	long data_offset;
};

struct hog_midi_catalog {
	struct hog_midi_entry *entries;
	size_t count;
	long file_size;
};

static inline uint32_t hog_midi_read_le32(const unsigned char *bytes)
{
	return (uint32_t) bytes[0] | ((uint32_t) bytes[1] << 8) |
	       ((uint32_t) bytes[2] << 16) | ((uint32_t) bytes[3] << 24);
}

static inline int hog_midi_has_extension(const char *name, const char *extension)
{
	size_t name_length = strlen(name);
	size_t extension_length = strlen(extension);

	return name_length > extension_length &&
	       !hog_catalog_strcasecmp(name + name_length - extension_length, extension);
}

static inline const char *hog_midi_catalog_status_name(int status)
{
	switch (status) {
		case HOG_MIDI_CATALOG_IO_ERROR: return "io_error";
		case HOG_MIDI_CATALOG_MALFORMED: return "malformed_hog";
		case HOG_MIDI_CATALOG_LIMIT: return "track_limit_exceeded";
		case HOG_MIDI_CATALOG_ALLOCATION: return "allocation_failed";
		default: return "ok";
	}
}

static inline void hog_midi_catalog_free(struct hog_midi_catalog *catalog)
{
	if (!catalog)
		return;
	free(catalog->entries);
	memset(catalog, 0, sizeof(*catalog));
}

static inline int hog_midi_catalog_load(const char *path, struct hog_midi_catalog *catalog)
{
	FILE *file = NULL;
	struct hog_midi_entry *entries = NULL;
	size_t count = 0;
	size_t capacity = 0;
	long file_size;
	long position;
	char magic[3];
	int status = HOG_MIDI_CATALOG_MALFORMED;

	if (!path || !catalog)
		return HOG_MIDI_CATALOG_IO_ERROR;
	memset(catalog, 0, sizeof(*catalog));
	file = fopen(path, "rb");
	if (!file)
		return HOG_MIDI_CATALOG_IO_ERROR;
	if (fseek(file, 0, SEEK_END) || (file_size = ftell(file)) < 3 ||
	    fseek(file, 0, SEEK_SET))
		goto done;
	if (fread(magic, 1, sizeof(magic), file) != sizeof(magic) ||
	    memcmp(magic, "DHF", sizeof(magic)))
		goto done;
	position = 3;
	while (position < file_size) {
		unsigned char header[17];
		char name[14];
		uint32_t entry_size;
		long data_offset;
		long remaining;

		if (file_size - position < (long) sizeof(header) ||
		    fread(header, 1, sizeof(header), file) != sizeof(header))
			goto done;
		entry_size = hog_midi_read_le32(header + 13);
		data_offset = position + (long) sizeof(header);
		remaining = file_size - data_offset;
		if (entry_size > HOG_MIDI_ENTRY_MAX_BYTES ||
		    (uint64_t) entry_size > (uint64_t) remaining)
			goto done;
		memcpy(name, header, 13);
		name[13] = '\0';

		if (hog_midi_has_extension(name, ".hmp") ||
		    hog_midi_has_extension(name, ".mid")) {
			struct hog_midi_entry *grown;
			size_t next_capacity;

			if (count == HOG_MIDI_MAX_TRACKS) {
				status = HOG_MIDI_CATALOG_LIMIT;
				goto done;
			}
			if (count == capacity) {
				next_capacity = capacity ? capacity * 2u : 16u;
				if (next_capacity > HOG_MIDI_MAX_TRACKS)
					next_capacity = HOG_MIDI_MAX_TRACKS;
				grown = (struct hog_midi_entry *) realloc(
				    entries, next_capacity * sizeof(*entries));
				if (!grown) {
					status = HOG_MIDI_CATALOG_ALLOCATION;
					goto done;
				}
				entries = grown;
				capacity = next_capacity;
			}
			memcpy(entries[count].name, name, sizeof(name));
			entries[count].size = entry_size;
			entries[count].data_offset = data_offset;
			count++;
		}
		if (entry_size && fseek(file, (long) entry_size, SEEK_CUR)) {
			status = HOG_MIDI_CATALOG_IO_ERROR;
			goto done;
		}
		position = data_offset + (long) entry_size;
		if (ftell(file) != position) {
			status = HOG_MIDI_CATALOG_IO_ERROR;
			goto done;
		}
	}
	if (position != file_size || ferror(file))
		goto done;
	catalog->entries = entries;
	catalog->count = count;
	catalog->file_size = file_size;
	entries = NULL;
	status = HOG_MIDI_CATALOG_OK;

done:
	free(entries);
	if (fclose(file) && status == HOG_MIDI_CATALOG_OK) {
		hog_midi_catalog_free(catalog);
		status = HOG_MIDI_CATALOG_IO_ERROR;
	}
	return status;
}

static inline int hog_midi_catalog_read(const char *path,
                                        const struct hog_midi_catalog *catalog, size_t index,
                                        unsigned char **data, int *length)
{
	const struct hog_midi_entry *entry;
	FILE *file;
	unsigned char *buffer;
	long current_size;

	if (!path || !catalog || index >= catalog->count || !data || !length)
		return 0;
	*data = NULL;
	*length = 0;
	entry = &catalog->entries[index];
	if (!entry->size)
		return 0;
	file = fopen(path, "rb");
	if (!file)
		return 0;
	if (fseek(file, 0, SEEK_END) || (current_size = ftell(file)) != catalog->file_size ||
	    fseek(file, entry->data_offset, SEEK_SET)) {
		fclose(file);
		return 0;
	}
	buffer = (unsigned char *) malloc(entry->size);
	if (!buffer) {
		fclose(file);
		return 0;
	}
	if (fread(buffer, 1, entry->size, file) != entry->size || ferror(file) ||
	    fclose(file)) {
		free(buffer);
		return 0;
	}
	*data = buffer;
	*length = (int) entry->size;
	return 1;
}

#endif
