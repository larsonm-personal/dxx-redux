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

static void put_be16(unsigned char *p, unsigned int value)
{
	p[0] = (unsigned char) (value >> 8);
	p[1] = (unsigned char) value;
}

static void put_be32(unsigned char *p, unsigned int value)
{
	p[0] = (unsigned char) (value >> 24);
	p[1] = (unsigned char) (value >> 16);
	p[2] = (unsigned char) (value >> 8);
	p[3] = (unsigned char) value;
}

static unsigned int fixture_crc16(const unsigned char *data, size_t len)
{
	unsigned int crc = 0;
	size_t i;

	for (i = 0; i < len; i++) {
		unsigned int bit;

		crc ^= data[i];
		for (bit = 0; bit < 8; bit++)
			crc = (crc & 1u) ? (crc >> 1) ^ 0xa001u : crc >> 1;
	}
	return crc & 0xffffu;
}

static size_t build_encrypted_fixture(unsigned char *archive, size_t capacity,
                                      unsigned int method, unsigned int compressed_size,
                                      unsigned int uncompressed_size)
{
	static const char name[] = "encrypted.hog";
	const size_t header_offset = 22u;
	const size_t payload_offset = header_offset + 112u;
	size_t total_size = payload_offset + compressed_size;
	unsigned char *header;

	if (capacity < total_size)
		return 0;
	memset(archive, 0, total_size);
	memcpy(archive, "STin", 4);
	put_be16(archive + 4, 1);
	put_be32(archive + 6, (unsigned int) total_size);
	put_be32(archive + 10, 0x724c6175u);

	header = archive + header_offset;
	header[1] = (unsigned char) method;
	header[2] = (unsigned char) strlen(name);
	memcpy(header + 3, name, strlen(name));
	put_be32(header + 88, uncompressed_size);
	put_be32(header + 96, compressed_size);
	put_be16(header + 110, fixture_crc16(header, 110));
	memset(archive + payload_offset, 0x5a, compressed_size);
	return total_size;
}

static void build_archive_header(unsigned char *archive, unsigned int file_count,
                                 unsigned int total_size)
{
	memset(archive, 0, 22u);
	memcpy(archive, "STin", 4);
	put_be16(archive + 4, file_count);
	put_be32(archive + 6, total_size);
	put_be32(archive + 10, 0x724c6175u);
}

static void build_file_header(unsigned char *header, const char *name,
                              unsigned int resource_method, unsigned int data_method,
                              unsigned int parent_offset, unsigned int child_count,
                              unsigned int resource_compressed_size,
                              unsigned int data_compressed_size)
{
	size_t name_len = strlen(name);

	memset(header, 0, 112u);
	header[0] = (unsigned char) resource_method;
	header[1] = (unsigned char) data_method;
	header[2] = (unsigned char) name_len;
	memcpy(header + 3, name, name_len);
	put_be16(header + 48, child_count);
	put_be32(header + 58, parent_offset);
	put_be32(header + 84, resource_compressed_size);
	put_be32(header + 88, data_compressed_size);
	put_be32(header + 92, resource_compressed_size);
	put_be32(header + 96, data_compressed_size);
	put_be16(header + 110, fixture_crc16(header, 110));
}

