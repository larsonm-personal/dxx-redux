/*
 * stuffit_extract.c - StuffIt 5 archive extraction for game data.
 *
 * The SIT5 container parser in this file is derived primarily from
 * XADMaster's StuffIt 5 support, especially XADStuffIt5Parser in The
 * Unarchiver, which is distributed under LGPL-2.1. The nested STi handoff
 * uses the STi parser and decompressors in sti2_extract.c. Behavior is
 * cross-checked against ssokolow/stuffit-test-files and the PC-side unar
 * oracle hashes for the Descent demo installers.
 * https://github.com/ashang/unar
 * https://github.com/ssokolow/stuffit-test-files
 */

#include "stuffit_extract.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir_one(path, mode) _mkdir(path)
#else
#include <strings.h>
#include <sys/stat.h>
#define mkdir_one(path, mode) mkdir((path), (mode))
#endif

#define SIT5_ENTRY_ID          0xa5a5a5a5u
#define SIT5_ARCHIVE_SIZE      100u
#define SIT5_ARCHIVE_VER       5u
#define SIT5_FLAG_DIR          0x40u
#define SIT5_FLAG_CRYPTED      0x20u
#define SIT5_NESTED_NAME_LIMIT 64

typedef struct {
	unsigned int next_offset;
	unsigned int child_count;
	int skip_entry;
	sti2_entry_t entry;
} sit5_parsed_entry_t;

typedef struct {
	unsigned int offset;
	char path[STI2_PATH_LEN];
} sit5_directory_t;

static unsigned int be16(const unsigned char *p)
{
	return ((unsigned int) p[0] << 8) | (unsigned int) p[1];
}

static unsigned int be32(const unsigned char *p)
{
	return ((unsigned int) p[0] << 24) |
	       ((unsigned int) p[1] << 16) |
	       ((unsigned int) p[2] << 8) |
	       (unsigned int) p[3];
}

static const char *basename_only(const char *path)
{
	const char *base = path;
	const char *p;

	if (!path) return "";
	for (p = path; *p; p++) {
		if (*p == '/' || *p == '\\') base = p + 1;
	}
	return base;
}

static int ext_matches(const char *name, const char **extensions)
{
	const char *dot;

	if (!extensions) return 1;
	dot = strrchr(name, '.');
	if (!dot || !dot[1]) return 0;
	dot++;
	for (const char **e = extensions; *e; e++) {
#ifdef _WIN32
		if (_stricmp(dot, *e) == 0) return 1;
#else
		if (strcasecmp(dot, *e) == 0) return 1;
#endif
	}
	return 0;
}

static int read_file_to_buffer(const char *path, unsigned char **out_data,
                               size_t *out_size)
{
	FILE *f;
	long len;
	unsigned char *data;

	if (!path || !out_data || !out_size) return -1;
	*out_data = NULL;
	*out_size = 0;
	f = fopen(path, "rb");
	if (!f) return -1;
	fseek(f, 0, SEEK_END);
	len = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (len < 0) {
		fclose(f);
		return -1;
	}
	data = (unsigned char *) malloc((size_t) len);
	if (!data && len != 0) {
		fclose(f);
		return -1;
	}
	if (len != 0 && fread(data, 1, (size_t) len, f) != (size_t) len) {
		free(data);
		fclose(f);
		return -1;
	}
	fclose(f);
	*out_data = data;
	*out_size = (size_t) len;
	return 0;
}

static int mkdirs_for_path(const char *path)
{
	char tmp[STI2_PATH_LEN * 2];
	char *p;

	if (!path || !path[0]) return -1;
	snprintf(tmp, sizeof(tmp), "%s", path);
	for (p = tmp + 1; *p; p++) {
		if (*p == '/' || *p == '\\') {
			char saved = *p;
			*p = '\0';
			mkdir_one(tmp, 0755);
			*p = saved;
		}
	}
	mkdir_one(tmp, 0755);
	return 0;
}

static void sanitize_temp_name(const char *src, char *dst, int dst_len)
{
	int out_len = 0;

	while (src && *src && out_len < dst_len - 1) {
		char c = *src++;
		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
		    (c >= '0' && c <= '9') || c == '.' || c == '_')
			dst[out_len++] = c;
		else
			dst[out_len++] = '_';
	}
	dst[out_len] = '\0';
}

static int sit5_is_archive(const unsigned char *archive_data, size_t archive_size)
{
	const char *match = "StuffIt (c)1997-\377\377\377\377 Aladdin Systems, Inc., http://www.aladdinsys.com/StuffIt/\r\n";

	if (!archive_data || archive_size < SIT5_ARCHIVE_SIZE) return 0;
	while (*match) {
		if (*match != '\377' && *archive_data != (unsigned char) *match)
			return 0;
		match++;
		archive_data++;
	}
	return archive_data[2] == SIT5_ARCHIVE_VER;
}

