/*
 * test_sti2.c - Tests for StuffIt Installer v2 archive listing and extraction.
 *
 * Uses the real primary MacPlay D1 disc when sample media is present.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#define fd_from_file(fp) _fileno(fp)
#define open_bin(path)   _open(path, _O_RDONLY | _O_BINARY)
#define close_fd(fd)     _close(fd)
#define stat_file        _stat64
#define stat_t           struct __stat64
#else
#include <fcntl.h>
#include <unistd.h>
#define fd_from_file(fp) fileno(fp)
#define open_bin(path)   open(path, O_RDONLY)
#define close_fd(fd)     close(fd)
#define stat_file        stat
#define stat_t           struct stat
#endif

#include "cue_parser.h"
#include "hfs_reader.h"
#include "mac_hfs_extract.h"
#include "sti2_extract.h"

#define PRIMARY_CUE_PATH   "../../../../../../game_data/CD images/Descent - Mac macplay/Descent - Mac macplay.cue"
#define PRIMARY_OUTPUT_DIR "../../../../../../game_data/CD images/Descent - Mac macplay/data_tracks"

#ifdef _WIN32
#define remove_dir _rmdir
#else
#define remove_dir rmdir
#endif

static int tests_run;
static int tests_passed;
static int tests_skipped;

#define TEST(name)                \
	do {                          \
		tests_run++;              \
		printf("  %-50s ", name); \
		fflush(stdout);           \
	} while (0)

#define PASS()            \
	do {                  \
		tests_passed++;   \
		printf("PASS\n"); \
	} while (0)

#define SKIP(msg)                  \
	do {                           \
		tests_skipped++;           \
		printf("SKIP: %s\n", msg); \
	} while (0)

#define FAIL(msg)                  \
	do {                           \
		printf("FAIL: %s\n", msg); \
	} while (0)

static char *read_text_file(const char *path)
{
	FILE *f;
	long len;
	char *buf;

	f = fopen(path, "rb");
	if (!f)
		return NULL;
	fseek(f, 0, SEEK_END);
	len = ftell(f);
	fseek(f, 0, SEEK_SET);
	buf = (char *) malloc((size_t) len + 1);
	if (!buf) {
		fclose(f);
		return NULL;
	}
	fread(buf, 1, (size_t) len, f);
	buf[len] = '\0';
	fclose(f);
	return buf;
}

static int file_exists(const char *path)
{
	stat_t st;
	return stat_file(path, &st) == 0;
}

static long long file_size(const char *path)
{
	stat_t st;

	if (stat_file(path, &st) != 0)
		return -1;
	return (long long) st.st_size;
}

static void path_dir(const char *path, char *dir, int dir_len)
{
	const char *last_sep = NULL;
	const char *p;

	for (p = path; *p; p++) {
		if (*p == '/' || *p == '\\')
			last_sep = p;
	}
	if (!last_sep) {
		dir[0] = '.';
		dir[1] = '\0';
		return;
	}
	if (last_sep - path >= dir_len)
		last_sep = path + dir_len - 1;
	memcpy(dir, path, (size_t) (last_sep - path));
	dir[last_sep - path] = '\0';
}

static void path_join(char *out, int out_len, const char *a, const char *b)
{
	snprintf(out, out_len, "%s/%s", a, b);
}

typedef struct {
	int fd;
	int track_start_sector;
	int track_num_sectors;
} cue_data_track_t;

static int open_cue_data_track(const char *cue_path, cue_data_track_t *out)
{
	char cue_dir[1024];
	char bin_path[1024];
	char *cue_text;
	long long bin_sizes[CUE_MAX_FILES];
	cue_disc_t disc;
	int data_track_index = -1;
	int i;

	if (!out)
		return -1;
	memset(out, 0, sizeof(*out));
	out->fd = -1;

	if (!file_exists(cue_path))
		return -2;

	cue_text = read_text_file(cue_path);
	if (!cue_text)
		return -1;

	memset(&disc, 0, sizeof(disc));
	if (cue_parse(cue_text, NULL, 0, &disc) <= 0) {
		free(cue_text);
		return -1;
	}

	path_dir(cue_path, cue_dir, sizeof(cue_dir));
	for (i = 0; i < disc.num_files; i++) {
		path_join(bin_path, sizeof(bin_path), cue_dir, disc.files[i].filename);
		bin_sizes[i] = file_size(bin_path);
		if (bin_sizes[i] < 0) {
			free(cue_text);
			return -1;
		}
	}

	memset(&disc, 0, sizeof(disc));
	if (cue_parse(cue_text, bin_sizes, CUE_MAX_FILES, &disc) <= 0) {
		free(cue_text);
		return -1;
	}
	free(cue_text);

	for (i = 0; i < disc.num_tracks; i++) {
		if (disc.tracks[i].type == CUE_TRACK_DATA) {
			data_track_index = i;
			break;
		}
	}
	if (data_track_index < 0)
		return -1;

	path_join(bin_path, sizeof(bin_path), cue_dir, disc.files[disc.tracks[data_track_index].file_index].filename);
	out->fd = open_bin(bin_path);
	if (out->fd < 0)
		return -1;

	out->track_start_sector = disc.tracks[data_track_index].start_sector;
	out->track_num_sectors = disc.tracks[data_track_index].num_sectors;
	return 0;
}

static void close_cue_data_track(cue_data_track_t *track)
{
	if (track && track->fd >= 0) {
		close_fd(track->fd);
		track->fd = -1;
	}
}

static int extract_hfs_file_from_cue(const char *cue_path, const char *hfs_path, const char *output_path)
{
	cue_data_track_t track;
	int rc;

	rc = open_cue_data_track(cue_path, &track);
	if (rc < 0)
		return rc;

	rc = hfs_extract_file(track.fd, track.track_start_sector, track.track_num_sectors,
	                      hfs_path, output_path);
	close_cue_data_track(&track);
	return rc;
}

static int read_binary_file(const char *path, unsigned char **out_buf, size_t *out_size)
{
	FILE *f;
	long len;
	unsigned char *buf;

	if (!out_buf || !out_size)
		return -1;
	*out_buf = NULL;
	*out_size = 0;

	f = fopen(path, "rb");
	if (!f)
		return -1;
	fseek(f, 0, SEEK_END);
	len = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (len < 0) {
		fclose(f);
		return -1;
	}
	buf = (unsigned char *) malloc((size_t) len);
	if (!buf) {
		fclose(f);
		return -1;
	}
	if (fread(buf, 1, (size_t) len, f) != (size_t) len) {
		free(buf);
		fclose(f);
		return -1;
	}
	fclose(f);
	*out_buf = buf;
	*out_size = (size_t) len;
	return 0;
}

static int files_match_exact(const char *actual_path, const char *expected_path)
{
	FILE *actual;
	FILE *expected;
	unsigned char actual_buf[4096];
	unsigned char expected_buf[4096];

	actual = fopen(actual_path, "rb");
	expected = fopen(expected_path, "rb");
	if (!actual || !expected) {
		if (actual)
			fclose(actual);
		if (expected)
			fclose(expected);
		return 0;
	}

	for (;;) {
		size_t actual_read = fread(actual_buf, 1, sizeof(actual_buf), actual);
		size_t expected_read = fread(expected_buf, 1, sizeof(expected_buf), expected);

		if (actual_read != expected_read || memcmp(actual_buf, expected_buf, actual_read) != 0) {
			fclose(actual);
			fclose(expected);
			return 0;
		}
		if (actual_read == 0)
			break;
	}

	fclose(actual);
	fclose(expected);
	return 1;
}

static void cleanup_test_output_dir(const char *dir, const char *const *files, int count)
{
	int i;

	for (i = 0; i < count; i++) {
		char path[1024];

		path_join(path, sizeof(path), dir, files[i]);
		remove(path);
	}
	remove_dir(dir);
}

static int run_header_reject_test(void)
{
	unsigned char bogus[STI2_PATH_LEN];
	sti2_entry_list_t list;

	TEST("sti2_rejects_non_archive_header");
	memset(bogus, 0, sizeof(bogus));
	memcpy(bogus, "NOT!", 4);
	if (sti2_is_archive(bogus, sizeof(bogus)) ||
	    sti2_list_entries(bogus, sizeof(bogus), &list) >= 0) {
		FAIL("bogus archive accepted");
		return 0;
	}
	PASS();
	return 1;
}

static int run_real_archive_listing_test(void)
{
	static const struct {
		const char *name;
		unsigned int expected_header_offset;
	} expected_offsets[] = {
		{ "CHAOS.HOG", 0x69398u },
		{ "CHAOS.MSN", 0x7e595u },
		{ "descent.hog", 0x7e767u },
		{ "descent.pig", 0x456260u },
		{ "demo1.dem", 0x5c0fc9u },
		{ "watchme.dem", 0x62ec5au },
		{ "yep9.dem", 0x6837ebu },
	};
	static const char *expected_names[] = {
		"CHAOS.HOG",
		"CHAOS.MSN",
		"Descent",
		"demo1.dem",
		"descent.hog",
		"descent.pig",
		"watchme.dem",
		"yep9.dem"
	};
	const char *archive_path = "test_sti2_install_descent.bin";
	unsigned char *archive_data = NULL;
	sti2_entry_list_t list;
	size_t archive_size = 0;
	int regular_count = 0;
	int i;

	TEST("real_primary_macplay_sti2_listing");
	if (!file_exists(PRIMARY_CUE_PATH)) {
		SKIP("sample media not present");
		return 1;
	}
	if (extract_hfs_file_from_cue(PRIMARY_CUE_PATH, "Install Descent", archive_path) < 0) {
		FAIL("could not extract Install Descent from HFS");
		return 0;
	}
	if (read_binary_file(archive_path, &archive_data, &archive_size) < 0) {
		remove(archive_path);
		FAIL("could not read extracted archive");
		return 0;
	}
	remove(archive_path);

	if (!sti2_is_archive(archive_data, archive_size)) {
		free(archive_data);
		FAIL("archive magic not recognized");
		return 0;
	}
	if (sti2_list_entries(archive_data, archive_size, &list) < 0) {
		free(archive_data);
		FAIL("entry listing failed");
		return 0;
	}
	free(archive_data);

	for (i = 0; i < list.num_entries; i++) {
		if (!list.entries[i].is_directory)
			regular_count++;
	}

	if (list.declared_file_count != 13u || regular_count != 8) {
		FAIL("unexpected archive entry counts");
		return 0;
	}

	for (i = 0; i < (int) (sizeof(expected_names) / sizeof(expected_names[0])); i++) {
		char oracle_path[1024];
		long long oracle_size;
		int index = sti2_find_entry_index(&list, expected_names[i]);

		if (index < 0) {
			FAIL("expected entry missing");
			return 0;
		}
		if (list.entries[index].is_directory || list.entries[index].data_method != 13u) {
			FAIL("unexpected entry metadata");
			return 0;
		}
		path_join(oracle_path, sizeof(oracle_path), PRIMARY_OUTPUT_DIR, expected_names[i]);
		oracle_size = file_size(oracle_path);
		if (oracle_size < 0 || (unsigned int) oracle_size != list.entries[index].uncompressed_size) {
			FAIL("unexpected entry size");
			return 0;
		}
	}

	for (i = 0; i < (int) (sizeof(expected_offsets) / sizeof(expected_offsets[0])); i++) {
		int index = sti2_find_entry_index(&list, expected_offsets[i].name);

		if (index < 0 || list.entries[index].header_offset != expected_offsets[i].expected_header_offset) {
			FAIL("unexpected entry header offset");
			return 0;
		}
	}

	PASS();
	return 1;
}

static int run_real_archive_extract_test(void)
{
	static const char *expected_names[] = {
		"CHAOS.MSN",
		"CHAOS.HOG",
		"Descent",
		"demo1.dem",
		"descent.hog",
		"descent.pig",
		"watchme.dem",
		"yep9.dem"
	};
	const char *archive_path = "test_sti2_install_descent.bin";
	unsigned char *archive_data = NULL;
	sti2_entry_list_t list;
	size_t archive_size = 0;
	int i;

	TEST("real_primary_macplay_sti2_extracts_match_oracles");
	if (!file_exists(PRIMARY_CUE_PATH)) {
		SKIP("sample media not present");
		return 1;
	}
	if (extract_hfs_file_from_cue(PRIMARY_CUE_PATH, "Install Descent", archive_path) < 0) {
		FAIL("could not extract Install Descent from HFS");
		return 0;
	}
	if (read_binary_file(archive_path, &archive_data, &archive_size) < 0) {
		remove(archive_path);
		FAIL("could not read extracted archive");
		return 0;
	}
	remove(archive_path);

	if (sti2_list_entries(archive_data, archive_size, &list) < 0) {
		free(archive_data);
		FAIL("entry listing failed");
		return 0;
	}

	for (i = 0; i < (int) (sizeof(expected_names) / sizeof(expected_names[0])); i++) {
		char actual_path[1024];
		char expected_path[1024];
		long long expected_size;
		int index = sti2_find_entry_index(&list, expected_names[i]);
		int extracted_size;

		if (index < 0) {
			free(archive_data);
			FAIL("expected entry missing");
			return 0;
		}
		snprintf(actual_path, sizeof(actual_path), "test_sti2_%s", expected_names[i]);
		path_join(expected_path, sizeof(expected_path), PRIMARY_OUTPUT_DIR, expected_names[i]);
		expected_size = file_size(expected_path);
		if (expected_size < 0) {
			free(archive_data);
			FAIL("oracle file missing");
			return 0;
		}

		extracted_size = sti2_extract_entry(archive_data, archive_size,
		                                    &list.entries[index], actual_path);
		if (extracted_size != (int) expected_size ||
		    !files_match_exact(actual_path, expected_path)) {
			remove(actual_path);
			free(archive_data);
			FAIL("extracted file mismatch");
			return 0;
		}
		remove(actual_path);
	}

	free(archive_data);
	PASS();
	return 1;
}

static int run_real_native_mac_extract_test(void)
{
	static const char *expected_names[] = {
		"CHAOS.MSN",
		"CHAOS.HOG",
		"Descent",
		"demo1.dem",
		"descent.hog",
		"descent.pig",
		"watchme.dem",
		"yep9.dem"
	};
	const char *output_dir = "test_sti2_native_extract";
	cue_data_track_t track;
	int extracted;
	int i;

	TEST("real_primary_macplay_native_extract_matches_oracles");
	if (!file_exists(PRIMARY_CUE_PATH)) {
		SKIP("sample media not present");
		return 1;
	}

	cleanup_test_output_dir(output_dir, expected_names,
	                        (int) (sizeof(expected_names) / sizeof(expected_names[0])));
	if (open_cue_data_track(PRIMARY_CUE_PATH, &track) < 0) {
		FAIL("could not open MacPlay data track");
		return 0;
	}

	extracted = mac_extract_files_from_hfs_track(track.fd,
	                                             track.track_start_sector,
	                                             track.track_num_sectors,
	                                             output_dir,
	                                             NULL,
	                                             NULL,
	                                             NULL,
	                                             NULL);
	close_cue_data_track(&track);
	if (extracted != (int) (sizeof(expected_names) / sizeof(expected_names[0]))) {
		cleanup_test_output_dir(output_dir, expected_names,
		                        (int) (sizeof(expected_names) / sizeof(expected_names[0])));
		FAIL("unexpected extracted count");
		return 0;
	}

	for (i = 0; i < (int) (sizeof(expected_names) / sizeof(expected_names[0])); i++) {
		char actual_path[1024];
		char expected_path[1024];

		path_join(actual_path, sizeof(actual_path), output_dir, expected_names[i]);
		path_join(expected_path, sizeof(expected_path), PRIMARY_OUTPUT_DIR, expected_names[i]);
		if (!file_exists(actual_path) || !files_match_exact(actual_path, expected_path)) {
			cleanup_test_output_dir(output_dir, expected_names,
			                        (int) (sizeof(expected_names) / sizeof(expected_names[0])));
			FAIL("native extraction output mismatch");
			return 0;
		}
	}

	cleanup_test_output_dir(output_dir, expected_names,
	                        (int) (sizeof(expected_names) / sizeof(expected_names[0])));
	PASS();
	return 1;
}

int main(void)
{
	int ok = 1;

	printf("STi2 tests\n");

	if (!run_header_reject_test())
		ok = 0;
	if (!run_real_archive_listing_test())
		ok = 0;
	if (!run_real_archive_extract_test())
		ok = 0;
	if (!run_real_native_mac_extract_test())
		ok = 0;

	printf("\nSummary: %d passed, %d skipped, %d total\n",
	       tests_passed, tests_skipped, tests_run);

	return ok ? 0 : 1;
}