static int run_structural_validation_test(void)
{
	static const char *extensions[] = { "hog", NULL };
	const char *malformed_output = "test_sti2_malformed_structure";
	unsigned char archive[512];
	sti2_entry_list_t list;
	unsigned char *root = archive + 22u;
	unsigned char *second = root + 112u;
	unsigned char *third = second + 112u;

	TEST("sti2_enforces_archive_order_and_declared_structure");

	memset(archive, 0, sizeof(archive));
	build_archive_header(archive, 1, 246u);
	build_file_header(root, "real.hog", 0, 0, 0, 0, 0, 112u);
	build_file_header(second, "fake.hog", 0, 0, 0, 0, 0, 0);
	if (sti2_list_entries(archive, 246u, &list) != 1 ||
	    strcmp(list.entries[0].path, "real.hog") != 0) {
		FAIL("header-shaped payload was parsed as an entry");
		return 0;
	}
	root[1] = 13u;
	put_be16(root + 110, fixture_crc16(root, 110));
	if (sti2_list_entries(archive, 246u, &list) != 1 ||
	    strcmp(list.entries[0].path, "real.hog") != 0) {
		FAIL("header-shaped compressed payload was parsed as an entry");
		return 0;
	}

	memset(archive, 0, sizeof(archive));
	build_archive_header(archive, 2, 246u);
	build_file_header(root, "first.hog", 0, 0, 0, 0, 0, 0);
	build_file_header(second, "second.hog", 0, 0, 0, 0, 0, 0);
	second[3] ^= 1u;
	remove_dir(malformed_output);
	if (sti2_list_entries(archive, 246u, &list) >= 0) {
		FAIL("corrupted middle header was skipped");
		return 0;
	}
	if (sti2_extract_matching(archive, 246u, extensions, malformed_output,
	                          NULL, NULL) >= 0 ||
	    file_exists(malformed_output)) {
		FAIL("malformed structure committed extraction output");
		return 0;
	}

	memset(archive, 0, sizeof(archive));
	build_archive_header(archive, 1, 137u);
	build_file_header(root, "truncated.hog", 0, 0, 0, 0, 0, 4u);
	if (sti2_list_entries(archive, 137u, &list) >= 0) {
		FAIL("truncated fork span was accepted");
		return 0;
	}

	memset(archive, 0, sizeof(archive));
	build_archive_header(archive, 1, 134u);
	build_file_header(root, "overflow.hog", 0, 0, 0, 0, 0xffffffffu, 1u);
	if (sti2_list_entries(archive, 134u, &list) >= 0) {
		FAIL("overflowing fork span was accepted");
		return 0;
	}

	memset(archive, 0, sizeof(archive));
	build_archive_header(archive, 1, 358u);
	build_file_header(root, "folder", 0x20u, 0x20u, 0, 1, 0, 224u);
	build_file_header(second, "child.hog", 0, 0, 22u, 0, 0, 0);
	build_file_header(third, "folder", 0x21u, 0x21u, 22u, 0, 0, 224u);
	if (sti2_list_entries(archive, 358u, &list) != 2 ||
	    strcmp(list.entries[0].path, "folder") != 0 ||
	    strcmp(list.entries[1].path, "folder/child.hog") != 0) {
		FAIL("valid nested folder structure was rejected");
		return 0;
	}

	put_be32(second + 58, 0);
	put_be16(second + 110, fixture_crc16(second, 110));
	if (sti2_list_entries(archive, 358u, &list) >= 0) {
		FAIL("invalid parent link was accepted");
		return 0;
	}
	build_file_header(second, "child.hog", 0, 0, 22u, 0, 0, 0);
	third[0] = 0;
	third[1] = 0;
	put_be16(third + 110, fixture_crc16(third, 110));
	if (sti2_list_entries(archive, 358u, &list) >= 0) {
		FAIL("unbalanced folder was accepted");
		return 0;
	}

	memset(archive, 0, sizeof(archive));
	build_archive_header(archive, 1, 246u);
	build_file_header(root, "first.hog", 0, 0, 0, 0, 0, 0);
	build_file_header(second, "second.hog", 0, 0, 0, 0, 0, 0);
	if (sti2_list_entries(archive, 246u, &list) >= 0) {
		FAIL("too-small declared root count was accepted");
		return 0;
	}
	put_be16(archive + 4, 2);
	put_be32(archive + 6, 134u);
	if (sti2_list_entries(archive, 134u, &list) >= 0) {
		FAIL("too-large declared root count was accepted");
		return 0;
	}

	PASS();
	return 1;
}

static int run_encrypted_entry_reject_test(void)
{
	static const struct {
		unsigned int method;
		unsigned int compressed_size;
		unsigned int uncompressed_size;
	} cases[] = {
		{ 0x80u, 8u, 8u },
		{ 0x8du, 8u, 32u },
		{ 0x8du, 20u, 32u },
	};
	static const char *extensions[] = { "hog", NULL };
	const char *output_path = "test_sti2_encrypted_output.hog";
	const char *output_dir = "test_sti2_encrypted_matching";
	unsigned char archive[22u + 112u + 32u];
	unsigned int i;

	TEST("sti2_rejects_encrypted_entries_before_output");
	remove(output_path);
	remove_dir(output_dir);
	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		sti2_entry_list_t list;
		size_t archive_size = build_encrypted_fixture(archive, sizeof(archive),
		                                              cases[i].method,
		                                              cases[i].compressed_size,
		                                              cases[i].uncompressed_size);

		if (archive_size == 0 || sti2_list_entries(archive, archive_size, &list) != 1 ||
		    !list.entries[0].data_encrypted || list.entries[0].resource_encrypted ||
		    list.entries[0].data_method != (cases[i].method & 0x0fu)) {
			FAIL("encrypted metadata was not preserved");
			return 0;
		}
		if (sti2_extract_entry(archive, archive_size, &list.entries[0], output_path) !=
		        STI2_EXTRACT_UNSUPPORTED_ENCRYPTION ||
		    file_exists(output_path)) {
			remove(output_path);
			FAIL("encrypted direct extraction was not rejected cleanly");
			return 0;
		}
		if (sti2_extract_matching(archive, archive_size, extensions, output_dir,
		                          NULL, NULL) != STI2_EXTRACT_UNSUPPORTED_ENCRYPTION ||
		    file_exists(output_dir)) {
			remove_dir(output_dir);
			FAIL("encrypted matching extraction mutated output");
			return 0;
		}
	}
	PASS();
	return 1;
}

