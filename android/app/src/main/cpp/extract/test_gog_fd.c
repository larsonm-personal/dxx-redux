#include "inno_reader.h"
#include "game_file_extensions.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zlib.h>

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

static int file_equals(const char *path, const void *expected, size_t expected_size)
{
	uint8_t actual[64];
	if (expected_size > sizeof(actual)) return 0;
	FILE *file = fopen(path, "rb");
	if (!file) return 0;
	size_t size = fread(actual, 1, sizeof(actual), file);
	fclose(file);
	return size == expected_size && memcmp(actual, expected, expected_size) == 0;
}

static void put_u32(uint8_t *buffer, size_t *position, uint32_t value)
{
	for (int i = 0; i < 4; i++)
		buffer[(*position)++] = (uint8_t) (value >> (i * 8));
}

static void put_u64(uint8_t *buffer, size_t *position, uint64_t value)
{
	for (int i = 0; i < 8; i++)
		buffer[(*position)++] = (uint8_t) (value >> (i * 8));
}

static size_t build_header_fixture(uint8_t *buffer, const inno_version_t *version,
                                   size_t digest_size,
                                   inno_compress_method_t compression)
{
	memset(buffer, 0, 512);
	size_t position = (version->patch >= 8 ? 29 : 28) * 4;
	position += 16 * 4;
	position += 20 + 8 + 4;
	position += digest_size + 8;
	position += 12 + 5;
	buffer[position++] = (uint8_t) compression;
	position += 2;
	if (version->patch >= 3) position += 2;
	if (version->patch >= 6) position += 4;
	position += 5;
	return position;
}

static size_t append_data_fixture(uint8_t *buffer, size_t position,
                                  size_t digest_size, int index)
{
	put_u32(buffer, &position, (uint32_t) index);
	put_u32(buffer, &position, (uint32_t) (index + 10));
	put_u32(buffer, &position, (uint32_t) (100 + index));
	put_u64(buffer, &position, (uint64_t) (200 + index));
	put_u64(buffer, &position, (uint64_t) (300 + index));
	put_u64(buffer, &position, (uint64_t) (400 + index));
	for (size_t i = 0; i < digest_size; i++)
		buffer[position++] = (uint8_t) (0x20 + index * 0x20 + i);
	position += 16;
	buffer[position++] = index == 0 ? 0x80 : 0x10;
	buffer[position++] = 0;
	return position;
}

static int check_checksum_layout_transition(void)
{
	const inno_version_t versions[3] = {
		{ 5, 3, 0, 1 },
		{ 5, 3, 8, 1 },
		{ 5, 3, 9, 1 }
	};
	const inno_checksum_type_t expected_types[3] = {
		INNO_CHECKSUM_MD5,
		INNO_CHECKSUM_MD5,
		INNO_CHECKSUM_SHA1
	};
	const size_t expected_sizes[3] = { 16, 16, 20 };
	int failures = 0;
	for (int version_index = 0; version_index < 3; version_index++) {
		inno_checksum_type_t type;
		size_t digest_size;
		if (inno_test_checksum_layout(&versions[version_index], &type,
		                              &digest_size) < 0 ||
		    type != expected_types[version_index] ||
		    digest_size != expected_sizes[version_index]) {
			fprintf(stderr, "5.3.%d checksum layout mismatch\n",
			        versions[version_index].patch);
			failures++;
			continue;
		}

		uint8_t header[512];
		size_t header_size = build_header_fixture(header, &versions[version_index],
		                                          digest_size,
		                                          INNO_COMPRESS_LZMA1);
		inno_compress_method_t compression = INNO_COMPRESS_STORED;
		if (inno_test_parse_header_stream(header, header_size,
		                                  &versions[version_index],
		                                  &compression) < 0 ||
		    compression != INNO_COMPRESS_LZMA1) {
			fprintf(stderr, "5.3.%d password digest width misaligned header\n",
			        versions[version_index].patch);
			failures++;
		}

		uint8_t table[192] = { 0 };
		size_t table_size = append_data_fixture(table, 0, digest_size, 0);
		table_size = append_data_fixture(table, table_size, digest_size, 1);
		inno_data_entry_t entries[2];
		if (inno_test_parse_data_entries(table, table_size,
		                                 &versions[version_index], entries, 2) < 0) {
			fprintf(stderr, "5.3.%d multi-entry checksum table failed\n",
			        versions[version_index].patch);
			failures++;
			continue;
		}
		for (int entry = 0; entry < 2; entry++) {
			uint8_t expected_first = (uint8_t) (0x20 + entry * 0x20);
			if (entries[entry].checksum_type != expected_types[version_index] ||
			    entries[entry].checksum[0] != expected_first ||
			    entries[entry].checksum[digest_size - 1] !=
			        (uint8_t) (expected_first + digest_size - 1) ||
			    entries[entry].file_offset != (uint64_t) (200 + entry) ||
			    entries[entry].chunk_compressed_size != (uint64_t) (400 + entry) ||
			    entries[entry].chunk_compressed != (entry == 0) ||
			    entries[entry].call_instruction_optimized != (entry == 1)) {
				fprintf(stderr, "5.3.%d data entry %d is misaligned\n",
				        versions[version_index].patch, entry);
				failures++;
			}
		}
	}
	return failures ? 1 : 0;
}

