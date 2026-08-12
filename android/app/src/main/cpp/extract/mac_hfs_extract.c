#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

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

static int is_descent_installer_name(const char *path)
{
	return str_equals_ignore_case(path, "Install Descent") ||
	       str_equals_ignore_case(path, "Install Descent 2") ||
	       str_equals_ignore_case(path, "Install Descent II");
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

static int file_exists(const char *path)
{
	FILE *f = fopen(path, "rb");
	if (!f)
		return 0;
	fclose(f);
	return 1;
}

typedef struct {
	const unsigned char *data;
	size_t size;
#ifdef _WIN32
	HANDLE file;
	HANDLE mapping;
#else
	int fd;
#endif
} mapped_file_t;

static void unmap_file(mapped_file_t *mapped)
{
	if (!mapped)
		return;
#ifdef _WIN32
	if (mapped->data)
		UnmapViewOfFile(mapped->data);
	if (mapped->mapping)
		CloseHandle(mapped->mapping);
	if (mapped->file && mapped->file != INVALID_HANDLE_VALUE)
		CloseHandle(mapped->file);
#else
	if (mapped->data && mapped->size)
		munmap((void *) mapped->data, mapped->size);
	if (mapped->fd >= 0)
		close(mapped->fd);
#endif
	memset(mapped, 0, sizeof(*mapped));
#ifndef _WIN32
	mapped->fd = -1;
#endif
}

static int map_file_read_only(const char *path, mapped_file_t *mapped)
{
	if (!path || !mapped)
		return -1;
	memset(mapped, 0, sizeof(*mapped));
#ifdef _WIN32
	{
		LARGE_INTEGER size;
		mapped->file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
		                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (mapped->file == INVALID_HANDLE_VALUE)
			return -1;
		if (!GetFileSizeEx(mapped->file, &size) || size.QuadPart <= 0 ||
		    (uint64_t) size.QuadPart > DXX_EXTRACT_MAX_ENTRY_BYTES ||
		    (uint64_t) size.QuadPart > (uint64_t) SIZE_MAX) {
			unmap_file(mapped);
			return -1;
		}
		mapped->size = (size_t) size.QuadPart;
		mapped->mapping = CreateFileMappingA(mapped->file, NULL, PAGE_READONLY, 0, 0, NULL);
		if (!mapped->mapping) {
			unmap_file(mapped);
			return -1;
		}
		mapped->data = (const unsigned char *) MapViewOfFile(
		    mapped->mapping, FILE_MAP_READ, 0, 0, 0);
		if (!mapped->data) {
			unmap_file(mapped);
			return -1;
		}
	}
#else
	{
		struct stat st;
		mapped->fd = open(path, O_RDONLY);
		if (mapped->fd < 0)
			return -1;
		if (fstat(mapped->fd, &st) != 0 || st.st_size <= 0 ||
		    (uint64_t) st.st_size > DXX_EXTRACT_MAX_ENTRY_BYTES ||
		    (uint64_t) st.st_size > (uint64_t) SIZE_MAX) {
			unmap_file(mapped);
			return -1;
		}
		mapped->size = (size_t) st.st_size;
		mapped->data = (const unsigned char *) mmap(
		    NULL, mapped->size, PROT_READ, MAP_PRIVATE, mapped->fd, 0);
		if (mapped->data == MAP_FAILED) {
			mapped->data = NULL;
			unmap_file(mapped);
			return -1;
		}
	}
#endif
	return 0;
}

static int hfs_sti2_entry_allowed(uint64_t data_size)
{
	if (data_size == 0 || data_size > DXX_EXTRACT_MAX_ENTRY_BYTES ||
	    data_size > (uint64_t) SIZE_MAX)
		return -1;
	return 0;
}

static int extract_sti2_from_hfs(hfs_catalog_t *catalog,
                                 const char *output_dir, const char **extensions,
                                 extract_progress_fn progress, void *user_data)
{
	char archive_path[1024];
	mapped_file_t archive = { 0 };
	const hfs_file_entry_t *entry = NULL;
	int i;
	int extracted;

	for (i = 0; i < hfs_catalog_file_count(catalog); i++) {
		entry = hfs_catalog_file_at(catalog, i);
		if (entry && !entry->is_dir &&
		    is_descent_installer_name(entry->path)) {
			if (hfs_sti2_entry_allowed(entry->data_size) < 0) {
				fprintf(stderr, "mac_hfs_extract: Install Descent size %u exceeds archive mapping limit\n",
				        entry->data_size);
				return -1;
			}
			break;
		}
		entry = NULL;
	}
	if (!entry) {
		fprintf(stderr, "mac_hfs_extract: Descent installer was not found in HFS catalog\n");
		return -2;
	}
	snprintf(archive_path, sizeof(archive_path), "%s/.install_descent.sti2", output_dir);
	if (hfs_catalog_extract_entry(catalog, entry, archive_path) < 0) {
		fprintf(stderr, "mac_hfs_extract: failed to extract Install Descent from HFS\n");
		return -1;
	}
	if (map_file_read_only(archive_path, &archive) < 0) {
		fprintf(stderr, "mac_hfs_extract: failed to map Install Descent archive\n");
		remove(archive_path);
		return -1;
	}
	if (!sti2_is_archive(archive.data, archive.size)) {
		fprintf(stderr, "mac_hfs_extract: Install Descent is not a recognized STi2 archive\n");
		unmap_file(&archive);
		remove(archive_path);
		return -1;
	}

	extracted = sti2_extract_matching(archive.data, archive.size,
	                                  extensions, output_dir,
	                                  progress, user_data);
	unmap_file(&archive);
	remove(archive_path);
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

		snprintf(output_path, sizeof(output_path), "%s/%s", output_dir, name);
		if (file_exists(output_path))
			continue;
		written = hfs_catalog_extract_entry(catalog, entry, output_path);
		if (written < 0)
			return -1;
		done_bytes += written;
		extracted++;
		if (progress && progress(name, done_bytes, (long long) total_bytes, user_data) != 0)
			return DXX_EXTRACT_CANCELLED;
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
	int fallback_extracted;
	const char **fallback_extensions = hfs_extensions ? hfs_extensions : sti2_extensions;

	if (bin_fd < 0 || !output_dir)
		return -1;
	if (hfs_catalog_open(bin_fd, track_start_sector, track_num_sectors, &catalog) < 0)
		return -1;

	extracted = extract_sti2_from_hfs(catalog, output_dir, sti2_extensions,
	                                  progress, user_data);
	if (extracted == DXX_EXTRACT_CANCELLED) {
		hfs_catalog_close(catalog);
		return extracted;
	}
	if (extracted == -2) {
		extracted = extract_hfs_matching_files(catalog, output_dir, fallback_extensions,
		                                       progress, user_data);
	} else if (extracted > 0) {
		fallback_extracted =
		    extract_hfs_matching_files(catalog, output_dir, fallback_extensions,
		                               progress, user_data);
		if (fallback_extracted == DXX_EXTRACT_CANCELLED) {
			hfs_catalog_close(catalog);
			return fallback_extracted;
		} else if (fallback_extracted < 0)
			extracted = -1;
		else
			extracted += fallback_extracted;
	}
	hfs_catalog_close(catalog);

	return extracted;
}