static int sit5_build_path(const char *parent, const unsigned char *name_bytes,
                           unsigned int name_len, char *out, int out_len)
{
	char name[STI2_PATH_LEN];
	unsigned int i;
	int name_out = 0;

	for (i = 0; i < name_len && name_out < (int) sizeof(name) - 1; i++) {
		unsigned char c = name_bytes[i];
		if (c == '/' || c == '\\')
			name[name_out++] = '_';
		else if (c >= 32 && c <= 126)
			name[name_out++] = (char) c;
		else
			name[name_out++] = '?';
	}
	name[name_out] = '\0';
	if (!parent || !parent[0])
		snprintf(out, out_len, "%s", name);
	else
		snprintf(out, out_len, "%s/%s", parent, name);
	return out[0] ? 0 : -1;
}

static int sit5_parse_entry(const unsigned char *archive_data, size_t archive_size,
                            unsigned int entry_offset, const char *parent,
                            sit5_parsed_entry_t *out)
{
	unsigned int flags;
	unsigned int version;
	unsigned int header_size;
	unsigned int header_end;
	unsigned int name_len;
	unsigned int data_len;
	unsigned int comp_len;
	unsigned int cursor;
	unsigned int data_start;
	unsigned int resource_len = 0;
	unsigned int resource_comp_len = 0;
	unsigned int data_method = 0;
	unsigned int pass_len;
	unsigned int bitfield;
	unsigned int child_count = 0;
	int is_dir;

	if (!archive_data || !out || entry_offset + 48u > archive_size) return -1;
	if (be32(archive_data + entry_offset) != SIT5_ENTRY_ID) return -1;
	version = archive_data[entry_offset + 4];
	if (version != 1u && version != 3u) return -1;
	memset(out, 0, sizeof(*out));
	flags = archive_data[entry_offset + 9];
	header_size = be16(archive_data + entry_offset + 6);
	header_end = entry_offset + header_size;
	name_len = be16(archive_data + entry_offset + 30);
	data_len = be32(archive_data + entry_offset + 34);
	comp_len = be32(archive_data + entry_offset + 38);
	is_dir = (flags & SIT5_FLAG_DIR) != 0;
	if (header_size < 48u || header_end > archive_size) return -1;
	cursor = entry_offset + 48u;
	if (is_dir) {
		child_count = be16(archive_data + entry_offset + 46);
		if (data_len == 0xffffffffu) {
			out->skip_entry = 1;
			out->next_offset = cursor;
			return 0;
		}
	} else {
		data_method = archive_data[entry_offset + 46];
		pass_len = archive_data[entry_offset + 47];
		if ((flags & SIT5_FLAG_CRYPTED) || pass_len != 0u) return -1;
	}
	if (cursor + name_len > header_end) return -1;
	if (sit5_build_path(parent, archive_data + cursor, name_len,
	                    out->entry.path, sizeof(out->entry.path)) < 0)
		return -1;
	cursor += name_len;
	if (cursor < header_end) {
		unsigned int comment_size;

		if (cursor + 4u > header_end) return -1;
		comment_size = be16(archive_data + cursor);
		cursor += 4u;
		if (cursor + comment_size > header_end) return -1;
		cursor += comment_size;
	}
	cursor = header_end;
	if (cursor + 32u > archive_size) return -1;
	bitfield = be16(archive_data + cursor);
	cursor += 4u;
	out->entry.header_offset = entry_offset;
	out->entry.file_type = be32(archive_data + cursor);
	cursor += 4u;
	out->entry.creator = be32(archive_data + cursor);
	cursor += 4u;
	out->entry.finder_flags = be16(archive_data + cursor);
	cursor += 2u;
	cursor += (version == 1u) ? 22u : 18u;
	if (!is_dir && (bitfield & 1u)) {
		if (cursor + 14u > archive_size) return -1;
		resource_len = be32(archive_data + cursor);
		resource_comp_len = be32(archive_data + cursor + 4);
		pass_len = archive_data[cursor + 13];
		if ((flags & SIT5_FLAG_CRYPTED) || pass_len != 0u) return -1;
		cursor += 14u;
	}
	data_start = cursor;
	if (is_dir) {
		data_len = 0u;
		comp_len = 0u;
		resource_len = 0u;
		resource_comp_len = 0u;
	}
	if (data_start > archive_size || resource_comp_len > archive_size - data_start)
		return -1;
	if (comp_len > archive_size - data_start - resource_comp_len)
		return -1;
	out->next_offset = data_start + resource_comp_len + comp_len;
	out->entry.data_offset = data_start + resource_comp_len;
	out->entry.compressed_size = comp_len;
	out->entry.uncompressed_size = data_len;
	out->entry.resource_offset = 0u;
	out->entry.resource_compressed_size = resource_comp_len;
	out->entry.resource_uncompressed_size = resource_len;
	out->entry.data_method = is_dir ? 0u : data_method;
	out->entry.resource_method = 0u;
	out->entry.is_directory = is_dir;
	out->child_count = child_count;
	return 0;
}

static const char *sit5_find_parent(const sit5_directory_t *dirs, int dir_count,
                                    unsigned int parent_offset)
{
	int i;

	for (i = dir_count - 1; i >= 0; i--) {
		if (dirs[i].offset == parent_offset)
			return dirs[i].path;
	}
	return "";
}

