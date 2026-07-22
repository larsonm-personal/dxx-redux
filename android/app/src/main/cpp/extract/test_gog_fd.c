#include "inno_reader.h"
#include "game_file_extensions.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#define OPEN_RB(path) _open((path), _O_RDONLY | _O_BINARY)
#define CLOSE_FD(fd)  _close(fd)
#define ci_cmp        _stricmp
#else
#include <fcntl.h>
#include <strings.h>
#include <unistd.h>
#define OPEN_RB(path) open((path), O_RDONLY)
#define CLOSE_FD(fd)  close(fd)
#define ci_cmp        strcasecmp
#endif

static int has_game_extension(const char *path)
{
	return dxx_has_android_game_file_extension(path);
}

static const char *basename_only(const char *path)
{
	const char *last = path;
	for (const char *p = path; *p; p++) {
		if (*p == '/' || *p == '\\') last = p + 1;
	}
	return last;
}

static int contains_file(inno_archive_t *arc, const char *expected)
{
	for (int i = 0; i < arc->file_count; i++) {
		if (!has_game_extension(arc->files[i].destination)) continue;
		if (ci_cmp(basename_only(arc->files[i].destination), expected) == 0)
			return 1;
	}
	return 0;
}

static int file_exists(const char *path)
{
	FILE *file = fopen(path, "rb");
	if (!file) return 0;
	fclose(file);
	return 1;
}

static int check_checksum_extraction(inno_archive_t *arc, const char *label,
                                     int gog_galaxy)
{
	int selected = -1;
	uint64_t selected_size = UINT64_MAX;
	for (int i = 0; i < arc->file_count; i++) {
		inno_file_entry_t *file = &arc->files[i];
		if (file->gog_galaxy != gog_galaxy ||
		    (gog_galaxy && file->external_size == 0) ||
		    file->location == 0xFFFFFFFF ||
		    file->location >= (uint32_t) arc->data_entry_count)
			continue;
		inno_data_entry_t *data = &arc->data_entries[file->location];
		uint64_t output_size = gog_galaxy ? file->external_size : data->file_size;
		if (data->checksum_type == INNO_CHECKSUM_SHA1 && output_size < selected_size) {
			selected = i;
			selected_size = output_size;
		}
	}
	if (selected < 0) return 0;

	char output_path[96];
	snprintf(output_path, sizeof(output_path), "test_gog_fd_%s_%s.tmp",
	         label, gog_galaxy ? "galaxy" : "regular");
	remove(output_path);
	if (inno_extract_file(arc, selected, output_path, NULL, NULL) < 0 ||
	    !file_exists(output_path)) {
		fprintf(stderr, "%s: valid checksum extraction failed for %s\n",
		        label, arc->files[selected].destination);
		remove(output_path);
		return 1;
	}
	remove(output_path);

	inno_data_entry_t *data = &arc->data_entries[arc->files[selected].location];
	data->sha1[0] ^= 0x01;
	int result = inno_extract_file(arc, selected, output_path, NULL, NULL);
	data->sha1[0] ^= 0x01;
	if (result == 0 || file_exists(output_path)) {
		fprintf(stderr, "%s: checksum mismatch published output for %s\n",
		        label, arc->files[selected].destination);
		remove(output_path);
		return 1;
	}
	return 0;
}

