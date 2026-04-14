/*
 * test_cue_iso.c — Tests for CUE parser and ISO 9660 reader.
 *
 * Generates minimal BIN+CUE test fixtures in-memory and on disk,
 * then exercises the parsers against both valid and malformed inputs.
 *
 * Build (via CMake from repo root):
 *   cmake -S android/app/src/main/cpp/extract -B android/tests/build
 *   cmake --build android/tests/build --config Release --target test_cue_iso
 *
 * Run:
 *   ./test_cue_iso
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#include <direct.h>
#define mkdir_p(d)     _mkdir(d)
#define open_bin(path) _open(path, _O_RDONLY | _O_BINARY)
#define close_fd(fd)   _close(fd)
#else
#include <fcntl.h>
#include <unistd.h>
#define mkdir_p(d)     mkdir(d, 0755)
#define open_bin(path) open(path, O_RDONLY)
#define close_fd(fd)   close(fd)
#endif

#include "cue_parser.h"
#include "iso9660_reader.h"

/* ── Helpers ─────────────────────────────────────────────────────────── */

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name)                 \
	do {                           \
		tests_run++;               \
		printf("  %-50s ", #name); \
		fflush(stdout);            \
	} while (0)

#define PASS()            \
	do {                  \
		tests_passed++;   \
		printf("PASS\n"); \
	} while (0)
#define FAIL(msg)                  \
	do {                           \
		printf("FAIL: %s\n", msg); \
	} while (0)

/* ── ISO 9660 sector builder ─────────────────────────────────────────── */

#define SECTOR_SIZE    2352
#define USER_DATA_SIZE 2048

/* Build a raw Mode 1 sector with sync, header, user data (no real ECC). */
static void build_mode1_sector(unsigned char *out,
                               int minute, int second, int frame,
                               const unsigned char *user_data)
{
	/* 12-byte sync pattern */
	out[0] = 0x00;
	memset(out + 1, 0xFF, 10);
	out[11] = 0x00;

	/* 4-byte header: minute, second, frame (BCD), mode */
	out[12] = (unsigned char) (((minute / 10) << 4) | (minute % 10));
	out[13] = (unsigned char) (((second / 10) << 4) | (second % 10));
	out[14] = (unsigned char) (((frame / 10) << 4) | (frame % 10));
	out[15] = 1; /* Mode 1 */

	/* 2048 bytes user data */
	if (user_data)
		memcpy(out + 16, user_data, USER_DATA_SIZE);
	else
		memset(out + 16, 0, USER_DATA_SIZE);

	/* 288 bytes ECC/EDC — zeroed (not validated by our reader) */
	memset(out + 16 + USER_DATA_SIZE, 0, 288);
}

/* Convert LBA to MSF (minute/second/frame). 75 frames/sec, 60 sec/min.
 * First 150 frames = 2-second pregap. */
static void lba_to_msf(int lba, int *m, int *s, int *f)
{
	int abs_lba = lba + 150; /* add 2-second pregap */
	*f = abs_lba % 75;
	*s = (abs_lba / 75) % 60;
	*m = abs_lba / (75 * 60);
}

/* ── Minimal ISO 9660 image builder ──────────────────────────────────── */

/*
 * Build a minimal ISO 9660 data track with one file in the root directory.
 * Returns malloc'd buffer of (num_sectors * SECTOR_SIZE) bytes.
 * The image has:
 *   Sectors 0-15  : system area (empty)
 *   Sector 16     : Primary Volume Descriptor
 *   Sector 17     : Volume Descriptor Set Terminator
 *   Sector 18     : Root directory (2 entries: . and ..)  + one file entry
 *   Sector 19     : File data
 *
 * file_name: name for the file (e.g., "HELLO.TXT")
 * file_data/file_len: content for the file (must fit in one sector, <=2048)
 * out_sectors: receives total sector count
 */
static unsigned char *build_minimal_iso(const char *file_name,
                                        const unsigned char *file_data,
                                        int file_len,
                                        int *out_sectors)
{
	int total = 20; /* sectors 0-19 */
	unsigned char *img = (unsigned char *) calloc(total, SECTOR_SIZE);
	unsigned char user[USER_DATA_SIZE];
	int m, s, f;

	/* Sectors 0-15: system area (empty) */
	for (int i = 0; i < 16; i++) {
		lba_to_msf(i, &m, &s, &f);
		build_mode1_sector(img + i * SECTOR_SIZE, m, s, f, NULL);
	}

	/* Sector 16: Primary Volume Descriptor */
	{
		memset(user, 0, sizeof(user));
		user[0] = 1;                  /* type = primary */
		memcpy(user + 1, "CD001", 5); /* standard identifier */
		user[6] = 1;                  /* version */
		/* Volume identifier at offset 40 (32 bytes) */
		memcpy(user + 40, "TEST_VOLUME", 11);

		/* Root directory record at offset 156 (34 bytes) */
		unsigned char *root_rec = user + 156;
		root_rec[0] = 34; /* length of directory record */
		root_rec[1] = 0;  /* extended attribute length */
		/* Extent location of root directory (LBA 18, little-endian at +2) */
		root_rec[2] = 18;
		root_rec[3] = 0;
		root_rec[4] = 0;
		root_rec[5] = 0;
		/* + big-endian at +6 */
		root_rec[6] = 0;
		root_rec[7] = 0;
		root_rec[8] = 0;
		root_rec[9] = 18;
		/* Data length of root dir (one sector = 2048) at +10 LE, +14 BE */
		root_rec[10] = 0;
		root_rec[11] = 8;
		root_rec[12] = 0;
		root_rec[13] = 0;
		root_rec[14] = 0;
		root_rec[15] = 0;
		root_rec[16] = 0x08;
		root_rec[17] = 0;
		/* Date at +18 (7 bytes, zeroed) */
		/* Flags at +25: 0x02 = directory */
		root_rec[25] = 0x02;
		/* File unit size, interleave gap at +26,+27: 0 */
		/* Volume sequence number at +28: 1 (LE+BE) */
		root_rec[28] = 1;
		root_rec[29] = 0;
		root_rec[30] = 0;
		root_rec[31] = 1;
		/* Name length at +32: 1, name at +33: 0x00 (root self-ref) */
		root_rec[32] = 1;
		root_rec[33] = 0x00;

		lba_to_msf(16, &m, &s, &f);
		build_mode1_sector(img + 16 * SECTOR_SIZE, m, s, f, user);
	}

	/* Sector 17: Volume Descriptor Set Terminator */
	{
		memset(user, 0, sizeof(user));
		user[0] = 255; /* type = terminator */
		memcpy(user + 1, "CD001", 5);
		user[6] = 1;

		lba_to_msf(17, &m, &s, &f);
		build_mode1_sector(img + 17 * SECTOR_SIZE, m, s, f, user);
	}

	/* Sector 18: Root directory with ., .., and one file */
	{
		int name_len = (int) strlen(file_name);
		int pos = 0;
		memset(user, 0, sizeof(user));

		/* "." entry (self) */
		user[pos + 0] = 34; /* record length */
		user[pos + 2] = 18; /* extent LBA (self) LE */
		user[pos + 10] = 0;
		user[pos + 11] = 8;    /* data size = 2048 LE */
		user[pos + 25] = 0x02; /* directory flag */
		user[pos + 32] = 1;    /* name len */
		user[pos + 33] = 0x00; /* "." */
		pos += 34;

		/* ".." entry (parent = self for root) */
		user[pos + 0] = 34;
		user[pos + 2] = 18; /* extent LBA (parent = root) LE */
		user[pos + 10] = 0;
		user[pos + 11] = 8;
		user[pos + 25] = 0x02;
		user[pos + 32] = 1;
		user[pos + 33] = 0x01; /* ".." */
		pos += 34;

		/* File entry */
		{
			int rec_len = 33 + name_len;
			if (rec_len % 2) rec_len++; /* pad to even */
			user[pos + 0] = (unsigned char) rec_len;
			/* Extent LBA = 19 (LE) */
			user[pos + 2] = 19;
			user[pos + 3] = 0;
			user[pos + 4] = 0;
			user[pos + 5] = 0;
			user[pos + 6] = 0;
			user[pos + 7] = 0;
			user[pos + 8] = 0;
			user[pos + 9] = 19;
			/* Data size (LE) */
			user[pos + 10] = (unsigned char) (file_len & 0xFF);
			user[pos + 11] = (unsigned char) ((file_len >> 8) & 0xFF);
			user[pos + 12] = 0;
			user[pos + 13] = 0;
			/* Data size (BE) */
			user[pos + 14] = 0;
			user[pos + 15] = 0;
			user[pos + 16] = (unsigned char) ((file_len >> 8) & 0xFF);
			user[pos + 17] = (unsigned char) (file_len & 0xFF);
			/* Flags at +25: 0 = regular file */
			user[pos + 25] = 0;
			/* Volume seq at +28 */
			user[pos + 28] = 1;
			user[pos + 29] = 0;
			user[pos + 30] = 0;
			user[pos + 31] = 1;
			/* Name length and name */
			user[pos + 32] = (unsigned char) name_len;
			memcpy(user + pos + 33, file_name, name_len);
		}

		lba_to_msf(18, &m, &s, &f);
		build_mode1_sector(img + 18 * SECTOR_SIZE, m, s, f, user);
	}

	/* Sector 19: File data */
	{
		memset(user, 0, sizeof(user));
		if (file_data && file_len > 0 && file_len <= USER_DATA_SIZE)
			memcpy(user, file_data, file_len);
		lba_to_msf(19, &m, &s, &f);
		build_mode1_sector(img + 19 * SECTOR_SIZE, m, s, f, user);
	}

	*out_sectors = total;
	return img;
}

static unsigned char *build_minimal_iso_image(const char *file_name,
                                              const unsigned char *file_data,
                                              int file_len,
                                              int *out_sectors)
{
	int sectors;
	unsigned char *raw = build_minimal_iso(file_name, file_data, file_len, &sectors);
	unsigned char *img = (unsigned char *) malloc((size_t) sectors * USER_DATA_SIZE);

	for (int i = 0; i < sectors; i++)
		memcpy(img + i * USER_DATA_SIZE,
		       raw + i * SECTOR_SIZE + 16,
		       USER_DATA_SIZE);

	free(raw);
	*out_sectors = sectors;
	return img;
}

/* Build sectors of raw audio (just filled with a pattern) */
static unsigned char *build_audio_sectors(int num_sectors)
{
	unsigned char *data = (unsigned char *) malloc(num_sectors * SECTOR_SIZE);
	for (int i = 0; i < num_sectors * SECTOR_SIZE; i++)
		data[i] = (unsigned char) (i & 0xFF);
	return data;
}

/* Write a BIN file with a data track and optional audio tracks.
 * data_img: raw data sectors (from build_minimal_iso)
 * data_sectors: number of data sectors
 * audio_sectors: number of audio sectors per audio track
 * num_audio_tracks: number of audio tracks
 *
 * Returns path to the written file. Caller must free. */
static const char *TEST_DIR = "test_fixtures";

static void write_test_bin(const char *filename,
                           const unsigned char *data_img, int data_sectors,
                           int audio_sectors_per_track, int num_audio_tracks)
{
	char path[512];
	snprintf(path, sizeof(path), "%s/%s", TEST_DIR, filename);

	FILE *f = fopen(path, "wb");
	if (!f) {
		fprintf(stderr, "Cannot create %s\n", path);
		return;
	}

	/* Write data track sectors */
	fwrite(data_img, SECTOR_SIZE, data_sectors, f);

	/* Write audio tracks */
	for (int t = 0; t < num_audio_tracks; t++) {
		unsigned char *audio = build_audio_sectors(audio_sectors_per_track);
		fwrite(audio, SECTOR_SIZE, audio_sectors_per_track, f);
		free(audio);
	}

	fclose(f);
}

static void write_test_cue(const char *filename, const char *content)
{
	char path[512];
	snprintf(path, sizeof(path), "%s/%s", TEST_DIR, filename);
	FILE *f = fopen(path, "w");
	if (!f) {
		fprintf(stderr, "Cannot create %s\n", path);
		return;
	}
	fputs(content, f);
	fclose(f);
}

static char *read_test_file(const char *filename)
{
	char path[512];
	snprintf(path, sizeof(path), "%s/%s", TEST_DIR, filename);
	FILE *f = fopen(path, "r");
	if (!f) return NULL;
	fseek(f, 0, SEEK_END);
	long len = ftell(f);
	fseek(f, 0, SEEK_SET);
	char *buf = (char *) malloc(len + 1);
	fread(buf, 1, len, f);
	buf[len] = '\0';
	fclose(f);
	return buf;
}

/* Read a CUE file from the test/data/ directory relative to cwd.
 * CTest sets working_directory to the extract source dir. */
static char *read_cue_data_file(const char *filename)
{
	char path[512];
	const char *dirs[] = { "test/data", "../test/data",
		                   "extract/test/data",
		                   "app/src/main/cpp/extract/test/data",
		                   "../app/src/main/cpp/extract/test/data", NULL };
	for (int i = 0; dirs[i]; i++) {
		snprintf(path, sizeof(path), "%s/%s", dirs[i], filename);
		FILE *f = fopen(path, "r");
		if (f) {
			fseek(f, 0, SEEK_END);
			long len = ftell(f);
			fseek(f, 0, SEEK_SET);
			char *buf = (char *) malloc(len + 1);
			fread(buf, 1, len, f);
			buf[len] = '\0';
			fclose(f);
			return buf;
		}
	}
	return NULL;
}

/* ── Test: Valid CUE parsing ─────────────────────────────────────────── */

static void test_valid_cue_parse(void)
{
	TEST(valid_cue_single_file);
	{
		const char *cue =
		    "FILE \"test.bin\" BINARY\n"
		    "  TRACK 01 MODE1/2352\n"
		    "    INDEX 01 00:00:00\n"
		    "  TRACK 02 AUDIO\n"
		    "    TITLE \"Test Song\"\n"
		    "    INDEX 01 00:02:00\n"
		    "  TRACK 03 AUDIO\n"
		    "    INDEX 01 00:04:00\n";

		/* 20 data + 150 audio + 150 audio = 320 sectors */
		long long bin_size = 320LL * CUE_SECTOR_SIZE;
		cue_disc_t disc;
		int n = cue_parse(cue, &bin_size, 1, &disc);

		if (n != 3) {
			FAIL("expected 3 tracks");
			return;
		}
		if (disc.num_files != 1) {
			FAIL("expected 1 file");
			return;
		}
		if (disc.tracks[0].type != CUE_TRACK_DATA) {
			FAIL("track 1 should be data");
			return;
		}
		if (disc.tracks[1].type != CUE_TRACK_AUDIO) {
			FAIL("track 2 should be audio");
			return;
		}
		if (strcmp(disc.tracks[1].title, "Test Song") != 0) {
			FAIL("track 2 title wrong");
			return;
		}
		if (disc.tracks[0].start_sector != 0) {
			FAIL("track 1 start should be 0");
			return;
		}
		if (disc.tracks[1].start_sector != 150) {
			FAIL("track 2 start should be 150");
			return;
		}
		if (disc.tracks[2].start_sector != 300) {
			FAIL("track 3 start should be 300");
			return;
		}
		/* Sector counts: track1=150, track2=150, track3=320-300=20 */
		if (disc.tracks[0].num_sectors != 150) {
			FAIL("track 1 sectors wrong");
			return;
		}
		if (disc.tracks[1].num_sectors != 150) {
			FAIL("track 2 sectors wrong");
			return;
		}
		if (disc.tracks[2].num_sectors != 20) {
			FAIL("track 3 sectors wrong");
			return;
		}
		PASS();
	}

	TEST(valid_cue_multi_file);
	{
		const char *cue =
		    "FILE \"data.bin\" BINARY\n"
		    "  TRACK 01 MODE1/2352\n"
		    "    INDEX 01 00:00:00\n"
		    "FILE \"audio.bin\" BINARY\n"
		    "  TRACK 02 AUDIO\n"
		    "    INDEX 01 00:00:00\n";

		long long sizes[2] = { 20LL * CUE_SECTOR_SIZE, 150LL * CUE_SECTOR_SIZE };
		cue_disc_t disc;
		int n = cue_parse(cue, sizes, 2, &disc);

		if (n != 2) {
			FAIL("expected 2 tracks");
			return;
		}
		if (disc.num_files != 2) {
			FAIL("expected 2 files");
			return;
		}
		if (disc.tracks[0].file_index != 0) {
			FAIL("track 1 file_index");
			return;
		}
		if (disc.tracks[1].file_index != 1) {
			FAIL("track 2 file_index");
			return;
		}
		if (disc.tracks[0].num_sectors != 20) {
			FAIL("track 1 sectors");
			return;
		}
		if (disc.tracks[1].num_sectors != 150) {
			FAIL("track 2 sectors");
			return;
		}
		PASS();
	}

	TEST(valid_cue_msf_conversion);
	{
		/* 00:00:00 = 0, 00:02:00 = 150, 01:00:00 = 4500 */
		if (cue_msf_to_sector("00:00:00") != 0) {
			FAIL("00:00:00");
			return;
		}
		if (cue_msf_to_sector("00:02:00") != 150) {
			FAIL("00:02:00");
			return;
		}
		if (cue_msf_to_sector("01:00:00") != 4500) {
			FAIL("01:00:00");
			return;
		}
		if (cue_msf_to_sector("00:00:01") != 1) {
			FAIL("00:00:01");
			return;
		}
		PASS();
	}
}

/* ── Test: Malformed CUE inputs ──────────────────────────────────────── */

static void test_malformed_cue(void)
{
	TEST(malformed_cue_null_input);
	{
		cue_disc_t disc;
		int n = cue_parse(NULL, NULL, 0, &disc);
		if (n != 0) {
			FAIL("should return 0 for NULL input");
			return;
		}
		PASS();
	}

	TEST(malformed_cue_null_output);
	{
		int n = cue_parse("FILE \"x\" BINARY\n", NULL, 0, NULL);
		if (n != 0) {
			FAIL("should return 0 for NULL output");
			return;
		}
		PASS();
	}

	TEST(malformed_cue_empty_string);
	{
		cue_disc_t disc;
		int n = cue_parse("", NULL, 0, &disc);
		if (n != 0) {
			FAIL("should return 0 for empty string");
			return;
		}
		PASS();
	}

	TEST(malformed_cue_track_before_file);
	{
		/* TRACK without a preceding FILE — should be ignored */
		const char *cue =
		    "TRACK 01 MODE1/2352\n"
		    "  INDEX 01 00:00:00\n";
		cue_disc_t disc;
		int n = cue_parse(cue, NULL, 0, &disc);
		if (n != 0) {
			FAIL("tracks before FILE should be ignored");
			return;
		}
		PASS();
	}

	TEST(malformed_cue_unclosed_quotes);
	{
		/* Missing closing quote on FILE — filename should be empty */
		const char *cue =
		    "FILE \"no_close\n"
		    "  TRACK 01 MODE1/2352\n"
		    "    INDEX 01 00:00:00\n";
		cue_disc_t disc;
		int n = cue_parse(cue, NULL, 0, &disc);
		/* File directive processes but filename is empty.
		 * Track should still parse since cur_file >= 0. */
		if (n != 1) {
			FAIL("should still parse 1 track");
			return;
		}
		if (disc.files[0].filename[0] != '\0') {
			FAIL("filename should be empty");
			return;
		}
		PASS();
	}

	TEST(malformed_cue_invalid_msf);
	{
		/* Garbage in MSF field — sscanf should fail, sector = 0 */
		const char *cue =
		    "FILE \"x.bin\" BINARY\n"
		    "TRACK 01 MODE1/2352\n"
		    "  INDEX 01 GARBAGE\n";
		cue_disc_t disc;
		int n = cue_parse(cue, NULL, 0, &disc);
		if (n != 1) {
			FAIL("should parse 1 track");
			return;
		}
		if (disc.tracks[0].start_sector != 0) {
			FAIL("bad MSF should give sector 0");
			return;
		}
		PASS();
	}

	TEST(malformed_cue_missing_index);
	{
		/* Track without INDEX 01 — start_sector stays 0 */
		const char *cue =
		    "FILE \"x.bin\" BINARY\n"
		    "TRACK 01 MODE1/2352\n"
		    "TRACK 02 AUDIO\n"
		    "  INDEX 01 00:02:00\n";
		long long sz = 300LL * CUE_SECTOR_SIZE;
		cue_disc_t disc;
		int n = cue_parse(cue, &sz, 1, &disc);
		if (n != 2) {
			FAIL("should parse 2 tracks");
			return;
		}
		if (disc.tracks[0].start_sector != 0) {
			FAIL("track 1 start should be 0");
			return;
		}
		PASS();
	}

	TEST(malformed_cue_negative_sectors_clamped);
	{
		/* start_sector beyond file size — num_sectors should be clamped to 0 */
		const char *cue =
		    "FILE \"tiny.bin\" BINARY\n"
		    "TRACK 01 MODE1/2352\n"
		    "  INDEX 01 10:00:00\n";          /* sector 45000, way past any small file */
		long long sz = 1LL * CUE_SECTOR_SIZE; /* 1 sector file */
		cue_disc_t disc;
		int n = cue_parse(cue, &sz, 1, &disc);
		if (n != 1) {
			FAIL("should parse 1 track");
			return;
		}
		if (disc.tracks[0].num_sectors < 0) {
			FAIL("negative sector count not clamped");
			return;
		}
		if (disc.tracks[0].num_sectors != 0) {
			FAIL("should be 0 sectors");
			return;
		}
		PASS();
	}

	TEST(malformed_cue_zero_size_file);
	{
		const char *cue =
		    "FILE \"empty.bin\" BINARY\n"
		    "TRACK 01 MODE1/2352\n"
		    "  INDEX 01 00:00:00\n";
		long long sz = 0;
		cue_disc_t disc;
		int n = cue_parse(cue, &sz, 1, &disc);
		if (n != 1) {
			FAIL("should parse 1 track");
			return;
		}
		/* With 0-size file, num_sectors stays 0 (fsize not > 0 path) */
		if (disc.tracks[0].num_sectors != 0) {
			FAIL("should be 0");
			return;
		}
		PASS();
	}
}

/* ── Test: ISO 9660 reader ───────────────────────────────────────────── */

static void test_iso_reader(void)
{
	int data_sectors;
	const char *file_content = "Hello from ISO!";
	unsigned char *img = build_minimal_iso(
	    "HELLO.TXT",
	    (const unsigned char *) file_content,
	    (int) strlen(file_content),
	    &data_sectors);

	/* Write to disk for fd-based testing */
	char bin_path[512];
	snprintf(bin_path, sizeof(bin_path), "%s/test_iso.bin", TEST_DIR);
	FILE *f = fopen(bin_path, "wb");
	if (!f) {
		fprintf(stderr, "Cannot create %s\n", bin_path);
		free(img);
		return;
	}
	fwrite(img, SECTOR_SIZE, data_sectors, f);
	fclose(f);
	free(img);

	TEST(iso_list_files_valid);
	{
		int fd = open_bin(bin_path);
		if (fd < 0) {
			FAIL("cannot open test BIN");
			return;
		}

		iso_file_list_t list;
		int rc = iso_list_files(fd, 0, data_sectors, &list);

		if (rc < 0) {
			FAIL("iso_list_files returned error");
			close_fd(fd);
			return;
		}
		/* Should find at least the file we put in */
		int found = 0;
		for (int i = 0; i < list.num_files; i++) {
			if (!list.files[i].is_dir && strstr(list.files[i].path, "hello")) {
				found = 1;
				if (list.files[i].size != (unsigned int) strlen(file_content)) {
					FAIL("file size mismatch");
					close_fd(fd);
					return;
				}
			}
		}
		if (!found) {
			FAIL("hello.txt not found in listing");
			close_fd(fd);
			return;
		}
		close_fd(fd);
		PASS();
	}

	TEST(iso_list_files_invalid_fd);
	{
		iso_file_list_t list;
		int rc = iso_list_files(-1, 0, 20, &list);
		if (rc != -1) {
			FAIL("should fail with invalid fd");
			return;
		}
		PASS();
	}

	TEST(iso_list_files_null_output);
	{
		int rc = iso_list_files(0, 0, 20, NULL);
		if (rc != -1) {
			FAIL("should fail with null output");
			return;
		}
		PASS();
	}

	TEST(iso_image_list_files_valid);
	{
		int iso_sectors;
		unsigned char *iso_img = build_minimal_iso_image(
		    "DIRECT.HOG",
		    (const unsigned char *) file_content,
		    (int) strlen(file_content),
		    &iso_sectors);
		char iso_path[512];
		snprintf(iso_path, sizeof(iso_path), "%s/test_iso_image.iso", TEST_DIR);
		FILE *iso_file = fopen(iso_path, "wb");
		if (!iso_file) {
			FAIL("cannot create test ISO image");
			free(iso_img);
			return;
		}
		fwrite(iso_img, USER_DATA_SIZE, iso_sectors, iso_file);
		fclose(iso_file);
		free(iso_img);

		int fd = open_bin(iso_path);
		if (fd < 0) {
			FAIL("cannot open test ISO image");
			return;
		}

		iso_file_list_t list;
		int rc = iso_list_image_files(fd, &list);
		if (rc < 0) {
			FAIL("iso_list_image_files returned error");
			close_fd(fd);
			return;
		}

		int found = 0;
		for (int i = 0; i < list.num_files; i++) {
			if (!list.files[i].is_dir && strcmp(list.files[i].path, "direct.hog") == 0)
				found = 1;
		}
		close_fd(fd);
		if (!found) {
			FAIL("direct.hog not found in ISO image listing");
			return;
		}
		PASS();
	}

	TEST(iso_image_list_files_invalid_fd);
	{
		iso_file_list_t list;
		int rc = iso_list_image_files(-1, &list);
		if (rc != -1) {
			FAIL("should fail with invalid ISO image fd");
			return;
		}
		PASS();
	}

	TEST(iso_image_list_files_null_output);
	{
		int rc = iso_list_image_files(0, NULL);
		if (rc != -1) {
			FAIL("should fail with null ISO image output");
			return;
		}
		PASS();
	}
}

/* ── Test: ISO extraction ────────────────────────────────────────────── */

static void test_iso_extraction(void)
{
	/* Build ISO with a .TXT file (should be ignored by game filter)
	 * and test extraction function */
	int data_sectors;
	const char *content = "test data";
	unsigned char *img = build_minimal_iso(
	    "TEST.HOG", /* .HOG extension will be matched by extraction filter */
	    (const unsigned char *) content,
	    (int) strlen(content),
	    &data_sectors);

	char bin_path[512];
	snprintf(bin_path, sizeof(bin_path), "%s/test_extract.bin", TEST_DIR);
	FILE *f = fopen(bin_path, "wb");
	fwrite(img, SECTOR_SIZE, data_sectors, f);
	fclose(f);
	free(img);

	TEST(iso_extract_files_valid);
	{
		static const char *exts[] = { "hog", "txt", NULL };
		char out_dir[512];
		snprintf(out_dir, sizeof(out_dir), "%s/extracted", TEST_DIR);
		mkdir_p(out_dir);

		int fd = open_bin(bin_path);
		if (fd < 0) {
			FAIL("cannot open test BIN");
			return;
		}

		iso_file_list_t list;
		iso_list_files(fd, 0, data_sectors, &list);

		int extracted = iso_extract_files(fd, 0, data_sectors,
		                                  &list, out_dir, exts, NULL, NULL);
		close_fd(fd);

		if (extracted < 1) {
			FAIL("should extract at least 1 file");
			return;
		}

		/* Verify extracted content */
		char check_path[512];
		snprintf(check_path, sizeof(check_path), "%s/test.hog", out_dir);
		FILE *check = fopen(check_path, "rb");
		if (!check) {
			FAIL("extracted file not found");
			return;
		}
		char buf[64];
		size_t nr = fread(buf, 1, sizeof(buf), check);
		fclose(check);
		if (nr != strlen(content) || memcmp(buf, content, nr) != 0) {
			FAIL("extracted content mismatch");
			return;
		}
		PASS();
	}

	TEST(iso_image_extract_files_valid);
	{
		int iso_sectors;
		const char *iso_content = "iso direct payload";
		unsigned char *iso_img = build_minimal_iso_image(
		    "IMAGE.HOG",
		    (const unsigned char *) iso_content,
		    (int) strlen(iso_content),
		    &iso_sectors);
		char iso_path[512];
		char out_dir[512];
		snprintf(iso_path, sizeof(iso_path), "%s/test_extract.iso", TEST_DIR);
		FILE *iso_file = fopen(iso_path, "wb");
		if (!iso_file) {
			FAIL("cannot create ISO image for extraction");
			free(iso_img);
			return;
		}
		fwrite(iso_img, USER_DATA_SIZE, iso_sectors, iso_file);
		fclose(iso_file);
		free(iso_img);

		snprintf(out_dir, sizeof(out_dir), "%s/extracted_iso_image", TEST_DIR);
		mkdir_p(out_dir);

		int fd = open_bin(iso_path);
		if (fd < 0) {
			FAIL("cannot open ISO image for extraction");
			return;
		}

		iso_file_list_t list;
		int listed = iso_list_image_files(fd, &list);
		if (listed < 0) {
			FAIL("iso_list_image_files failed before extraction");
			close_fd(fd);
			return;
		}

		static const char *exts[] = { "hog", NULL };
		int extracted = iso_extract_image_files(fd, &list, out_dir,
		                                        exts, NULL, NULL);
		close_fd(fd);
		if (extracted != 1) {
			FAIL("should extract 1 file from ISO image");
			return;
		}

		char check_path[512];
		snprintf(check_path, sizeof(check_path), "%s/image.hog", out_dir);
		FILE *check = fopen(check_path, "rb");
		if (!check) {
			FAIL("ISO image extracted file not found");
			return;
		}
		char buf[64];
		size_t nr = fread(buf, 1, sizeof(buf), check);
		fclose(check);
		if (nr != strlen(iso_content) || memcmp(buf, iso_content, nr) != 0) {
			FAIL("ISO image extracted content mismatch");
			return;
		}
		PASS();
	}
}

/* ── Test: File-based CUE parsing (test/data/*.cue) ──────────────────── */

static void test_file_based_cue(void)
{
	TEST(file_valid_single_data);
	{
		char *cue = read_cue_data_file("valid_single_data.cue");
		if (!cue) {
			FAIL("cannot read valid_single_data.cue");
			return;
		}
		long long sz = 20LL * CUE_SECTOR_SIZE;
		cue_disc_t disc;
		int n = cue_parse(cue, &sz, 1, &disc);
		free(cue);
		if (n != 1) {
			FAIL("expected 1 track");
			return;
		}
		if (disc.tracks[0].type != CUE_TRACK_DATA) {
			FAIL("should be data");
			return;
		}
		if (disc.num_files != 1) {
			FAIL("expected 1 file");
			return;
		}
		if (strcmp(disc.files[0].filename, "game.bin") != 0) {
			FAIL("filename mismatch");
			return;
		}
		PASS();
	}

	TEST(file_valid_data_plus_audio);
	{
		char *cue = read_cue_data_file("valid_data_plus_audio.cue");
		if (!cue) {
			FAIL("cannot read valid_data_plus_audio.cue");
			return;
		}
		long long sz = 320LL * CUE_SECTOR_SIZE;
		cue_disc_t disc;
		int n = cue_parse(cue, &sz, 1, &disc);
		free(cue);
		if (n != 3) {
			FAIL("expected 3 tracks");
			return;
		}
		if (disc.tracks[0].type != CUE_TRACK_DATA) {
			FAIL("track 1 type");
			return;
		}
		if (disc.tracks[1].type != CUE_TRACK_AUDIO) {
			FAIL("track 2 type");
			return;
		}
		if (strcmp(disc.tracks[1].title, "Track Two") != 0) {
			FAIL("track 2 title");
			return;
		}
		if (strcmp(disc.tracks[2].title, "Track Three") != 0) {
			FAIL("track 3 title");
			return;
		}
		if (disc.tracks[1].start_sector != 150) {
			FAIL("track 2 start");
			return;
		}
		PASS();
	}

	TEST(file_valid_multi_file);
	{
		char *cue = read_cue_data_file("valid_multi_file.cue");
		if (!cue) {
			FAIL("cannot read valid_multi_file.cue");
			return;
		}
		long long sizes[3] = { 20LL * CUE_SECTOR_SIZE, 150LL * CUE_SECTOR_SIZE, 200LL * CUE_SECTOR_SIZE };
		cue_disc_t disc;
		int n = cue_parse(cue, sizes, 3, &disc);
		free(cue);
		if (n != 3) {
			FAIL("expected 3 tracks");
			return;
		}
		if (disc.num_files != 3) {
			FAIL("expected 3 files");
			return;
		}
		if (disc.tracks[0].file_index != 0) {
			FAIL("track 1 file");
			return;
		}
		if (disc.tracks[1].file_index != 1) {
			FAIL("track 2 file");
			return;
		}
		if (disc.tracks[2].file_index != 2) {
			FAIL("track 3 file");
			return;
		}
		if (strcmp(disc.tracks[1].title, "Song One") != 0) {
			FAIL("track 2 title");
			return;
		}
		PASS();
	}

	TEST(file_valid_many_audio);
	{
		char *cue = read_cue_data_file("valid_many_audio.cue");
		if (!cue) {
			FAIL("cannot read valid_many_audio.cue");
			return;
		}
		long long sz = 1350LL * CUE_SECTOR_SIZE;
		cue_disc_t disc;
		int n = cue_parse(cue, &sz, 1, &disc);
		free(cue);
		if (n != 9) {
			FAIL("expected 9 tracks");
			return;
		}
		if (disc.tracks[0].type != CUE_TRACK_DATA) {
			FAIL("track 1 type");
			return;
		}
		for (int i = 1; i < 9; i++) {
			if (disc.tracks[i].type != CUE_TRACK_AUDIO) {
				FAIL("audio track type");
				return;
			}
		}
		PASS();
	}

	TEST(file_valid_pregap_index00);
	{
		char *cue = read_cue_data_file("valid_pregap.cue");
		if (!cue) {
			FAIL("cannot read valid_pregap.cue");
			return;
		}
		long long sz = 500LL * CUE_SECTOR_SIZE;
		cue_disc_t disc;
		int n = cue_parse(cue, &sz, 1, &disc);
		free(cue);
		if (n != 2) {
			FAIL("expected 2 tracks");
			return;
		}
		/* Parser only uses INDEX 01, should ignore INDEX 00 */
		if (disc.tracks[0].start_sector != 150) {
			FAIL("track 1 INDEX 01 should be 00:02:00=150");
			return;
		}
		if (disc.tracks[1].start_sector != 450) {
			FAIL("track 2 INDEX 01 should be 00:06:00=450");
			return;
		}
		PASS();
	}

	TEST(file_valid_mode2);
	{
		char *cue = read_cue_data_file("valid_mode2.cue");
		if (!cue) {
			FAIL("cannot read valid_mode2.cue");
			return;
		}
		long long sz = 20LL * CUE_SECTOR_SIZE;
		cue_disc_t disc;
		int n = cue_parse(cue, &sz, 1, &disc);
		free(cue);
		if (n != 1) {
			FAIL("expected 1 track");
			return;
		}
		if (disc.tracks[0].type != CUE_TRACK_DATA) {
			FAIL("mode2 should be data");
			return;
		}
		PASS();
	}

	TEST(file_valid_extra_whitespace);
	{
		char *cue = read_cue_data_file("valid_extra_whitespace.cue");
		if (!cue) {
			FAIL("cannot read valid_extra_whitespace.cue");
			return;
		}
		long long sz = 300LL * CUE_SECTOR_SIZE;
		cue_disc_t disc;
		int n = cue_parse(cue, &sz, 1, &disc);
		free(cue);
		if (n != 2) {
			FAIL("expected 2 tracks");
			return;
		}
		if (strcmp(disc.tracks[1].title, "Tabbed Title") != 0) {
			FAIL("title mismatch");
			return;
		}
		PASS();
	}

	TEST(file_bad_no_file_directive);
	{
		char *cue = read_cue_data_file("bad_no_file_directive.cue");
		if (!cue) {
			FAIL("cannot read bad_no_file_directive.cue");
			return;
		}
		cue_disc_t disc;
		int n = cue_parse(cue, NULL, 0, &disc);
		free(cue);
		if (n != 0) {
			FAIL("should parse 0 tracks without FILE");
			return;
		}
		PASS();
	}

	TEST(file_bad_unclosed_quote);
	{
		char *cue = read_cue_data_file("bad_unclosed_quote.cue");
		if (!cue) {
			FAIL("cannot read bad_unclosed_quote.cue");
			return;
		}
		cue_disc_t disc;
		int n = cue_parse(cue, NULL, 0, &disc);
		free(cue);
		if (n != 1) {
			FAIL("should still parse 1 track");
			return;
		}
		if (disc.files[0].filename[0] != '\0') {
			FAIL("filename should be empty");
			return;
		}
		PASS();
	}

	TEST(file_bad_garbage_msf);
	{
		char *cue = read_cue_data_file("bad_garbage_msf.cue");
		if (!cue) {
			FAIL("cannot read bad_garbage_msf.cue");
			return;
		}
		cue_disc_t disc;
		int n = cue_parse(cue, NULL, 0, &disc);
		free(cue);
		if (n != 1) {
			FAIL("should parse 1 track");
			return;
		}
		if (disc.tracks[0].start_sector != 0) {
			FAIL("garbage MSF should give 0");
			return;
		}
		PASS();
	}

	TEST(file_bad_missing_index);
	{
		char *cue = read_cue_data_file("bad_missing_index.cue");
		if (!cue) {
			FAIL("cannot read bad_missing_index.cue");
			return;
		}
		long long sz = 300LL * CUE_SECTOR_SIZE;
		cue_disc_t disc;
		int n = cue_parse(cue, &sz, 1, &disc);
		free(cue);
		if (n != 2) {
			FAIL("should parse 2 tracks");
			return;
		}
		if (disc.tracks[0].start_sector != 0) {
			FAIL("track 1 start should be 0");
			return;
		}
		PASS();
	}

	TEST(file_bad_start_past_eof);
	{
		char *cue = read_cue_data_file("bad_start_past_eof.cue");
		if (!cue) {
			FAIL("cannot read bad_start_past_eof.cue");
			return;
		}
		long long sz = 1LL * CUE_SECTOR_SIZE;
		cue_disc_t disc;
		int n = cue_parse(cue, &sz, 1, &disc);
		free(cue);
		if (n != 1) {
			FAIL("should parse 1 track");
			return;
		}
		if (disc.tracks[0].num_sectors != 0) {
			FAIL("should be 0 sectors");
			return;
		}
		PASS();
	}

	TEST(file_bad_empty);
	{
		char *cue = read_cue_data_file("bad_empty.cue");
		if (!cue) {
			FAIL("cannot read bad_empty.cue");
			return;
		}
		cue_disc_t disc;
		int n = cue_parse(cue, NULL, 0, &disc);
		free(cue);
		if (n != 0) {
			FAIL("should parse 0 tracks");
			return;
		}
		PASS();
	}

	TEST(file_bad_backwards_index);
	{
		char *cue = read_cue_data_file("bad_backwards_index.cue");
		if (!cue) {
			FAIL("cannot read bad_backwards_index.cue");
			return;
		}
		long long sz = 500LL * CUE_SECTOR_SIZE;
		cue_disc_t disc;
		int n = cue_parse(cue, &sz, 1, &disc);
		free(cue);
		if (n != 2) {
			FAIL("should parse 2 tracks");
			return;
		}
		if (disc.tracks[0].num_sectors < 0) {
			FAIL("negative sectors");
			return;
		}
		if (disc.tracks[0].num_sectors != 0) {
			FAIL("should be 0 sectors (clamped)");
			return;
		}
		PASS();
	}
}

/* ── Test: CUE parser edge cases ─────────────────────────────────────── */

static void test_cue_edge_cases(void)
{
	TEST(cue_case_insensitive_directives);
	{
		const char *cue =
		    "file \"test.bin\" binary\n"
		    "  track 01 mode1/2352\n"
		    "    index 01 00:00:00\n"
		    "  track 02 audio\n"
		    "    title \"Lower Case\"\n"
		    "    index 01 00:02:00\n";
		long long sz = 300LL * CUE_SECTOR_SIZE;
		cue_disc_t disc;
		int n = cue_parse(cue, &sz, 1, &disc);
		if (n != 2) {
			FAIL("expected 2 tracks");
			return;
		}
		if (disc.tracks[1].type != CUE_TRACK_AUDIO) {
			FAIL("type wrong");
			return;
		}
		if (strcmp(disc.tracks[1].title, "Lower Case") != 0) {
			FAIL("title wrong");
			return;
		}
		PASS();
	}

	TEST(cue_windows_line_endings);
	{
		const char *cue =
		    "FILE \"test.bin\" BINARY\r\n"
		    "  TRACK 01 MODE1/2352\r\n"
		    "    INDEX 01 00:00:00\r\n"
		    "  TRACK 02 AUDIO\r\n"
		    "    TITLE \"CRLF Song\"\r\n"
		    "    INDEX 01 00:02:00\r\n";
		long long sz = 300LL * CUE_SECTOR_SIZE;
		cue_disc_t disc;
		int n = cue_parse(cue, &sz, 1, &disc);
		if (n != 2) {
			FAIL("expected 2 tracks");
			return;
		}
		if (strcmp(disc.tracks[1].title, "CRLF Song") != 0) {
			FAIL("title mismatch");
			return;
		}
		PASS();
	}

	TEST(cue_title_without_quotes);
	{
		const char *cue =
		    "FILE \"test.bin\" BINARY\n"
		    "  TRACK 01 AUDIO\n"
		    "    TITLE No Quotes Here\n"
		    "    INDEX 01 00:00:00\n";
		long long sz = 150LL * CUE_SECTOR_SIZE;
		cue_disc_t disc;
		int n = cue_parse(cue, &sz, 1, &disc);
		if (n != 1) {
			FAIL("expected 1 track");
			return;
		}
		if (disc.tracks[0].title[0] != '\0') {
			FAIL("title should be empty");
			return;
		}
		PASS();
	}

	TEST(cue_max_files_boundary);
	{
		char big_cue[8192];
		int pos = 0;
		for (int i = 0; i < CUE_MAX_FILES; i++) {
			pos += snprintf(big_cue + pos, sizeof(big_cue) - pos,
			                "FILE \"file%02d.bin\" BINARY\nTRACK %02d MODE1/2352\n  INDEX 01 00:00:00\n",
			                i, i + 1);
		}
		pos += snprintf(big_cue + pos, sizeof(big_cue) - pos,
		                "FILE \"overflow.bin\" BINARY\nTRACK 51 MODE1/2352\n  INDEX 01 00:00:00\n");

		cue_disc_t disc;
		cue_parse(big_cue, NULL, 0, &disc);
		if (disc.num_files != CUE_MAX_FILES) {
			FAIL("should cap at CUE_MAX_FILES");
			return;
		}
		PASS();
	}

	TEST(cue_msf_edge_values);
	{
		if (cue_msf_to_sector("00:00:74") != 74) {
			FAIL("00:00:74");
			return;
		}
		if (cue_msf_to_sector("01:00:00") != 4500) {
			FAIL("01:00:00");
			return;
		}
		if (cue_msf_to_sector("99:59:74") != 99 * 4500 + 59 * 75 + 74) {
			FAIL("99:59:74");
			return;
		}
		if (cue_msf_to_sector("00:00:00") != 0) {
			FAIL("00:00:00");
			return;
		}
		PASS();
	}
}

/* ── Test: ISO 9660 name cleaning (indirect via listing) ─────────────── */

static void test_iso_name_cleaning(void)
{
	TEST(iso_name_version_suffix);
	{
		int sectors;
		unsigned char *img = build_minimal_iso("DESCENT2.HOG;1",
		                                       (const unsigned char *) "x", 1, &sectors);
		char path[512];
		snprintf(path, sizeof(path), "%s/name_test1.bin", TEST_DIR);
		FILE *f = fopen(path, "wb");
		fwrite(img, SECTOR_SIZE, sectors, f);
		fclose(f);
		free(img);

		int fd = open_bin(path);
		iso_file_list_t list;
		iso_list_files(fd, 0, sectors, &list);
		close_fd(fd);

		int found = 0;
		for (int i = 0; i < list.num_files; i++)
			if (strcmp(list.files[i].path, "descent2.hog") == 0) found = 1;
		if (!found) {
			FAIL("version suffix not stripped");
			return;
		}
		PASS();
	}

	TEST(iso_name_trailing_dot);
	{
		int sectors;
		unsigned char *img = build_minimal_iso("FILE.",
		                                       (const unsigned char *) "y", 1, &sectors);
		char path[512];
		snprintf(path, sizeof(path), "%s/name_test2.bin", TEST_DIR);
		FILE *f = fopen(path, "wb");
		fwrite(img, SECTOR_SIZE, sectors, f);
		fclose(f);
		free(img);

		int fd = open_bin(path);
		iso_file_list_t list;
		iso_list_files(fd, 0, sectors, &list);
		close_fd(fd);

		int found = 0;
		for (int i = 0; i < list.num_files; i++)
			if (strcmp(list.files[i].path, "file") == 0) found = 1;
		if (!found) {
			FAIL("trailing dot not stripped");
			return;
		}
		PASS();
	}

	TEST(iso_name_lowercase);
	{
		int sectors;
		unsigned char *img = build_minimal_iso("ROBOTS.HAM",
		                                       (const unsigned char *) "z", 1, &sectors);
		char path[512];
		snprintf(path, sizeof(path), "%s/name_test3.bin", TEST_DIR);
		FILE *f = fopen(path, "wb");
		fwrite(img, SECTOR_SIZE, sectors, f);
		fclose(f);
		free(img);

		int fd = open_bin(path);
		iso_file_list_t list;
		iso_list_files(fd, 0, sectors, &list);
		close_fd(fd);

		int found = 0;
		for (int i = 0; i < list.num_files; i++)
			if (strcmp(list.files[i].path, "robots.ham") == 0) found = 1;
		if (!found) {
			FAIL("not lowercased");
			return;
		}
		PASS();
	}
}

/* ── Test: ISO invalid PVD ───────────────────────────────────────────── */

static void test_iso_invalid_pvd(void)
{
	TEST(iso_bad_pvd_signature);
	{
		int total = 20;
		unsigned char *img = (unsigned char *) calloc(total, SECTOR_SIZE);
		int m, s, f;
		for (int i = 0; i < total; i++) {
			lba_to_msf(i, &m, &s, &f);
			build_mode1_sector(img + i * SECTOR_SIZE, m, s, f, NULL);
		}
		char path[512];
		snprintf(path, sizeof(path), "%s/bad_pvd.bin", TEST_DIR);
		FILE *fp = fopen(path, "wb");
		fwrite(img, SECTOR_SIZE, total, fp);
		fclose(fp);
		free(img);

		int fd = open_bin(path);
		iso_file_list_t list;
		int rc = iso_list_files(fd, 0, total, &list);
		close_fd(fd);

		if (rc != -1) {
			FAIL("should fail with invalid PVD");
			return;
		}
		PASS();
	}
}

/* ── Test: ISO extraction with extension filter ──────────────────────── */

static void test_iso_ext_filter(void)
{
	TEST(iso_extract_filter_rejects);
	{
		int sectors;
		unsigned char *img = build_minimal_iso("README.TXT",
		                                       (const unsigned char *) "readme", 6, &sectors);
		char path[512];
		snprintf(path, sizeof(path), "%s/filter_test.bin", TEST_DIR);
		FILE *f = fopen(path, "wb");
		fwrite(img, SECTOR_SIZE, sectors, f);
		fclose(f);
		free(img);

		int fd = open_bin(path);
		iso_file_list_t list;
		iso_list_files(fd, 0, sectors, &list);

		char out_dir[512];
		snprintf(out_dir, sizeof(out_dir), "%s/filter_out", TEST_DIR);
		mkdir_p(out_dir);

		static const char *hog_only[] = { "hog", NULL };
		int extracted = iso_extract_files(fd, 0, sectors, &list, out_dir,
		                                  hog_only, NULL, NULL);
		close_fd(fd);

		if (extracted != 0) {
			FAIL("should extract 0 files (no .hog)");
			return;
		}
		PASS();
	}

	TEST(iso_extract_null_filter_extracts_all);
	{
		int sectors;
		unsigned char *img = build_minimal_iso("DATA.BIN",
		                                       (const unsigned char *) "all", 3, &sectors);
		char path[512];
		snprintf(path, sizeof(path), "%s/allext_test.bin", TEST_DIR);
		FILE *f = fopen(path, "wb");
		fwrite(img, SECTOR_SIZE, sectors, f);
		fclose(f);
		free(img);

		int fd = open_bin(path);
		iso_file_list_t list;
		iso_list_files(fd, 0, sectors, &list);

		char out_dir[512];
		snprintf(out_dir, sizeof(out_dir), "%s/allext_out", TEST_DIR);
		mkdir_p(out_dir);

		int extracted = iso_extract_files(fd, 0, sectors, &list, out_dir,
		                                  NULL, NULL, NULL);
		close_fd(fd);

		if (extracted < 1) {
			FAIL("null filter should extract all");
			return;
		}
		PASS();
	}
}

/* ── Test: Full round-trip integration ───────────────────────────────── */

static void test_integration_round_trip(void)
{
	TEST(round_trip_cue_to_extract);
	{
		/* 1. Build a minimal ISO data track */
		int data_sectors;
		const char *game_data = "DESCENT2 DATA PAYLOAD";
		unsigned char *img = build_minimal_iso("DESCENT2.HOG",
		                                       (const unsigned char *) game_data, (int) strlen(game_data),
		                                       &data_sectors);

		/* 2. Write the combined BIN (data + 2 audio tracks of 150 sectors) */
		int audio_per_track = 150;
		write_test_bin("roundtrip.bin", img, data_sectors,
		               audio_per_track, 2);
		free(img);

		/* 3. Build matching CUE text */
		int t2_start = data_sectors;
		int t3_start = data_sectors + audio_per_track;
		char cue_text[512];
		snprintf(cue_text, sizeof(cue_text),
		         "FILE \"roundtrip.bin\" BINARY\n"
		         "  TRACK 01 MODE1/2352\n"
		         "    INDEX 01 00:00:00\n"
		         "  TRACK 02 AUDIO\n"
		         "    TITLE \"Audio A\"\n"
		         "    INDEX 01 %02d:%02d:%02d\n"
		         "  TRACK 03 AUDIO\n"
		         "    TITLE \"Audio B\"\n"
		         "    INDEX 01 %02d:%02d:%02d\n",
		         t2_start / 4500, (t2_start / 75) % 60, t2_start % 75,
		         t3_start / 4500, (t3_start / 75) % 60, t3_start % 75);

		/* 4. Parse the CUE */
		long long bin_size = (long long) (data_sectors + audio_per_track * 2) * CUE_SECTOR_SIZE;
		cue_disc_t disc;
		int n = cue_parse(cue_text, &bin_size, 1, &disc);
		if (n != 3) {
			FAIL("cue parse: expected 3 tracks");
			return;
		}
		if (disc.tracks[0].type != CUE_TRACK_DATA) {
			FAIL("track 1 type");
			return;
		}
		if (disc.tracks[1].type != CUE_TRACK_AUDIO) {
			FAIL("track 2 type");
			return;
		}
		if (disc.tracks[0].num_sectors != data_sectors) {
			FAIL("data sector count");
			return;
		}

		/* 5. Open BIN and list ISO files from data track */
		char bin_path[512];
		snprintf(bin_path, sizeof(bin_path), "%s/roundtrip.bin", TEST_DIR);
		int fd = open_bin(bin_path);
		if (fd < 0) {
			FAIL("cannot open roundtrip.bin");
			return;
		}

		iso_file_list_t list;
		int rc = iso_list_files(fd, disc.tracks[0].start_sector,
		                        disc.tracks[0].num_sectors, &list);
		if (rc < 0) {
			FAIL("iso_list_files failed");
			close_fd(fd);
			return;
		}

		/* 6. Extract and verify */
		char out_dir[512];
		snprintf(out_dir, sizeof(out_dir), "%s/roundtrip_out", TEST_DIR);
		mkdir_p(out_dir);

		static const char *exts[] = { "hog", NULL };
		int extracted = iso_extract_files(fd, disc.tracks[0].start_sector,
		                                  disc.tracks[0].num_sectors,
		                                  &list, out_dir, exts, NULL, NULL);
		close_fd(fd);

		if (extracted != 1) {
			FAIL("should extract 1 file");
			return;
		}

		/* Verify content */
		char check[512];
		snprintf(check, sizeof(check), "%s/descent2.hog", out_dir);
		FILE *f = fopen(check, "rb");
		if (!f) {
			FAIL("extracted file missing");
			return;
		}
		char buf[64];
		size_t nr = fread(buf, 1, sizeof(buf), f);
		fclose(f);
		if (nr != strlen(game_data) || memcmp(buf, game_data, nr) != 0) {
			FAIL("content mismatch");
			return;
		}
		PASS();
	}
}

/* ── Test: Edge cases for sector count computation ───────────────────── */

static void test_sector_count_edge_cases(void)
{
	TEST(sector_count_large_start_offset);
	{
		/* Track starts past file end — should get 0 sectors, not negative */
		const char *cue =
		    "FILE \"x.bin\" BINARY\n"
		    "TRACK 01 MODE1/2352\n"
		    "  INDEX 01 00:00:00\n"
		    "TRACK 02 AUDIO\n"
		    "  INDEX 01 50:00:00\n"; /* sector 225000 */
		long long sz = 100LL * CUE_SECTOR_SIZE;
		cue_disc_t disc;
		cue_parse(cue, &sz, 1, &disc);
		if (disc.tracks[1].num_sectors < 0) {
			FAIL("negative sector count for track past EOF");
			return;
		}
		PASS();
	}

	TEST(sector_count_overlapping_tracks);
	{
		/* Track 2 starts before track 1 — should get 0 sectors for track 1 */
		const char *cue =
		    "FILE \"x.bin\" BINARY\n"
		    "TRACK 01 MODE1/2352\n"
		    "  INDEX 01 00:04:00\n"
		    "TRACK 02 AUDIO\n"
		    "  INDEX 01 00:02:00\n";
		long long sz = 500LL * CUE_SECTOR_SIZE;
		cue_disc_t disc;
		cue_parse(cue, &sz, 1, &disc);
		/* Track 1 starts at 300, track 2 at 150 → track 1 gets clamped to 0 */
		if (disc.tracks[0].num_sectors < 0) {
			FAIL("negative sector count for overlapping tracks");
			return;
		}
		PASS();
	}
}

/* ── Main ────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
	(void) argc;
	(void) argv;

	printf("Creating test fixtures in %s/\n", TEST_DIR);
	mkdir_p(TEST_DIR);

	printf("\n--- CUE Parser Tests ---\n");
	test_valid_cue_parse();
	test_malformed_cue();
	test_sector_count_edge_cases();

	printf("\n--- File-Based CUE Tests ---\n");
	test_file_based_cue();
	test_cue_edge_cases();

	printf("\n--- ISO 9660 Reader Tests ---\n");
	test_iso_reader();
	test_iso_extraction();
	test_iso_name_cleaning();
	test_iso_invalid_pvd();
	test_iso_ext_filter();

	printf("\n--- Integration Tests ---\n");
	test_integration_round_trip();

	printf("\n%d/%d tests passed\n", tests_passed, tests_run);
	return (tests_passed == tests_run) ? 0 : 1;
}
