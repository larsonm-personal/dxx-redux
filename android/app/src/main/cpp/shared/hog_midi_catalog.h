#ifndef HOG_MIDI_CATALOG_H
#define HOG_MIDI_CATALOG_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../../../../XL_compat/hog_selection.h"

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
	char name[DXX_HOG_LONG_NAME_BYTES];
	uint32_t size;
	long data_offset;
};

struct hog_midi_catalog {
	struct hog_midi_entry *entries;
	size_t count;
	long file_size;
};

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

static inline size_t hog_catalog_read_header_bytes(void *context, unsigned char *out, size_t count)
{
	return fread(out, 1, count, (FILE *) context);
}

// Shared native headers; validate all names before publishing a filtered catalog
static inline int hog_catalog_load_filtered(const char *path, struct hog_midi_catalog *catalog,
                                            int (*accept_entry)(const char *name))
{
	FILE *file = NULL;
	struct hog_midi_entry *entries = NULL;
	struct dxx_hog_lookup *order = NULL;
	unsigned char *selected = NULL;
	size_t count = 0, capacity = 0, kept = 0, i;
	long file_size, position;
	unsigned char magic[DXX_HOG_SIGNATURE_BYTES];
	int status = HOG_MIDI_CATALOG_MALFORMED;

	if (!path || !catalog || !accept_entry) return HOG_MIDI_CATALOG_IO_ERROR;
	memset(catalog, 0, sizeof(*catalog));
	file = fopen(path, "rb");
	if (!file) return HOG_MIDI_CATALOG_IO_ERROR;
	if (fseek(file, 0, SEEK_END) || (file_size = ftell(file)) < (long) sizeof(magic) || fseek(file, 0, SEEK_SET)) goto done;
	if (fread(magic, 1, sizeof(magic), file) != sizeof(magic) || !dxx_hog_signature(magic)) goto done;
	position = sizeof(magic);
	while (position < file_size) {
		struct dxx_hog_header header;
		const char *error = NULL;
		long data_offset;
		if (dxx_hog_read_header(hog_catalog_read_header_bytes, file, &header, &error) != 1) goto done;
		data_offset = ftell(file);
		if (data_offset < position || data_offset > file_size || (uint64_t) header.size > (uint64_t) (file_size - data_offset)) goto done;
		if (count == 65536u) {
			status = HOG_MIDI_CATALOG_LIMIT;
			goto done;
		}
		if (count == capacity) {
			struct hog_midi_entry *grown;
			capacity = capacity ? capacity * 2u : 16u;
			grown = (struct hog_midi_entry *) realloc(entries, capacity * sizeof(*entries));
			if (!grown) {
				status = HOG_MIDI_CATALOG_ALLOCATION;
				goto done;
			}
			entries = grown;
		}
		memcpy(entries[count].name, header.name, sizeof(header.name));
		entries[count].size = header.size;
		entries[count].data_offset = data_offset;
		++count;
		if (header.size && fseek(file, (long) header.size, SEEK_CUR)) {
			status = HOG_MIDI_CATALOG_IO_ERROR;
			goto done;
		}
		position = data_offset + (long) header.size;
		if (ftell(file) != position) goto done;
	}
	if (position != file_size || ferror(file)) goto done;
	if (count) {
		order = (struct dxx_hog_lookup *) malloc(count * sizeof(*order));
		selected = (unsigned char *) malloc(count);
		if (!order || !selected) {
			status = HOG_MIDI_CATALOG_ALLOCATION;
			goto done;
		}
		for (i = 0; i < count; ++i) {
			order[i].name = entries[i].name;
			order[i].ordinal = i;
		}
		dxx_hog_select(order, count, selected);
	}
	for (i = 0; i < count; ++i) {
		if (!selected[i] || !accept_entry(entries[i].name)) continue;
		if (entries[i].size > HOG_MIDI_ENTRY_MAX_BYTES) goto done;
		if (kept == HOG_MIDI_MAX_TRACKS) {
			status = HOG_MIDI_CATALOG_LIMIT;
			goto done;
		}
		if (kept != i) entries[kept] = entries[i];
		++kept;
	}
	if (kept) {
		catalog->entries = (struct hog_midi_entry *) malloc(kept * sizeof(*entries));
		if (!catalog->entries) {
			status = HOG_MIDI_CATALOG_ALLOCATION;
			goto done;
		}
		memcpy(catalog->entries, entries, kept * sizeof(*entries));
	}
	catalog->count = kept;
	catalog->file_size = file_size;
	status = HOG_MIDI_CATALOG_OK;

done:
	free(selected);
	free(order);
	free(entries);
	if (fclose(file) && status == HOG_MIDI_CATALOG_OK) {
		hog_midi_catalog_free(catalog);
		status = HOG_MIDI_CATALOG_IO_ERROR;
	}
	return status;
}

static inline int hog_midi_accept_entry(const char *name)
{
	return hog_midi_has_extension(name, ".hmp") || hog_midi_has_extension(name, ".mid");
}

static inline int hog_midi_catalog_load(const char *path, struct hog_midi_catalog *catalog)
{
	return hog_catalog_load_filtered(path, catalog, hog_midi_accept_entry);
}

static inline int hog_midi_catalog_read(const char *path,
                                        const struct hog_midi_catalog *catalog, size_t index,
                                        unsigned char **data, int *length)
{
	const struct hog_midi_entry *entry;
	FILE *file;
	unsigned char *buffer;
	long current_size;
	int read_failed, close_failed;

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
	read_failed = fread(buffer, 1, entry->size, file) != entry->size || ferror(file);
	close_failed = fclose(file);
	if (read_failed || close_failed) {
		free(buffer);
		return 0;
	}
	*data = buffer;
	*length = (int) entry->size;
	return 1;
}

#endif