static int check_galaxy_checksum_order(void)
{
	static const uint8_t inner_zlib[] = {
		0x78, 0x01, 0x01, 0x05, 0x00, 0xfa, 0xff, 0x68,
		0x65, 0x6c, 0x6c, 0x6f, 0x06, 0x2c, 0x02, 0x15
	};
	static const uint8_t inner_sha1[20] = {
		0x0f, 0xf3, 0x23, 0xed, 0x08, 0x89, 0x69, 0xe7, 0x0c, 0xd0,
		0x93, 0x1c, 0x7f, 0xde, 0xf1, 0x5e, 0x25, 0x93, 0xc3, 0xa5
	};
	const char *source_path = "test_gog_fd_galaxy_source.tmp";
	const char *output_path = "test_gog_fd_galaxy_output.tmp";
	remove(source_path);
	remove(output_path);
	FILE *source = fopen(source_path, "wb");
	if (!source) return 1;
	const uint8_t magic[4] = { 'z', 'l', 'b', 0x1a };
	int source_failed = fwrite(magic, 1, sizeof(magic), source) != sizeof(magic) ||
	                    fwrite(inner_zlib, 1, sizeof(inner_zlib), source) != sizeof(inner_zlib);
	if (fclose(source) != 0) source_failed = 1;
	if (source_failed) {
		remove(source_path);
		return 1;
	}

	inno_archive_t arc;
	inno_data_entry_t data;
	memset(&arc, 0, sizeof(arc));
	memset(&data, 0, sizeof(data));
	arc.fd = OPEN_RB(source_path);
	if (arc.fd < 0) {
		remove(source_path);
		return 1;
	}
	arc.compression = INNO_COMPRESS_STORED;
	arc.file_count = 1;
	arc.data_entry_count = 1;
	arc.data_entries = &data;
	strcpy(arc.files[0].destination, "galaxy.bin");
	arc.files[0].external_size = 5;
	arc.files[0].gog_galaxy = 1;
	data.file_size = sizeof(inner_zlib);
	data.chunk_compressed_size = sizeof(inner_zlib);
	data.checksum_type = INNO_CHECKSUM_SHA1;
	memcpy(data.sha1, inner_sha1, sizeof(inner_sha1));

	int failures = 0;
	if (inno_extract_file(&arc, 0, output_path, NULL, NULL) < 0) {
		fprintf(stderr, "synthetic Galaxy checksum extraction failed\n");
		failures++;
	} else {
		uint8_t output[5];
		FILE *file = fopen(output_path, "rb");
		if (!file || fread(output, 1, sizeof(output), file) != sizeof(output) ||
		    memcmp(output, "hello", sizeof(output)) != 0) {
			fprintf(stderr, "synthetic Galaxy output mismatch\n");
			failures++;
		}
		if (file) fclose(file);
	}
	remove(output_path);
	data.sha1[0] ^= 0x01;
	if (inno_extract_file(&arc, 0, output_path, NULL, NULL) == 0 ||
	    file_exists(output_path)) {
		fprintf(stderr, "synthetic Galaxy checksum mismatch published output\n");
		failures++;
	}
	CLOSE_FD(arc.fd);
	remove(source_path);
	remove(output_path);
	return failures ? 1 : 0;
}

static int check_installer(const char *path, const char **expected, int expected_count,
                           int expected_game_count, const char *label)
{
	int fd = OPEN_RB(path);
	if (fd < 0) {
		printf("skip missing fixture: %s\n", path);
		return 0;
	}

	inno_archive_t arc;
	int n = inno_open_fd(fd, &arc);
	CLOSE_FD(fd);
	if (n < 0) {
		fprintf(stderr, "failed to open fd fixture: %s\n", path);
		return 1;
	}

	int game_count = 0;
	int failures = 0;
	for (int i = 0; i < arc.file_count; i++) {
		if (has_game_extension(arc.files[i].destination)) game_count++;
	}
	if (game_count != expected_game_count) {
		fprintf(stderr, "%s: expected %d game files, got %d\n",
		        path, expected_game_count, game_count);
		failures++;
	}
	for (int i = 0; i < expected_count; i++) {
		if (!contains_file(&arc, expected[i])) {
			fprintf(stderr, "%s: missing %s\n", path, expected[i]);
			failures++;
		}
	}
	failures += check_checksum_extraction(&arc, label, 0);
	failures += check_checksum_extraction(&arc, label, 1);
	inno_close(&arc);
	return failures ? 1 : 0;
}

int main(int argc, char **argv)
{
	if (argc != 3) {
		fprintf(stderr, "usage: test_gog_fd <d1 installer> <d2 installer>\n");
		return 1;
	}

	const char *d1_expected[] = { "DESCENT.HOG", "DESCENT.PIG" };
	const char *d2_expected[] = {
		"DESCENT2.HAM", "DESCENT2.HOG", "DESCENT2.S11",
		"DESCENT2.S22", "GROUPA.PIG"
	};

	int failures = 0;
	failures += check_galaxy_checksum_order();
	failures += check_installer(argv[1], d1_expected, 2, 7, "d1");
	failures += check_installer(argv[2], d2_expected, 5, 21, "d2");
	return failures ? 1 : 0;
}