static int sit5_parse_entries(const unsigned char *archive_data, size_t archive_size,
                              unsigned int entry_offset, unsigned int entry_count,
                              sti2_entry_list_t *out)
{
	sit5_directory_t dirs[STI2_MAX_ENTRIES];
	unsigned int current_offset = entry_offset;
	unsigned int entries_to_read = entry_count;
	unsigned int i;
	int dir_count = 0;

	for (i = 0; i < entries_to_read; i++) {
		sit5_parsed_entry_t parsed;
		unsigned int parent_offset;
		const char *parent;

		if (current_offset >= archive_size) return -1;
		parent_offset = be32(archive_data + current_offset + 26);
		parent = sit5_find_parent(dirs, dir_count, parent_offset);
		if (sit5_parse_entry(archive_data, archive_size, current_offset, parent, &parsed) < 0) return -1;
		if (!parsed.skip_entry) {
			if (out->num_entries >= STI2_MAX_ENTRIES) return -1;
			out->entries[out->num_entries++] = parsed.entry;
			if (parsed.entry.is_directory) {
				if (dir_count >= STI2_MAX_ENTRIES) return -1;
				dirs[dir_count].offset = parsed.entry.header_offset;
				snprintf(dirs[dir_count].path, sizeof(dirs[dir_count].path), "%s", parsed.entry.path);
				dir_count++;
				entries_to_read += parsed.child_count;
			}
		} else {
			entries_to_read++;
		}
		if (parsed.next_offset <= current_offset || parsed.next_offset > archive_size) return -1;
		current_offset = parsed.next_offset;
	}
	return 0;
}

static int sit5_list_entries(const unsigned char *archive_data, size_t archive_size,
                             sti2_entry_list_t *out)
{
	unsigned int root_count;
	unsigned int first_offset;

	if (!out || !sit5_is_archive(archive_data, archive_size)) return -1;
	memset(out, 0, sizeof(*out));
	root_count = be16(archive_data + 92);
	first_offset = be32(archive_data + 94);
	out->declared_file_count = root_count;
	out->declared_total_size = (unsigned int) archive_size;
	if (root_count == 0u || first_offset >= archive_size) return -1;
	if (sit5_parse_entries(archive_data, archive_size, first_offset, root_count,
	                       out) < 0)
		return -1;
	return out->num_entries;
}

static int maybe_extract_nested_sti(const unsigned char *sit_data, size_t sit_size,
                                    const sti2_entry_t *entry,
                                    const char *output_dir,
                                    const char **extensions,
                                    sti2_progress_fn progress, void *user_data)
{
	char safe_name[SIT5_NESTED_NAME_LIMIT];
	char temp_path[STI2_PATH_LEN * 2];
	unsigned char *nested_data = NULL;
	size_t nested_size = 0;
	int extracted = 0;

	if (entry->is_directory || entry->uncompressed_size < 22u) return 0;
	sanitize_temp_name(basename_only(entry->path), safe_name, sizeof(safe_name));
	snprintf(temp_path, sizeof(temp_path), "%s/.stuffit_nested_%s", output_dir, safe_name);
	if (sti2_extract_entry(sit_data, sit_size, entry, temp_path) < 0) return 0;
	if (read_file_to_buffer(temp_path, &nested_data, &nested_size) == 0 &&
	    sti2_is_archive(nested_data, nested_size)) {
		extracted = sti2_extract_matching(nested_data, nested_size, extensions,
		                                  output_dir, progress, user_data);
		if (extracted < 0) extracted = 0;
	}
	free(nested_data);
	remove(temp_path);
	return extracted;
}

int stuffit_extract(const char *sit_path, const char *output_dir,
                    const char **extensions,
                    sti2_progress_fn progress, void *user_data)
{
	unsigned char *archive_data = NULL;
	size_t archive_size = 0;
	sti2_entry_list_t list;
	int extracted = 0;

	if (!sit_path || !output_dir) return -1;
	if (read_file_to_buffer(sit_path, &archive_data, &archive_size) < 0) return -1;
	if (sit5_list_entries(archive_data, archive_size, &list) < 0) {
		free(archive_data);
		return -1;
	}
	if (mkdirs_for_path(output_dir) < 0) {
		free(archive_data);
		return -1;
	}
	for (int i = 0; i < list.num_entries; i++) {
		char output_path[STI2_PATH_LEN * 2];
		const char *name;
		int written;

		if (list.entries[i].is_directory) continue;
		name = basename_only(list.entries[i].path);
		if (ext_matches(name, extensions)) {
			snprintf(output_path, sizeof(output_path), "%s/%s", output_dir, name);
			written = sti2_extract_entry(archive_data, archive_size, &list.entries[i], output_path);
			if (written < 0) {
				free(archive_data);
				return -1;
			}
			extracted++;
			if (progress && progress(name, written, written, user_data) != 0) {
				free(archive_data);
				return -1;
			}
		} else {
			extracted += maybe_extract_nested_sti(archive_data, archive_size,
			                                      &list.entries[i], output_dir,
			                                      extensions, progress, user_data);
		}
	}
	free(archive_data);
	return extracted;
}