#include "inno_reader.h"
#include "game_file_extensions.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zlib.h>

#define INNO_TEST_VERSION_ID_SIZE 64

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

static int select_game_file(const char *path, void *user_data)
{
	(void) user_data;
	return has_game_extension(path);
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
	for (uint32_t i = 0; i < arc->file_count; i++) {
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

static void set_u16(uint8_t *buffer, size_t offset, uint16_t value)
{
	buffer[offset] = (uint8_t) value;
	buffer[offset + 1] = (uint8_t) (value >> 8);
}

static void set_u32(uint8_t *buffer, size_t offset, uint32_t value)
{
	for (int i = 0; i < 4; i++)
		buffer[offset + (size_t) i] = (uint8_t) (value >> (i * 8));
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

static int expect_version_id(const char *id, int major, int minor, int patch,
                             int unicode)
{
	uint8_t field[INNO_TEST_VERSION_ID_SIZE] = { 0 };
	inno_version_t version;
	size_t len = strlen(id);

	if (len >= sizeof(field))
		return 1;
	memcpy(field, id, len);
	if (inno_test_parse_version_id(field, &version) < 0 ||
	    version.major != major || version.minor != minor ||
	    version.patch != patch || version.unicode != unicode) {
		fprintf(stderr, "valid version ID rejected: %s\n", id);
		return 1;
	}
	return 0;
}

static int expect_invalid_version_id(const char *id)
{
	uint8_t field[INNO_TEST_VERSION_ID_SIZE] = { 0 };
	inno_version_t version;
	size_t len = strlen(id);

	if (len >= sizeof(field))
		return 1;
	memcpy(field, id, len);
	if (inno_test_parse_version_id(field, &version) == 0) {
		fprintf(stderr, "invalid version ID accepted: %s\n", id);
		return 1;
	}
	return 0;
}

static int check_version_id_bounds(void)
{
	static const char *invalid_ids[] = {
		"",
		"Inno Setup Setup Data ()",
		"Inno Setup Setup Data (.3.0)",
		"Inno Setup Setup Data (5..0)",
		"Inno Setup Setup Data (5.3.)",
		"Inno Setup Setup Data (5.3)",
		"Inno Setup Setup Data (2147483648.3.0)",
		"Inno Setup Setup Data (5.2147483648.0)",
		"Inno Setup Setup Data (5.3.2147483648)",
		"Inno Setup Setup Data (5.3.0) garbage",
		"Inno Setup Setup Data (5.3.0)(u)",
		"Inno Setup Setup Data (5.3.0) (x)",
		"Inno Setup Setup Data (5.3.0) (u) garbage"
	};
	uint8_t field[INNO_TEST_VERSION_ID_SIZE];
	inno_version_t version;
	int failures = 0;

	failures += expect_version_id("Inno Setup Setup Data (5.3.0)", 5, 3, 0, 0);
	failures += expect_version_id("Inno Setup Setup Data (5.5.0) (u)", 5, 5, 0, 1);
	failures += expect_version_id("Inno Setup Setup Data (5.5.6) (u)", 5, 5, 6, 1);
	failures += expect_version_id("Inno Setup Setup Data (5.5.7) (u)", 5, 5, 7, 1);
	failures += expect_version_id("Inno Setup Setup Data (5.5.7) (U)", 5, 5, 7, 1);
	failures += expect_version_id("Inno Setup Setup Data (5.5.8) (u)", 5, 5, 7, 1);
	failures += expect_version_id("Inno Setup Setup Data (5.6.0) (u)", 5, 6, 0, 1);
	failures += expect_version_id("Inno Setup Setup Data (5.6.2) (u)", 5, 6, 2, 1);
	for (size_t i = 0; i < sizeof(invalid_ids) / sizeof(invalid_ids[0]); i++)
		failures += expect_invalid_version_id(invalid_ids[i]);

	memset(field, '7', sizeof(field));
	memcpy(field, "Inno Setup Setup Data (", 23);
	if (inno_test_parse_version_id(field, &version) == 0) {
		fprintf(stderr, "unterminated digit field accepted\n");
		failures++;
	}

	for (size_t closing = 59; closing <= 62; closing++) {
		size_t tail_start = closing - 9;
		memset(field, 0, sizeof(field));
		memcpy(field, "Inno Setup Setup Data (", 23);
		memset(field + 23, '0', tail_start - 23);
		memcpy(field + tail_start, "5.6.2) (u)", 10);
		if (inno_test_parse_version_id(field, &version) < 0 ||
		    version.major != 5 || version.minor != 6 ||
		    version.patch != 2 || !version.unicode) {
			fprintf(stderr, "version ID ending at byte %zu rejected\n", closing);
			failures++;
		}
	}
	memset(field, 0, sizeof(field));
	memcpy(field, "Inno Setup Setup Data (", 23);
	memset(field + 23, '0', 31);
	memcpy(field + 54, "5.6.2) (u)", 10);
	if (inno_test_parse_version_id(field, &version) == 0) {
		fprintf(stderr, "unterminated version ID ending at byte 63 accepted\n");
		failures++;
	}

	memset(field, 0, sizeof(field));
	memcpy(field, "Inno Setup Setup Data (5.6.2) (u)", 33);
	field[34] = 'X';
	if (inno_test_parse_version_id(field, &version) == 0) {
		fprintf(stderr, "nonzero version ID padding accepted\n");
		failures++;
	}

	return failures ? 1 : 0;
}

static int decode_utf16_fixture(const uint16_t *units, size_t unit_count,
                                char *output, size_t output_size)
{
	uint8_t buffer[4 + INNO_PATH_LEN * 2];
	if (unit_count > INNO_PATH_LEN)
		return -1;
	set_u32(buffer, 0, (uint32_t) (unit_count * 2u));
	for (size_t i = 0; i < unit_count; i++)
		set_u16(buffer, 4u + i * 2u, units[i]);
	return inno_test_read_string(buffer, 4u + unit_count * 2u,
	                             output, output_size, 1);
}

static int check_unicode_destination_paths(void)
{
	static const uint16_t mixed_units[] = {
		'A', 0x00E9, 0x6F22, 0xD83D, 0xDE80, '.', 'h', 'o', 'g'
	};
	static const char mixed_utf8[] = {
		'A', (char) 0xC3, (char) 0xA9,
		(char) 0xE6, (char) 0xBC, (char) 0xA2,
		(char) 0xF0, (char) 0x9F, (char) 0x9A, (char) 0x80,
		'.', 'h', 'o', 'g', '\0'
	};
	static const uint16_t malformed[][2] = {
		{ 0xD800, 'A' },
		{ 0xDC00, 'A' },
		{ 0xD800, 0xD800 },
		{ 'A', 0 }
	};
	char output[INNO_PATH_LEN];
	int failures = 0;
	if (decode_utf16_fixture(mixed_units,
	                         sizeof(mixed_units) / sizeof(mixed_units[0]),
	                         output, sizeof(output)) < 0 ||
	    strcmp(output, mixed_utf8) != 0 ||
	    !inno_test_destination_path_valid(output)) {
		fprintf(stderr, "valid UTF-16 destination was not preserved\n");
		failures++;
	}
	for (size_t i = 0; i < sizeof(malformed) / sizeof(malformed[0]); i++) {
		if (decode_utf16_fixture(malformed[i], 2,
		                         output, sizeof(output)) == 0) {
			fprintf(stderr, "malformed UTF-16 destination accepted\n");
			failures++;
		}
	}

	uint8_t odd[] = { 1, 0, 0, 0, 'A' };
	if (inno_test_read_string(odd, sizeof(odd), output,
	                          sizeof(output), 1) == 0) {
		fprintf(stderr, "odd UTF-16 destination accepted\n");
		failures++;
	}

	uint16_t capacity_units[INNO_PATH_LEN];
	for (size_t i = 0; i < INNO_PATH_LEN; i++)
		capacity_units[i] = 'a';
	if (decode_utf16_fixture(capacity_units, INNO_PATH_LEN - 1u,
	                         output, sizeof(output)) < 0 ||
	    strlen(output) != INNO_PATH_LEN - 1u) {
		fprintf(stderr, "exact-capacity UTF-8 destination rejected\n");
		failures++;
	}
	if (decode_utf16_fixture(capacity_units, INNO_PATH_LEN,
	                         output, sizeof(output)) == 0) {
		fprintf(stderr, "over-capacity UTF-8 destination accepted\n");
		failures++;
	}

	static const char invalid_question[] = "bad?name.hog";
	static const char invalid_pipe[] = "bad|name.hog";
	static const char invalid_trailing[] = "bad.\\name.hog";
	if (inno_test_destination_path_valid(invalid_question) ||
	    inno_test_destination_path_valid(invalid_pipe) ||
	    inno_test_destination_path_valid(invalid_trailing)) {
		fprintf(stderr, "Windows-invalid destination accepted\n");
		failures++;
	}

	inno_archive_t archive;
	inno_file_entry_t files[2];
	memset(&archive, 0, sizeof(archive));
	memset(files, 0, sizeof(files));
	archive.files = files;
	archive.file_count = 2;
	memcpy(files[0].destination, "one\\", 4);
	memcpy(files[0].destination + 4, mixed_utf8, sizeof(mixed_utf8));
	memcpy(files[1].destination, "two\\", 4);
	memcpy(files[1].destination + 4, mixed_utf8, sizeof(mixed_utf8));
	if (inno_output_names_unique(&archive, NULL, NULL)) {
		fprintf(stderr, "flattened Unicode basename collision accepted\n");
		failures++;
	}
	files[1].destination[4] = 'B';
	if (!inno_output_names_unique(&archive, NULL, NULL)) {
		fprintf(stderr, "distinct Unicode basenames collided\n");
		failures++;
	}
	strcpy(files[0].destination, "one\\FILE.HOG");
	strcpy(files[1].destination, "two\\file.hog");
	if (inno_output_names_unique(&archive, NULL, NULL)) {
		fprintf(stderr, "ASCII case-only basename collision accepted\n");
		failures++;
	}
	return failures ? 1 : 0;
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

static void append_empty_string(uint8_t *buffer, size_t *position)
{
	put_u32(buffer, position, 0);
}

static void append_utf16_string(uint8_t *buffer, size_t *position,
                                const char *text)
{
	size_t len = strlen(text);

	put_u32(buffer, position, (uint32_t) (len * 2));
	for (size_t i = 0; i < len; i++) {
		buffer[(*position)++] = (uint8_t) text[i];
		buffer[(*position)++] = 0;
	}
}

static uint8_t *build_file_catalog_fixture(uint32_t file_count,
                                           size_t *fixture_size)
{
	static const inno_version_t version = { 5, 3, 0, 1 };
	size_t capacity = 512 + (size_t) file_count * 128;
	uint8_t *buffer = (uint8_t *) calloc(capacity, 1);
	size_t count_position = 28 * 4 + 7 * 4;
	size_t position;

	if (!buffer)
		return NULL;
	position = build_header_fixture(buffer, &version, 16,
	                                INNO_COMPRESS_LZMA1);
	put_u32(buffer, &count_position, file_count);
	for (uint32_t i = 0; i < file_count; i++) {
		append_empty_string(buffer, &position);
		if (i + 1 == file_count)
			append_utf16_string(buffer, &position, "late.hog");
		else
			append_empty_string(buffer, &position);
		for (int string_index = 0; string_index < 8; string_index++)
			append_empty_string(buffer, &position);
		position += 20 + 4 + 4 + 8 + 2 + 4 + 1;
		if (position > capacity) {
			free(buffer);
			return NULL;
		}
	}
	*fixture_size = position;
	return buffer;
}

static int check_complete_file_catalog(void)
{
	static const inno_version_t version = { 5, 3, 0, 1 };
	static const uint32_t valid_counts[] = { 512, 513, 1024, 4096 };
	char last_destination[INNO_PATH_LEN];
	uint32_t parsed_count;
	int failures = 0;

	for (size_t i = 0; i < sizeof(valid_counts) / sizeof(valid_counts[0]); i++) {
		size_t fixture_size;
		uint8_t *fixture =
		    build_file_catalog_fixture(valid_counts[i], &fixture_size);
		if (!fixture ||
		    inno_test_parse_file_catalog(fixture, fixture_size, &version,
		                                 &parsed_count, last_destination,
		                                 sizeof(last_destination)) < 0 ||
		    parsed_count != valid_counts[i] ||
		    strcmp(last_destination, "late.hog") != 0) {
			fprintf(stderr, "%u-entry file catalog was incomplete\n",
			        valid_counts[i]);
			failures++;
		}
		free(fixture);
	}

	size_t fixture_size;
	uint8_t *fixture = build_file_catalog_fixture(1, &fixture_size);
	inno_test_set_allocation_fail_after(0);
	if (!fixture ||
	    inno_test_parse_file_catalog(fixture, fixture_size, &version,
	                                 &parsed_count, last_destination,
	                                 sizeof(last_destination)) == 0) {
		fprintf(stderr, "file catalog allocation failure was accepted\n");
		failures++;
	}
	inno_test_set_allocation_fail_after(-1);
	free(fixture);

	fixture = build_file_catalog_fixture(4097, &fixture_size);
	if (!fixture ||
	    inno_test_parse_file_catalog(fixture, fixture_size, &version,
	                                 &parsed_count, last_destination,
	                                 sizeof(last_destination)) == 0) {
		fprintf(stderr, "over-budget file catalog was accepted\n");
		failures++;
	}
	free(fixture);
	return failures ? 1 : 0;
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
	buffer[position++] = index == 0 ? 0xc0 : 0x10;
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
			    entries[entry].chunk_encrypted != (entry == 0) ||
			    entries[entry].call_instruction_optimized != (entry == 1)) {
				fprintf(stderr, "5.3.%d data entry %d is misaligned\n",
				        versions[version_index].patch, entry);
				failures++;
			}
		}
	}
	return failures ? 1 : 0;
}

static int check_unsigned_entry_bounds(void)
{
	static const uint32_t rejected_counts[] = {
		INT_MAX,
		(uint32_t) INT_MAX + 1u,
		UINT32_MAX - 1u,
		UINT32_MAX
	};
	int failures = 0;

	if (!inno_test_entry_count_allowed(0) ||
	    !inno_test_entry_count_allowed(4096)) {
		fprintf(stderr, "valid entry count rejected\n");
		failures++;
	}
	for (size_t i = 0;
	     i < sizeof(rejected_counts) / sizeof(rejected_counts[0]); i++) {
		if (inno_test_entry_count_allowed(rejected_counts[i])) {
			fprintf(stderr, "oversized entry count %u accepted\n",
			        rejected_counts[i]);
			failures++;
		}
	}
	if (inno_test_data_location_valid(0, 1, 0) ||
	    inno_test_data_location_valid(1, 0, 0) ||
	    !inno_test_data_location_valid(4096, 1, 0) ||
	    !inno_test_data_location_valid(4096, 1, 4095) ||
	    inno_test_data_location_valid(4096, 1, 4096) ||
	    inno_test_data_location_valid(4096, 1, INT_MAX) ||
	    inno_test_data_location_valid(4096, 1, (uint32_t) INT_MAX + 1u) ||
	    inno_test_data_location_valid(4096, 1, UINT32_MAX - 1u) ||
	    inno_test_data_location_valid(4096, 1, UINT32_MAX)) {
		fprintf(stderr, "data location validation mismatch\n");
		failures++;
	}
	return failures ? 1 : 0;
}

static int check_checksum_extraction(inno_archive_t *arc, const char *label,
                                     int gog_galaxy)
{
	int selected = -1;
	uint64_t selected_size = UINT64_MAX;
	for (uint32_t i = 0; i < arc->file_count; i++) {
		inno_file_entry_t *file = &arc->files[i];
		const inno_data_entry_t *data = inno_file_data_entry(arc, i);
		if (!has_game_extension(file->destination) ||
		    file->gog_galaxy != gog_galaxy ||
		    !data)
			continue;
		uint64_t output_size = gog_galaxy && file->external_size > 0
		                           ? file->external_size
		                           : data->file_size;
		if (data->checksum_type == INNO_CHECKSUM_SHA1 && output_size < selected_size) {
			selected = (int) i;
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

	inno_data_entry_t *data =
	    &arc->data_entries[arc->files[selected].location];
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
	inno_file_entry_t entry;
	memset(&arc, 0, sizeof(arc));
	memset(&data, 0, sizeof(data));
	memset(&entry, 0, sizeof(entry));
	arc.fd = OPEN_RB(source_path);
	if (arc.fd < 0) {
		remove(source_path);
		return 1;
	}
	arc.compression = INNO_COMPRESS_STORED;
	arc.source_size = sizeof(magic) + sizeof(inner_zlib);
	arc.file_count = 1;
	arc.files = &entry;
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

static void init_range_archive(inno_archive_t *arc, inno_file_entry_t *entry,
                               inno_data_entry_t *data, int fd,
                               uint64_t source_size)
{
	memset(arc, 0, sizeof(*arc));
	memset(entry, 0, sizeof(*entry));
	memset(data, 0, sizeof(*data));
	arc->fd = fd;
	arc->source_size = source_size;
	arc->compression = INNO_COMPRESS_STORED;
	arc->file_count = 1;
	arc->files = entry;
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

static int count_progress(const char *current_file, long long bytes_done,
                          long long bytes_total, void *user_data)
{
	int *calls = (int *) user_data;
	(void) current_file;
	(void) bytes_done;
	(void) bytes_total;
	(*calls)++;
	return 0;
}

static int check_encrypted_chunk_rejection(void)
{
	static const uint8_t payload_sha1[20] = {
		0xe5, 0xe9, 0xfa, 0x1b, 0xa3, 0x1e, 0xcd, 0x1a, 0xe8, 0x4f,
		0x75, 0xca, 0xaa, 0x47, 0x4f, 0x3a, 0x66, 0x3f, 0x05, 0xf4
	};
	static const inno_compress_method_t methods[] = {
		INNO_COMPRESS_STORED,
		INNO_COMPRESS_ZLIB,
		INNO_COMPRESS_LZMA1,
		INNO_COMPRESS_LZMA2
	};
	const uint8_t magic[4] = { 'z', 'l', 'b', 0x1a };
	const char *source_path = "test_gog_fd_encrypted_source.tmp";
	const char *output_path = "test_gog_fd_encrypted_output.tmp";
	remove(source_path);
	remove(output_path);
	FILE *source = fopen(source_path, "wb");
	if (!source) return 1;
	int write_failed = fwrite(magic, 1, sizeof(magic), source) != sizeof(magic) ||
	                   fwrite("secret", 1, 6, source) != 6;
	if (fclose(source) != 0) write_failed = 1;
	int fd = write_failed ? -1 : OPEN_RB(source_path);
	if (fd < 0) {
		remove(source_path);
		return 1;
	}

	inno_archive_t arc;
	inno_file_entry_t entry;
	inno_data_entry_t data;
	init_range_archive(&arc, &entry, &data, fd, sizeof(magic) + 6);
	data.chunk_compressed_size = 6;
	data.file_size = 6;
	data.chunk_encrypted = 1;
	memcpy(data.checksum, payload_sha1, sizeof(payload_sha1));

	int failures = 0;
	for (size_t i = 0; i < sizeof(methods) / sizeof(methods[0]); i++) {
		int progress_calls = 0;
		arc.compression = methods[i];
		data.chunk_compressed = methods[i] != INNO_COMPRESS_STORED;
		remove(output_path);
		if (inno_extract_file(&arc, 0, output_path, count_progress,
		                      &progress_calls) == 0 ||
		    progress_calls != 0 || file_exists(output_path) ||
		    arc.extracted_files != 0 || arc.extracted_bytes != 0) {
			fprintf(stderr,
			        "encrypted method %d reached payload handling or output\n",
			        methods[i]);
			failures++;
		}
	}

	data.chunk_encrypted = 0;
	data.chunk_compressed = 0;
	arc.compression = INNO_COMPRESS_STORED;
	if (inno_extract_file(&arc, 0, output_path, NULL, NULL) < 0 ||
	    !file_equals(output_path, "secret", 6)) {
		fprintf(stderr, "unencrypted stored compatibility failed\n");
		failures++;
	}
	CLOSE_FD(fd);
	remove(source_path);
	remove(output_path);
	return failures ? 1 : 0;
}

static int run_buffered_decoder_case(inno_compress_method_t method,
                                     const uint8_t *compressed,
                                     size_t compressed_size,
                                     uint64_t file_offset,
                                     const uint8_t checksum[20],
                                     const char *expected,
                                     int should_succeed,
                                     const char *case_name)
{
	const uint8_t magic[4] = { 'z', 'l', 'b', 0x1a };
	const char *source_path = "test_gog_fd_decoder_source.tmp";
	const char *output_path = "test_gog_fd_decoder_output.tmp";
	remove(source_path);
	remove(output_path);
	FILE *source = fopen(source_path, "wb");
	if (!source) return 1;
	int write_failed = fwrite(magic, 1, sizeof(magic), source) != sizeof(magic) ||
	                   fwrite(compressed, 1, compressed_size, source) != compressed_size;
	if (fclose(source) != 0) write_failed = 1;
	int fd = write_failed ? -1 : OPEN_RB(source_path);
	if (fd < 0) {
		remove(source_path);
		return 1;
	}

	inno_archive_t arc;
	inno_file_entry_t entry;
	inno_data_entry_t data;
	init_range_archive(&arc, &entry, &data, fd,
	                   sizeof(magic) + compressed_size);
	arc.compression = method;
	data.chunk_compressed = 1;
	data.chunk_compressed_size = compressed_size;
	data.file_offset = file_offset;
	data.file_size = 4;
	memcpy(data.checksum, checksum, 20);

	int result = inno_extract_file(&arc, 0, output_path, NULL, NULL);
	int failed = should_succeed
	                 ? result < 0 || !file_equals(output_path, expected, 4)
	                 : result == 0 || file_exists(output_path);
	if (failed)
		fprintf(stderr, "buffered decoder case failed: %s\n", case_name);
	CLOSE_FD(fd);
	remove(source_path);
	remove(output_path);
	return failed;
}

static int check_buffered_decoder_failures(void)
{
	static const uint8_t abcd_sha1[20] = {
		0x81, 0xfe, 0x8b, 0xfe, 0x87, 0x57, 0x6c, 0x3e, 0xcb, 0x22,
		0x42, 0x6f, 0x8e, 0x57, 0x84, 0x73, 0x82, 0x91, 0x7a, 0xcf
	};
	static const uint8_t zzzz_sha1[20] = {
		0x98, 0x65, 0xd4, 0x83, 0xbc, 0x5a, 0x94, 0xf2, 0xe3, 0x00,
		0x56, 0xfc, 0x25, 0x6e, 0xd3, 0x06, 0x6a, 0xf5, 0x4d, 0x04
	};
	static const uint8_t lzma1[] = {
		0x5d, 0x00, 0x10, 0x00, 0x00,
		0x00, 0x30, 0x98, 0x88, 0x98, 0x3e, 0x1e, 0x93, 0xb7, 0x8d,
		0xfc, 0x3e, 0x6c, 0x5f, 0xa0, 0x89, 0xb3, 0x6b, 0xe2, 0x92,
		0x70, 0xfb, 0x3c, 0x61, 0x2c, 0xad, 0x24, 0x94, 0xd8, 0xda,
		0x06, 0xff, 0xfe, 0x01, 0x50, 0x00
	};
	static const uint8_t lzma2[] = {
		0x00,
		0xe0, 0x0f, 0xff, 0x00, 0x1e, 0x5d, 0x00, 0x30, 0x98, 0x88,
		0x98, 0x3e, 0x1e, 0x93, 0xb7, 0x8d, 0xfc, 0x3e, 0x6c, 0x5f,
		0xa0, 0x89, 0xb3, 0x6b, 0xe2, 0x92, 0x70, 0xfb, 0x3c, 0x61,
		0x2c, 0xad, 0x24, 0x8f, 0xba, 0xaf, 0x13, 0x00
	};
	uint8_t plain[4096];
	memcpy(plain, "abcd", 4);
	memset(plain + 4, 'Z', sizeof(plain) - 4);
	uint8_t zlib_data[128];
	uLongf zlib_size = sizeof(zlib_data);
	if (compress2(zlib_data, &zlib_size, plain, sizeof(plain),
	              Z_BEST_SPEED) != Z_OK)
		return 1;

	int failures = 0;
	failures += run_buffered_decoder_case(
	    INNO_COMPRESS_ZLIB, zlib_data, zlib_size, 0, abcd_sha1,
	    "abcd", 1, "valid zlib");
	failures += run_buffered_decoder_case(
	    INNO_COMPRESS_LZMA1, lzma1, sizeof(lzma1), 0, abcd_sha1,
	    "abcd", 1, "valid LZMA1");
	failures += run_buffered_decoder_case(
	    INNO_COMPRESS_LZMA2, lzma2, sizeof(lzma2), 0, abcd_sha1,
	    "abcd", 1, "valid LZMA2");

	failures += run_buffered_decoder_case(
	    INNO_COMPRESS_ZLIB, zlib_data, zlib_size - 1, 0, abcd_sha1,
	    "abcd", 0, "truncated zlib after selected range");
	failures += run_buffered_decoder_case(
	    INNO_COMPRESS_LZMA1, lzma1, sizeof(lzma1) - 1, 0, abcd_sha1,
	    "abcd", 0, "truncated LZMA1 after selected range");
	failures += run_buffered_decoder_case(
	    INNO_COMPRESS_LZMA2, lzma2, sizeof(lzma2) - 1, 0, abcd_sha1,
	    "abcd", 0, "truncated LZMA2 after selected range");
	failures += run_buffered_decoder_case(
	    INNO_COMPRESS_ZLIB, zlib_data, zlib_size - 1, 2048, zzzz_sha1,
	    "ZZZZ", 0, "truncated solid zlib");
	failures += run_buffered_decoder_case(
	    INNO_COMPRESS_LZMA1, lzma1, sizeof(lzma1) - 1, 2048, zzzz_sha1,
	    "ZZZZ", 0, "truncated solid LZMA1");
	failures += run_buffered_decoder_case(
	    INNO_COMPRESS_LZMA2, lzma2, sizeof(lzma2) - 1, 2048, zzzz_sha1,
	    "ZZZZ", 0, "truncated solid LZMA2");

	uint8_t trailing[sizeof(zlib_data) + 1];
	memcpy(trailing, lzma1, sizeof(lzma1));
	trailing[sizeof(lzma1)] = 0;
	failures += run_buffered_decoder_case(
	    INNO_COMPRESS_LZMA1, trailing, sizeof(trailing), 0, abcd_sha1,
	    "abcd", 0, "LZMA1 trailing data");
	memcpy(trailing, lzma2, sizeof(lzma2));
	trailing[sizeof(lzma2)] = 0;
	failures += run_buffered_decoder_case(
	    INNO_COMPRESS_LZMA2, trailing, sizeof(lzma2) + 1, 0, abcd_sha1,
	    "abcd", 0, "LZMA2 trailing data");
	memcpy(trailing, zlib_data, zlib_size);
	trailing[zlib_size] = 0;
	failures += run_buffered_decoder_case(
	    INNO_COMPRESS_ZLIB, trailing, zlib_size + 1, 0, abcd_sha1,
	    "abcd", 0, "zlib trailing data");

	uint8_t malformed[sizeof(lzma1)];
	memcpy(malformed, lzma1, sizeof(lzma1));
	malformed[0] = 0xff;
	failures += run_buffered_decoder_case(
	    INNO_COMPRESS_LZMA1, malformed, sizeof(malformed), 0, abcd_sha1,
	    "abcd", 0, "invalid LZMA1 properties");
	memcpy(malformed, lzma2, sizeof(lzma2));
	malformed[0] = 0xff;
	failures += run_buffered_decoder_case(
	    INNO_COMPRESS_LZMA2, malformed, sizeof(lzma2), 0, abcd_sha1,
	    "abcd", 0, "invalid LZMA2 properties");
	zlib_data[0] ^= 0xff;
	failures += run_buffered_decoder_case(
	    INNO_COMPRESS_ZLIB, zlib_data, zlib_size, 0, abcd_sha1,
	    "abcd", 0, "malformed zlib header");

	z_stream dict_stream;
	uint8_t dictionary_zlib[128];
	memset(&dict_stream, 0, sizeof(dict_stream));
	if (deflateInit(&dict_stream, Z_BEST_SPEED) != Z_OK)
		return 1;
	if (deflateSetDictionary(&dict_stream, (const Bytef *) "dictionary", 10) != Z_OK) {
		deflateEnd(&dict_stream);
		return 1;
	}
	dict_stream.next_in = plain;
	dict_stream.avail_in = sizeof(plain);
	dict_stream.next_out = dictionary_zlib;
	dict_stream.avail_out = sizeof(dictionary_zlib);
	int dict_result = deflate(&dict_stream, Z_FINISH);
	size_t dictionary_zlib_size = sizeof(dictionary_zlib) - dict_stream.avail_out;
	deflateEnd(&dict_stream);
	if (dict_result != Z_STREAM_END)
		return 1;
	failures += run_buffered_decoder_case(
	    INNO_COMPRESS_ZLIB, dictionary_zlib, dictionary_zlib_size,
	    0, abcd_sha1, "abcd", 0, "zlib dictionary request");
	failures += run_buffered_decoder_case(
	    INNO_COMPRESS_LZMA1, lzma1, 5, 0, abcd_sha1,
	    "abcd", 0, "LZMA1 no-progress input");
	failures += run_buffered_decoder_case(
	    INNO_COMPRESS_LZMA2, lzma2, 1, 0, abcd_sha1,
	    "abcd", 0, "LZMA2 no-progress input");
	return failures ? 1 : 0;
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
	inno_file_entry_t entry;
	inno_data_entry_t data;
	init_range_archive(&arc, &entry, &data, fd,
	                   sizeof(magic) + sizeof(stored_payload));
	entry.location = (uint32_t) INT_MAX + 1u;
	int failures =
	    expect_range_failure(&arc, output_path, "high-bit data location");
	entry.location = UINT32_MAX - 1u;
	failures +=
	    expect_range_failure(&arc, output_path, "maximum data location");
	entry.location = UINT32_MAX;
	failures +=
	    expect_range_failure(&arc, output_path, "no-data location");
	entry.location = 0;
	arc.data_entry_count = 0;
	failures +=
	    expect_range_failure(&arc, output_path, "zero data-entry count");
	arc.data_entry_count = 1;
	data.chunk_compressed_size = 4;
	data.file_offset = 1;
	data.file_size = 3;
	memcpy(data.checksum, exact_sha1, sizeof(exact_sha1));
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
	init_range_archive(&arc, &entry, &data, fd, 4 + compressed_size + 1);
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

enum {
	PE_FIXTURE_CAPACITY = 4096,
	PE_FIXTURE_OFFSET = 0x80,
	PE_FIXTURE_RAW_OFFSET = 0x800,
	PE_FIXTURE_RVA = 0x1000,
	PE_FIXTURE_RESOURCE_SIZE = 100,
	PE_FIXTURE_DATA_OFFSET = 88,
	PE_FIXTURE_DATA_SIZE = 12
};

static size_t build_pe_resource_fixture(uint8_t *buffer, int pe32_plus,
                                        uint16_t resource_section)
{
	uint16_t section_count = (uint16_t) (resource_section + 1u);
	uint16_t optional_size = pe32_plus ? 240u : 224u;
	uint32_t directory_offset = pe32_plus ? 112u : 96u;
	size_t optional = PE_FIXTURE_OFFSET + 24u;
	size_t section_table = optional + optional_size;
	size_t section = section_table + (size_t) resource_section * 40u;
	size_t resource = PE_FIXTURE_RAW_OFFSET;

	memset(buffer, 0, PE_FIXTURE_CAPACITY);
	buffer[0] = 'M';
	buffer[1] = 'Z';
	set_u32(buffer, 0x3C, PE_FIXTURE_OFFSET);
	memcpy(buffer + PE_FIXTURE_OFFSET, "PE\0\0", 4);
	set_u16(buffer, PE_FIXTURE_OFFSET + 6u, section_count);
	set_u16(buffer, PE_FIXTURE_OFFSET + 20u, optional_size);
	set_u16(buffer, optional, pe32_plus ? 0x20bu : 0x10bu);
	set_u32(buffer, optional + directory_offset - 4u, 3);
	set_u32(buffer, optional + directory_offset + 16u, PE_FIXTURE_RVA);
	set_u32(buffer, optional + directory_offset + 20u,
	        PE_FIXTURE_RESOURCE_SIZE);
	set_u32(buffer, section + 8u, PE_FIXTURE_RESOURCE_SIZE);
	set_u32(buffer, section + 12u, PE_FIXTURE_RVA);
	set_u32(buffer, section + 16u, PE_FIXTURE_RESOURCE_SIZE);
	set_u32(buffer, section + 20u, PE_FIXTURE_RAW_OFFSET);

	set_u16(buffer, resource + 14u, 1);
	set_u32(buffer, resource + 16u, 10);
	set_u32(buffer, resource + 20u, 0x80000018u);
	set_u16(buffer, resource + 24u + 14u, 1);
	set_u32(buffer, resource + 40u, 11111);
	set_u32(buffer, resource + 44u, 0x80000030u);
	set_u16(buffer, resource + 48u + 14u, 1);
	set_u32(buffer, resource + 64u, 1033);
	set_u32(buffer, resource + 68u, 72);
	set_u32(buffer, resource + 72u,
	        PE_FIXTURE_RVA + PE_FIXTURE_DATA_OFFSET);
	set_u32(buffer, resource + 76u, PE_FIXTURE_DATA_SIZE);
	memcpy(buffer + resource + PE_FIXTURE_DATA_OFFSET,
	       "resource111", PE_FIXTURE_DATA_SIZE);
	return PE_FIXTURE_RAW_OFFSET + PE_FIXTURE_RESOURCE_SIZE;
}

static int check_pe_fixture(const uint8_t *buffer, size_t size,
                            int expect_success, const char *label)
{
	const char *path = "test_gog_fd_pe_resource.tmp";
	FILE *file = fopen(path, "wb");
	if (!file)
		return 1;
	int failed = fwrite(buffer, 1, size, file) != size;
	if (fclose(file) != 0)
		failed = 1;
	int fd = failed ? -1 : OPEN_RB(path);
	if (fd < 0) {
		remove(path);
		return 1;
	}
	uint64_t offset = UINT64_MAX;
	int result = inno_test_find_pe_resource_11111(fd, size, &offset);
	CLOSE_FD(fd);
	remove(path);
	if ((result == 0) != expect_success ||
	    (expect_success &&
	     offset != PE_FIXTURE_RAW_OFFSET + PE_FIXTURE_DATA_OFFSET)) {
		fprintf(stderr, "PE resource boundary case failed: %s\n", label);
		return 1;
	}
	return 0;
}

static int check_pe_resource_bounds(void)
{
	uint8_t fixture[PE_FIXTURE_CAPACITY];
	size_t size = build_pe_resource_fixture(fixture, 0, 0);
	int failures = check_pe_fixture(fixture, size, 1, "valid PE32");

	size = build_pe_resource_fixture(fixture, 1, 0);
	failures += check_pe_fixture(fixture, size, 1, "valid PE32+");
	size = build_pe_resource_fixture(fixture, 0, 16);
	failures += check_pe_fixture(fixture, size, 1,
	                             "resource section after entry 16");

	for (uint32_t resource_size = 0; resource_size < 24; resource_size++) {
		size = build_pe_resource_fixture(fixture, 0, 0);
		set_u32(fixture, PE_FIXTURE_OFFSET + 24u + 96u + 20u,
		        resource_size);
		set_u32(fixture, PE_FIXTURE_OFFSET + 24u + 224u + 8u,
		        resource_size);
		set_u32(fixture, PE_FIXTURE_OFFSET + 24u + 224u + 16u,
		        resource_size);
		failures += check_pe_fixture(
		    fixture, PE_FIXTURE_RAW_OFFSET + resource_size, 0,
		    "zero through 23 byte resource");
	}

	size = build_pe_resource_fixture(fixture, 0, 0);
	set_u16(fixture, PE_FIXTURE_RAW_OFFSET + 12u, UINT16_MAX);
	set_u16(fixture, PE_FIXTURE_RAW_OFFSET + 14u, UINT16_MAX);
	failures += check_pe_fixture(fixture, size, 0,
	                             "overflowing root entry count");

	size = build_pe_resource_fixture(fixture, 0, 0);
	set_u32(fixture, PE_FIXTURE_RAW_OFFSET + 16u, 0x80000063u);
	failures += check_pe_fixture(fixture, size, 0,
	                             "out of range resource name");

	size = build_pe_resource_fixture(fixture, 0, 0);
	set_u32(fixture, PE_FIXTURE_RAW_OFFSET + 20u, 0x80000055u);
	failures += check_pe_fixture(fixture, size, 0,
	                             "truncated nested directory");

	size = build_pe_resource_fixture(fixture, 0, 0);
	set_u32(fixture, PE_FIXTURE_RAW_OFFSET + 68u, 0x80000000u);
	failures += check_pe_fixture(fixture, size, 0,
	                             "cyclic language directory");

	size = build_pe_resource_fixture(fixture, 0, 0);
	set_u32(fixture, PE_FIXTURE_RAW_OFFSET + 68u, 85);
	failures += check_pe_fixture(fixture, size, 0,
	                             "truncated data entry");

	size = build_pe_resource_fixture(fixture, 0, 0);
	set_u32(fixture, PE_FIXTURE_OFFSET + 24u + 96u + 20u, 88);
	failures += check_pe_fixture(fixture, size, 1,
	                             "data entry at directory boundary");

	size = build_pe_resource_fixture(fixture, 0, 0);
	set_u32(fixture, PE_FIXTURE_OFFSET + 24u + 96u + 20u,
	        PE_FIXTURE_RESOURCE_SIZE + 1u);
	failures += check_pe_fixture(fixture, size, 0,
	                             "resource crosses raw section");

	size = build_pe_resource_fixture(fixture, 0, 0);
	set_u32(fixture, PE_FIXTURE_OFFSET + 24u + 96u + 20u,
	        64u * 1024u * 1024u + 1u);
	failures += check_pe_fixture(fixture, size, 0,
	                             "oversized resource declaration");

	size = build_pe_resource_fixture(fixture, 0, 0);
	set_u32(fixture, PE_FIXTURE_RAW_OFFSET + 76u,
	        PE_FIXTURE_DATA_SIZE + 1u);
	failures += check_pe_fixture(fixture, size, 0,
	                             "data one byte beyond section");

	size = build_pe_resource_fixture(fixture, 0, 0);
	set_u32(fixture, PE_FIXTURE_OFFSET + 24u + 224u + 12u,
	        UINT32_MAX - 7u);
	set_u32(fixture, PE_FIXTURE_OFFSET + 24u + 96u + 16u,
	        UINT32_MAX - 3u);
	failures += check_pe_fixture(fixture, size, 0,
	                             "wrapping virtual range");

	size = build_pe_resource_fixture(fixture, 0, 0);
	set_u32(fixture, PE_FIXTURE_OFFSET + 24u + 224u + 20u,
	        UINT32_MAX);
	failures += check_pe_fixture(fixture, size, 0,
	                             "out of range raw section");

	size = build_pe_resource_fixture(fixture, 0, 0);
	set_u16(fixture, PE_FIXTURE_OFFSET + 6u, 97);
	failures += check_pe_fixture(fixture, size, 0,
	                             "unreasonable section count");

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
	if (!inno_output_names_unique(&arc, select_game_file, NULL)) {
		fprintf(stderr, "%s: selected output names collide or are invalid\n",
		        path);
		failures++;
	}
	for (uint32_t i = 0; i < arc.file_count; i++) {
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
	failures += check_version_id_bounds();
	failures += check_unicode_destination_paths();
	failures += check_pe_resource_bounds();
	failures += check_complete_file_catalog();
	failures += check_unsigned_entry_bounds();
	failures += check_checksum_layout_transition();
	failures += check_encrypted_chunk_rejection();
	failures += check_buffered_decoder_failures();
	failures += check_chunk_range_boundaries();
	failures += check_galaxy_checksum_order();
	failures += check_installer(argv[1], d1_expected, 2, 7, 7, "d1");
	failures += check_installer(argv[2], d2_expected, 5, 21, -1, "d2");
	return failures ? 1 : 0;
}
