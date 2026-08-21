#include "midi_metadata_physfs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <physfs.h>

static int has_extension_ci(const char *filename, const char *extension)
{
	size_t filename_length = strlen(filename);
	size_t extension_length = strlen(extension);
	size_t i;
	if (filename_length <= extension_length)
		return 0;
	filename += filename_length - extension_length;
	for (i = 0; i < extension_length; ++i) {
		char a = filename[i], b = extension[i];
		if (a >= 'A' && a <= 'Z') a = (char) (a - 'A' + 'a');
		if (b >= 'A' && b <= 'Z') b = (char) (b - 'A' + 'a');
		if (a != b) return 0;
	}
	return 1;
}

static int strings_equal_ci_n(const char *left, const char *right, size_t length)
{
	size_t i;
	for (i = 0; i < length; ++i) {
		char a = left[i], b = right[i];
		if (a >= 'A' && a <= 'Z') a = (char) (a - 'A' + 'a');
		if (b >= 'A' && b <= 'Z') b = (char) (b - 'A' + 'a');
		if (a != b) return 0;
	}
	return 1;
}

static int find_midi_peer(const char *filename, char *peer, size_t peer_size)
{
	const char *slash = strrchr(filename, '/');
	const char *leaf = slash ? slash + 1 : filename;
	const size_t directory_length = slash ? (size_t) (slash - filename) : 0u;
	const size_t leaf_length = strlen(leaf);
	const size_t stem_length = leaf_length - 4u;
	char directory[MIDI_METADATA_SOURCE_FILENAME_BYTES];
	char **entries, **entry;
	if (directory_length >= sizeof(directory))
		return 0;
	if (directory_length) {
		memcpy(directory, filename, directory_length);
		directory[directory_length] = '\0';
	} else {
		directory[0] = '\0';
	}
	entries = PHYSFS_enumerateFiles(directory);
	if (!entries)
		return 0;
	for (entry = entries; *entry; ++entry) {
		const size_t entry_length = strlen(*entry);
		int written;
		if (entry_length != stem_length + 4u ||
		    !strings_equal_ci_n(*entry, leaf, stem_length) ||
		    !has_extension_ci(*entry, ".mid"))
			continue;
		if (directory_length)
			written = snprintf(peer, peer_size, "%s/%s", directory, *entry);
		else
			written = snprintf(peer, peer_size, "%s", *entry);
		PHYSFS_freeList(entries);
		return written > 0 && (size_t) written < peer_size;
	}
	PHYSFS_freeList(entries);
	return 0;
}

int midi_metadata_read_physfs(const char *filename, midi_metadata *metadata)
{
	PHYSFS_File *file;
	PHYSFS_sint64 length, read;
	unsigned char *bytes;
	int status;
	if (!filename || !metadata)
		return MIDI_METADATA_INVALID;
	file = PHYSFS_openRead(filename);
	if (!file) {
		midi_metadata_free(metadata);
		metadata->status = MIDI_METADATA_INVALID;
		return metadata->status;
	}
	length = PHYSFS_fileLength(file);
	if (length < 0 || (PHYSFS_uint64) length > MIDI_METADATA_MAX_INPUT_BYTES) {
		PHYSFS_close(file);
		midi_metadata_free(metadata);
		metadata->status = MIDI_METADATA_LIMIT;
		return metadata->status;
	}
	bytes = (unsigned char *) malloc((size_t) length);
	if (!bytes) {
		PHYSFS_close(file);
		midi_metadata_free(metadata);
		metadata->status = MIDI_METADATA_ALLOCATION;
		return metadata->status;
	}
	read = PHYSFS_readBytes(file, bytes, (PHYSFS_uint64) length);
	if (!PHYSFS_close(file) || read != length) {
		free(bytes);
		midi_metadata_free(metadata);
		metadata->status = MIDI_METADATA_INVALID;
		return metadata->status;
	}
	status = midi_metadata_parse(bytes, (size_t) length,
	                             has_extension_ci(filename, ".hmp") ||
	                                 has_extension_ci(filename, ".hmq"),
	                             metadata);
	free(bytes);
	return status;
}

int midi_metadata_resolve_physfs(const char *filename, midi_metadata *metadata,
                                 char *source_filename, size_t source_filename_size,
                                 int *inherited_from_midi)
{
	char peer[MIDI_METADATA_SOURCE_FILENAME_BYTES];
	midi_metadata peer_metadata;
	int status;
	if (!filename || !metadata || !source_filename || !source_filename_size ||
	    !inherited_from_midi)
		return MIDI_METADATA_INVALID;
	source_filename[0] = '\0';
	*inherited_from_midi = 0;
	status = midi_metadata_read_physfs(filename, metadata);
	if (status == MIDI_METADATA_OK && midi_metadata_has_text(metadata)) {
		snprintf(source_filename, source_filename_size, "%s", filename);
		return status;
	}
	if (!has_extension_ci(filename, ".hmp")) {
		if (status == MIDI_METADATA_OK)
			snprintf(source_filename, source_filename_size, "%s", filename);
		return status;
	}
	if (!find_midi_peer(filename, peer, sizeof(peer)))
		return status;
	midi_metadata_init(&peer_metadata);
	if (midi_metadata_read_physfs(peer, &peer_metadata) == MIDI_METADATA_OK &&
	    midi_metadata_has_text(&peer_metadata)) {
		const int duration_ms = metadata->duration_ms;
		midi_metadata_free(metadata);
		*metadata = peer_metadata;
		metadata->duration_ms = duration_ms;
		snprintf(source_filename, source_filename_size, "%s", peer);
		*inherited_from_midi = 1;
		return MIDI_METADATA_OK;
	}
	midi_metadata_free(&peer_metadata);
	return status;
}