static int run_payload_checksum_test(void)
{
	static const unsigned char expected[] = { 'D', 'E', 'S', 'C', 'E', 'N', 'T' };
	const char *output_path = "test_sti2_payload_crc.hog";
	unsigned char archive[22u + 112u + sizeof(expected)];
	unsigned char *header = archive + 22u;
	unsigned char *payload = header + 112u;
	unsigned char *actual = NULL;
	sti2_entry_list_t list;
	size_t actual_size = 0;

	TEST("sti2_verifies_payload_crc_before_output_commit");
	memset(archive, 0, sizeof(archive));
	build_archive_header(archive, 1, (unsigned int) sizeof(archive));
	build_file_header(header, "payload.hog", 0, 0, 0, 0, 0, sizeof(expected));
	memcpy(payload, expected, sizeof(expected));
	put_be16(header + 102, fixture_crc16(payload, sizeof(expected)));
	put_be16(header + 110, fixture_crc16(header, 110));
	remove(output_path);

	if (sti2_list_entries(archive, sizeof(archive), &list) != 1 ||
	    !list.entries[0].data_crc_present ||
	    list.entries[0].data_crc != fixture_crc16(payload, sizeof(expected)) ||
	    sti2_extract_entry(archive, sizeof(archive), &list.entries[0], output_path) !=
	        (int) sizeof(expected) ||
	    read_binary_file(output_path, &actual, &actual_size) < 0 ||
	    actual_size != sizeof(expected) || memcmp(actual, expected, sizeof(expected)) != 0) {
		free(actual);
		remove(output_path);
		FAIL("valid stored payload checksum was rejected");
		return 0;
	}
	free(actual);
	actual = NULL;
	remove(output_path);

	payload[0] ^= 1u;
	if (sti2_extract_entry(archive, sizeof(archive), &list.entries[0], output_path) >= 0 ||
	    file_exists(output_path)) {
		remove(output_path);
		FAIL("corrupted stored payload was committed");
		return 0;
	}
	payload[0] ^= 1u;
	put_be16(header + 102, fixture_crc16(payload, sizeof(expected)) ^ 1u);
	put_be16(header + 110, fixture_crc16(header, 110));
	if (sti2_list_entries(archive, sizeof(archive), &list) != 1 ||
	    sti2_extract_entry(archive, sizeof(archive), &list.entries[0], output_path) >= 0 ||
	    file_exists(output_path)) {
		remove(output_path);
		FAIL("mismatched stored payload checksum was committed");
		return 0;
	}

	PASS();
	return 1;
}

static int run_bit_reader_exhaustion_test(void)
{
	TEST("sti2_bit_readers_record_input_exhaustion");
	if (sti2_test_bit_reader_exhaustion() < 0) {
		FAIL("bit reader synthesized unreported input");
		return 0;
	}
	PASS();
	return 1;
}

static int run_method14_code_length_test(void)
{
	unsigned char lengths[308];

	TEST("sti2_method14_rejects_invalid_code_lengths");
	memset(lengths, 0, sizeof(lengths));
	if (sti2_test_method14_code_lengths(lengths, 308) >= 0) {
		FAIL("empty method 14 tree accepted");
		return 0;
	}
	lengths[307] = 31;
	if (sti2_test_method14_code_lengths(lengths, 308) < 0) {
		FAIL("31-bit method 14 code rejected");
		return 0;
	}
	lengths[307] = 32;
	if (sti2_test_method14_code_lengths(lengths, 308) < 0) {
		FAIL("32-bit method 14 code rejected");
		return 0;
	}
	lengths[307] = 33;
	if (sti2_test_method14_code_lengths(lengths, 308) >= 0) {
		FAIL("33-bit method 14 code accepted");
		return 0;
	}
	lengths[307] = 37;
	if (sti2_test_method14_code_lengths(lengths, 308) >= 0) {
		FAIL("37-bit method 14 code accepted");
		return 0;
	}
	memset(lengths, 0, sizeof(lengths));
	lengths[0] = 1;
	lengths[1] = 1;
	if (sti2_test_method14_code_lengths(lengths, 2) < 0) {
		FAIL("complete method 14 tree rejected");
		return 0;
	}
	lengths[2] = 1;
	if (sti2_test_method14_code_lengths(lengths, 3) >= 0) {
		FAIL("oversubscribed method 14 tree accepted");
		return 0;
	}
	PASS();
	return 1;
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

	if (!run_method14_code_length_test())
		ok = 0;
	if (!run_encrypted_entry_reject_test())
		ok = 0;
	if (!run_payload_checksum_test())
		ok = 0;
	if (!run_bit_reader_exhaustion_test())
		ok = 0;
	if (!run_structural_validation_test())
		ok = 0;
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