static int check_checksum_extraction(inno_archive_t *arc, const char *label,
                                     int gog_galaxy)
{
	int selected = -1;
	uint64_t selected_size = UINT64_MAX;
	for (int i = 0; i < arc->file_count; i++) {
		inno_file_entry_t *file = &arc->files[i];
		if (!has_game_extension(file->destination) ||
		    file->gog_galaxy != gog_galaxy ||
		    file->location == 0xFFFFFFFF ||
		    file->location >= (uint32_t) arc->data_entry_count)
			continue;
		inno_data_entry_t *data = &arc->data_entries[file->location];
		uint64_t output_size = gog_galaxy && file->external_size > 0
		                           ? file->external_size
		                           : data->file_size;
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
	data->checksum[0] ^= 0x01;
	int result = inno_extract_file(arc, selected, output_path, NULL, NULL);
	data->checksum[0] ^= 0x01;
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
	arc.source_size = sizeof(magic) + sizeof(inner_zlib);
	arc.file_count = 1;
	arc.data_entry_count = 1;
	arc.data_entries = &data;
	strcpy(arc.files[0].destination, "galaxy.bin");
	arc.files[0].external_size = 0;
	arc.files[0].gog_galaxy = 1;
	data.file_size = sizeof(inner_zlib);
	data.chunk_compressed_size = sizeof(inner_zlib);
	data.checksum_type = INNO_CHECKSUM_SHA1;
	memcpy(data.checksum, inner_sha1, sizeof(inner_sha1));

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
	arc.files[0].external_size = 5;
	if (inno_extract_file(&arc, 0, output_path, NULL, NULL) < 0) {
		fprintf(stderr, "synthetic sized Galaxy extraction failed\n");
		failures++;
	}
	remove(output_path);
	data.checksum[0] ^= 0x01;
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

static void init_range_archive(inno_archive_t *arc, inno_data_entry_t *data,
                               int fd, uint64_t source_size)
{
	memset(arc, 0, sizeof(*arc));
	memset(data, 0, sizeof(*data));
	arc->fd = fd;
	arc->source_size = source_size;
	arc->compression = INNO_COMPRESS_STORED;
	arc->file_count = 1;
	arc->data_entry_count = 1;
	arc->data_entries = data;
	strcpy(arc->files[0].destination, "range.bin");
	data->checksum_type = INNO_CHECKSUM_SHA1;
}

static int expect_range_failure(inno_archive_t *arc, const char *output_path,
                                const char *case_name)
{
	remove(output_path);
	if (inno_extract_file(arc, 0, output_path, NULL, NULL) == 0 ||
	    file_exists(output_path)) {
		fprintf(stderr, "%s was accepted\n", case_name);
		remove(output_path);
		return 1;
	}
	return 0;
}

static int check_chunk_range_boundaries(void)
{
	static const uint8_t exact_sha1[20] = {
		0x92, 0x4f, 0x61, 0x66, 0x1a, 0x34, 0x72, 0xda, 0x74, 0x30,
		0x7a, 0x35, 0xf2, 0xc8, 0xd2, 0x2e, 0x07, 0xe8, 0x4a, 0x4d
	};
	static const uint8_t exact_md5[16] = {
		0xe2, 0xfc, 0x71, 0x4c, 0x47, 0x27, 0xee, 0x93,
		0x95, 0xf3, 0x24, 0xcd, 0x2e, 0x7f, 0x33, 0x1f
	};
	const uint8_t magic[4] = { 'z', 'l', 'b', 0x1a };
	const uint8_t stored_payload[5] = { 'a', 'b', 'c', 'd', 'X' };
	const char *source_path = "test_gog_fd_range_source.tmp";
	const char *output_path = "test_gog_fd_range_output.tmp";
	remove(source_path);
	remove(output_path);
	FILE *source = fopen(source_path, "wb");
	if (!source) return 1;
	int write_failed = fwrite(magic, 1, sizeof(magic), source) != sizeof(magic) ||
	                   fwrite(stored_payload, 1, sizeof(stored_payload), source) != sizeof(stored_payload);
	if (fclose(source) != 0) write_failed = 1;
	if (write_failed) {
		remove(source_path);
		return 1;
	}

	int fd = OPEN_RB(source_path);
	if (fd < 0) {
		remove(source_path);
		return 1;
	}
	inno_archive_t arc;
	inno_data_entry_t data;
	init_range_archive(&arc, &data, fd, sizeof(magic) + sizeof(stored_payload));
	data.chunk_compressed_size = 4;
	data.file_offset = 1;
	data.file_size = 3;
	memcpy(data.checksum, exact_sha1, sizeof(exact_sha1));
	int failures = 0;
	if (inno_extract_file(&arc, 0, output_path, NULL, NULL) < 0 ||
	    !file_equals(output_path, "bcd", 3)) {
		fprintf(stderr, "exact stored chunk boundary failed\n");
		failures++;
	}
	remove(output_path);
	data.file_offset = 0;
	data.file_size = 4;
	data.checksum_type = INNO_CHECKSUM_MD5;
	memcpy(data.checksum, exact_md5, sizeof(exact_md5));
	if (inno_extract_file(&arc, 0, output_path, NULL, NULL) < 0 ||
	    !file_equals(output_path, "abcd", 4)) {
		fprintf(stderr, "valid MD5 extraction failed\n");
		failures++;
	}
	remove(output_path);
	data.checksum[0] ^= 0x01;
	failures += expect_range_failure(&arc, output_path, "MD5 mismatch");
	data.checksum[0] ^= 0x01;
	data.checksum_type = INNO_CHECKSUM_SHA1;

	data.file_offset = 1;
	data.file_size = 4;
	failures += expect_range_failure(&arc, output_path, "stored one-byte-over");
	data.file_offset = UINT64_MAX - 1;
	data.file_size = 4;
	failures += expect_range_failure(&arc, output_path, "wrapping file");
	data.file_offset = 0;
	data.file_size = 1;
	data.chunk_offset = UINT64_MAX;
	failures += expect_range_failure(&arc, output_path, "wrapping chunk");
	data.chunk_offset = 0;
	data.chunk_compressed_size = 6;
	failures += expect_range_failure(&arc, output_path, "physical span");
	data.chunk_compressed_size = 64ULL * 1024ULL * 1024ULL;
	data.file_offset = data.chunk_compressed_size;
	arc.source_size = 4 + data.chunk_compressed_size;
	failures += expect_range_failure(&arc, output_path, "streamed stored one-byte-over");
	CLOSE_FD(fd);

	uint8_t compressed[64];
	uLongf compressed_size = sizeof(compressed);
	if (compress2(compressed, &compressed_size, (const Bytef *) "abcd", 4,
	              Z_BEST_SPEED) != Z_OK) {
		remove(source_path);
		return 1;
	}
	source = fopen(source_path, "wb");
	if (!source) return 1;
	write_failed = fwrite(magic, 1, sizeof(magic), source) != sizeof(magic) ||
	               fwrite(compressed, 1, compressed_size, source) != compressed_size ||
	               fwrite("X", 1, 1, source) != 1;
	if (fclose(source) != 0) write_failed = 1;
	fd = write_failed ? -1 : OPEN_RB(source_path);
	if (fd < 0) {
		remove(source_path);
		return 1;
	}
	init_range_archive(&arc, &data, fd, 4 + compressed_size + 1);
	arc.compression = INNO_COMPRESS_ZLIB;
	data.chunk_compressed = 1;
	data.chunk_compressed_size = compressed_size;
	data.file_offset = 2;
	data.file_size = 3;
	failures += expect_range_failure(&arc, output_path, "zlib expanded one-byte-over");
	CLOSE_FD(fd);
	remove(source_path);
	remove(output_path);
	return failures ? 1 : 0;
}

static int check_installer(const char *path, const char **expected, int expected_count,
                           int expected_game_count, int expected_galaxy_game_count,
                           const char *label)
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
	int galaxy_game_count = 0;
	int failures = 0;
	for (int i = 0; i < arc.file_count; i++) {
		if (!has_game_extension(arc.files[i].destination)) continue;
		game_count++;
		if (arc.files[i].gog_galaxy) galaxy_game_count++;
	}
	if (game_count != expected_game_count) {
		fprintf(stderr, "%s: expected %d game files, got %d\n",
		        path, expected_game_count, game_count);
		failures++;
	}
	if (expected_galaxy_game_count >= 0 &&
	    galaxy_game_count != expected_galaxy_game_count) {
		fprintf(stderr, "%s: expected %d Galaxy game files, got %d\n",
		        path, expected_galaxy_game_count, galaxy_game_count);
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
	failures += check_checksum_layout_transition();
	failures += check_chunk_range_boundaries();
	failures += check_galaxy_checksum_order();
	failures += check_installer(argv[1], d1_expected, 2, 7, 7, "d1");
	failures += check_installer(argv[2], d2_expected, 5, 21, -1, "d2");
	return failures ? 1 : 0;
}
