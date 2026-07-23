#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "extract_limits.h"
#include "hfs_reader.h"
#include "mac_hfs_extract.h"
#include "sti2_extract.h"

static const char *path_basename(const char *path)
{
	const char *last = path;

	while (*path) {
		if (*path == '/' || *path == '\\')
			last = path + 1;
		path++;
	}

	return last;
}

static int str_equals_ignore_case(const char *a, const char *b)
{
	while (*a && *b) {
		char ca = *a;
		char cb = *b;

		if (ca >= 'A' && ca <= 'Z') ca = (char) (ca - 'A' + 'a');
		if (cb >= 'A' && cb <= 'Z') cb = (char) (cb - 'A' + 'a');
		if (ca != cb)
			return 0;
		a++;
		b++;
	}

	return *a == '\0' && *b == '\0';
}

static int ext_matches(const char *filename, const char **extensions)
{
	const char *dot;

	if (!extensions)
		return 1;

	dot = strrchr(filename, '.');
	if (!dot || !dot[1])
		return 0;
	dot++;

	while (*extensions) {
		if (str_equals_ignore_case(dot, *extensions))
			return 1;
		extensions++;
	}

	return 0;
}

static int read_file_to_buffer(const char *path, unsigned char **out_data, size_t *out_size)
{
	FILE *f;
	long len;
	unsigned char *data;

	if (!out_data || !out_size)
		return -1;
	*out_data = NULL;
	*out_size = 0;

	f = fopen(path, "rb");
	if (!f)
		return -1;
	if (fseek(f, 0, SEEK_END) != 0) {
		fclose(f);
		return -1;
	}
	len = ftell(f);
	if (len < 0 || fseek(f, 0, SEEK_SET) != 0) {
		fclose(f);
		return -1;
	}
	if (!dxx_extract_memory_allowed((uint64_t) len, 0)) {
		fclose(f);
		return -1;
	}
	data = (unsigned char *) malloc((size_t) len);
	if (!data && len != 0) {
		fclose(f);
		return -1;
	}
	if ((size_t) len != 0 && fread(data, 1, (size_t) len, f) != (size_t) len) {
		free(data);
		fclose(f);
		return -1;
	}
	if (fclose(f) != 0) {
		free(data);
		return -1;
	}

	*out_data = data;
	*out_size = (size_t) len;
	return 0;
}

static int extract_sti2_from_hfs(hfs_catalog_t *catalog,
                                 const char *output_dir, const char **extensions,
                                 extract_progress_fn progress, void *user_data)
{
	char archive_path[1024];
	unsigned char *archive_data = NULL;
	size_t archive_size = 0;
	const hfs_file_entry_t *entry = NULL;
	int i;
	int extracted;

	for (i = 0; i < hfs_catalog_file_count(catalog); i++) {
		entry = hfs_catalog_file_at(catalog, i);
		if (entry && !entry->is_dir &&
		    str_equals_ignore_case(entry->path, "Install Descent")) {
			if (!dxx_extract_memory_allowed(entry->data_size, 0))
				return -1;
			break;
		}
		entry = NULL;
	}
	if (!entry)
		return -1;
	snprintf(archive_path, sizeof(archive_path), "%s/.install_descent.sti2", output_dir);
	if (hfs_catalog_extract_entry(catalog, entry, archive_path) < 0)
		return -1;
	if (read_file_to_buffer(archive_path, &archive_data, &archive_size) < 0) {
		remove(archive_path);
		return -1;
	}
	remove(archive_path);
	if (!sti2_is_archive(archive_data, archive_size)) {
		free(archive_data);
		return -1;
	}

	extracted = sti2_extract_matching(archive_data, archive_size,
	                                  extensions, output_dir,
	                                  progress, user_data);
	free(archive_data);
	return extracted;
}

static int extract_hfs_matching_files(hfs_catalog_t *catalog,
                                      const char *output_dir, const char **extensions,
                                      extract_progress_fn progress, void *user_data)
{
	uint64_t total_bytes = 0;
	long long done_bytes = 0;
	int extracted = 0;
	int i;

	for (i = 0; i < hfs_catalog_file_count(catalog); i++) {
		const hfs_file_entry_t *entry = hfs_catalog_file_at(catalog, i);
		if (!entry || entry->is_dir ||
		    !ext_matches(path_basename(entry->path), extensions))
			continue;
		if (entry->data_size > DXX_EXTRACT_MAX_ENTRY_BYTES ||
		    dxx_extract_add_bytes(&total_bytes, entry->data_size,
		                          DXX_EXTRACT_MAX_TOTAL_BYTES) < 0)
			return -1;
	}
	if (!dxx_extract_has_free_space(output_dir, total_bytes))
		return -1;

	for (i = 0; i < hfs_catalog_file_count(catalog); i++) {
		char output_path[1024];
		const hfs_file_entry_t *entry = hfs_catalog_file_at(catalog, i);
		int written;
		const char *name;

		if (!entry || entry->is_dir ||
		    !(name = path_basename(entry->path)) ||
		    !ext_matches(name, extensions))
			continue;

		snprintf(output_path, sizeof(output_path), "%s/%s", output_dir, entry->path);
		written = hfs_catalog_extract_entry(catalog, entry, output_path);
		if (written < 0)
			return -1;
		done_bytes += written;
		extracted++;
		if (progress && progress(name, done_bytes, (long long) total_bytes, user_data) != 0)
			return -1;
	}

	return extracted;
}

int mac_extract_files_from_hfs_track(int bin_fd, int track_start_sector, int track_num_sectors,
                                     const char *output_dir,
                                     const char **sti2_extensions,
                                     const char **hfs_extensions,
                                     extract_progress_fn progress,
                                     void *user_data)
{
	hfs_catalog_t *catalog;
	int extracted;
	const char **fallback_extensions = hfs_extensions ? hfs_extensions : sti2_extensions;

	if (bin_fd < 0 || !output_dir)
		return -1;
	if (hfs_catalog_open(bin_fd, track_start_sector, track_num_sectors, &catalog) < 0)
		return -1;

	extracted = extract_sti2_from_hfs(catalog, output_dir, sti2_extensions,
	                                  progress, user_data);
	if (extracted <= 0)
		extracted = extract_hfs_matching_files(catalog, output_dir, fallback_extensions,
		                                       progress, user_data);
	hfs_catalog_close(catalog);

	return extracted;
}
