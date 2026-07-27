#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#define make_dir(path)   _mkdir(path)
#define remove_dir(path) _rmdir(path)
#define PATH_SEP_CHAR    '\\'
#else
#include <unistd.h>
#define make_dir(path)   mkdir(path, 0755)
#define remove_dir(path) rmdir(path)
#define PATH_SEP_CHAR    '/'
#endif

#include "sow_extract.h"

#define FIXTURE_CAPACITY 512u

typedef struct {
	unsigned char data[FIXTURE_CAPACITY];
	size_t size;
	size_t basic_header_offset;
	size_t extended_header_offset;
	size_t payload_offset;
} fixture_t;

static void put_u16(unsigned char *p, unsigned int value)
{
	p[0] = (unsigned char) value;
	p[1] = (unsigned char) (value >> 8);
}

static void put_u32(unsigned char *p, unsigned int value)
{
	p[0] = (unsigned char) value;
	p[1] = (unsigned char) (value >> 8);
	p[2] = (unsigned char) (value >> 16);
	p[3] = (unsigned char) (value >> 24);
}

static unsigned int crc32_bytes(const unsigned char *data, size_t len)
{
	unsigned int crc = 0xffffffffu;
	while (len-- > 0) {
		crc ^= *data++;
		for (int bit = 0; bit < 8; bit++)
			crc = (crc & 1u) ? (crc >> 1) ^ 0xedb88320u : crc >> 1;
	}
	return ~crc;
}

static void put_bits(unsigned char *data, unsigned int *bitpos,
                     unsigned int value, unsigned int count)
{
	for (unsigned int i = 0; i < count; i++) {
		unsigned int shift = count - i - 1u;
		if (value & (1u << shift))
			data[*bitpos / 8u] |= (unsigned char) (1u << (7u - *bitpos % 8u));
		(*bitpos)++;
	}
}

static size_t make_compressed_literal(unsigned char *data, unsigned char literal)
{
	unsigned int bitpos = 0;
	memset(data, 0, 8);
	put_bits(data, &bitpos, 1, 16);      /* one symbol in the block */
	put_bits(data, &bitpos, 0, 5);       /* degenerate code-length tree */
	put_bits(data, &bitpos, 0, 5);       /* code-length symbol zero */
	put_bits(data, &bitpos, 0, 9);       /* degenerate literal tree */
	put_bits(data, &bitpos, literal, 9); /* selected literal */
	put_bits(data, &bitpos, 0, 5);       /* degenerate position tree */
	put_bits(data, &bitpos, 0, 5);       /* position symbol zero */
	return (bitpos + 7u) / 8u;
}

static fixture_t make_fixture(int method, const unsigned char *payload,
                              size_t payload_size,
                              const unsigned char *expected,
                              size_t expected_size, int with_extended_header)
{
	static const char filename[] = "crc_test.hog";
	static const unsigned char extended[] = { 0x12, 0x34, 0x56 };
	fixture_t fixture;
	unsigned char header[64];
	size_t header_size = 30u + sizeof(filename) + 1u;
	size_t pos = 0;

	memset(&fixture, 0, sizeof(fixture));
	memset(header, 0, sizeof(header));
	header[0] = 30;
	header[5] = (unsigned char) method;
	header[6] = 0;
	put_u32(header + 12, (unsigned int) payload_size);
	put_u32(header + 16, (unsigned int) expected_size);
	put_u32(header + 20, crc32_bytes(expected, expected_size));
	memcpy(header + 30, filename, sizeof(filename));

	fixture.data[pos++] = 0x60;
	fixture.data[pos++] = 0xea;
	put_u16(fixture.data + pos, (unsigned int) header_size);
	pos += 2;
	fixture.basic_header_offset = pos;
	memcpy(fixture.data + pos, header, header_size);
	pos += header_size;
	put_u32(fixture.data + pos, crc32_bytes(header, header_size));
	pos += 4;
	if (with_extended_header) {
		put_u16(fixture.data + pos, sizeof(extended));
		pos += 2;
		fixture.extended_header_offset = pos;
		memcpy(fixture.data + pos, extended, sizeof(extended));
		pos += sizeof(extended);
		put_u32(fixture.data + pos, crc32_bytes(extended, sizeof(extended)));
		pos += 4;
	}
	put_u16(fixture.data + pos, 0);
	pos += 2;
	fixture.payload_offset = pos;
	memcpy(fixture.data + pos, payload, payload_size);
	pos += payload_size;
	fixture.data[pos++] = 0x60;
	fixture.data[pos++] = 0xea;
	put_u16(fixture.data + pos, 0);
	pos += 2;
	fixture.size = pos;
	return fixture;
}

static void refresh_basic_header_crc(fixture_t *fixture)
{
	unsigned int header_size = fixture->data[2] |
	                           ((unsigned int) fixture->data[3] << 8);
	put_u32(fixture->data + fixture->basic_header_offset + header_size,
	        crc32_bytes(fixture->data + fixture->basic_header_offset,
	                    header_size));
}

static void resize_basic_header(fixture_t *fixture, unsigned int header_size)
{
	put_u16(fixture->data + 2, header_size);
	refresh_basic_header_crc(fixture);
	put_u16(fixture->data + fixture->basic_header_offset + header_size + 4u, 0);
	fixture->size = fixture->basic_header_offset + header_size + 6u;
}

static int write_fixture(const char *path, const fixture_t *fixture)
{
	FILE *fp = fopen(path, "wb");
	if (!fp) return -1;
	int ok = fwrite(fixture->data, 1, fixture->size, fp) == fixture->size;
	if (fclose(fp) != 0) ok = 0;
	return ok ? 0 : -1;
}

static int file_matches(const char *path, const unsigned char *expected, size_t expected_size)
{
	unsigned char actual[32];
	FILE *fp = fopen(path, "rb");
	if (!fp) return 0;
	size_t size = fread(actual, 1, sizeof(actual), fp);
	int extra = fgetc(fp);
	fclose(fp);
	return size == expected_size && extra == EOF &&
	       memcmp(actual, expected, expected_size) == 0;
}

static int file_exists(const char *path)
{
	FILE *fp = fopen(path, "rb");
	if (!fp) return 0;
	fclose(fp);
	return 1;
}

static int cancel_progress(const char *filename, long long done, long long total,
                           void *user_data)
{
	(void) filename;
	(void) done;
	(void) total;
	(*(int *) user_data)++;
	return 1;
}

static int run_case(const char *name, const fixture_t *fixture,
                    const unsigned char *expected, size_t expected_size,
                    int should_succeed)
{
	static const char *extensions[] = { "hog", NULL };
	char archive_path[SOW_PATH_LEN];
	char output_path[SOW_PATH_LEN];
	int result;

	snprintf(archive_path, sizeof(archive_path), "sow_integrity_temp%c%s.sow",
	         PATH_SEP_CHAR, name);
	snprintf(output_path, sizeof(output_path),
	         "sow_integrity_temp%coutput%ccrc_test.hog",
	         PATH_SEP_CHAR, PATH_SEP_CHAR);
	remove(output_path);
	if (write_fixture(archive_path, fixture) < 0) {
		fprintf(stderr, "failed to write %s fixture\n", name);
		return 1;
	}
	result = sow_extract(archive_path, "sow_integrity_temp/output",
	                     extensions, NULL, NULL);
	remove(archive_path);
	if (should_succeed) {
		if (result != 1 || !file_matches(output_path, expected, expected_size)) {
			fprintf(stderr, "%s fixture did not extract expected bytes\n", name);
			return 1;
		}
		remove(output_path);
		return 0;
	}
	if (result > 0 || file_exists(output_path)) {
		fprintf(stderr, "%s corruption produced an accepted output\n", name);
		remove(output_path);
		return 1;
	}
	return 0;
}

static int run_malformed_case(const char *name, const fixture_t *fixture)
{
	static const char *extensions[] = { "hog", NULL };
	char archive_path[SOW_PATH_LEN];
	char output_path[SOW_PATH_LEN];
	int result;

	snprintf(archive_path, sizeof(archive_path), "sow_integrity_temp%c%s.sow",
	         PATH_SEP_CHAR, name);
	snprintf(output_path, sizeof(output_path),
	         "sow_integrity_temp%coutput%ccrc_test.hog",
	         PATH_SEP_CHAR, PATH_SEP_CHAR);
	remove(output_path);
	if (write_fixture(archive_path, fixture) < 0) {
		fprintf(stderr, "failed to write %s fixture\n", name);
		return 1;
	}
	result = sow_extract(archive_path, "sow_integrity_temp/output",
	                     extensions, NULL, NULL);
	remove(archive_path);
	if (result >= 0 || file_exists(output_path)) {
		fprintf(stderr, "%s malformed header was not reported as an error\n", name);
		remove(output_path);
		return 1;
	}
	return 0;
}

int main(void)
{
	static const unsigned char stored[] = "stored payload";
	static const unsigned char literal_a[] = { 'A' };
	unsigned char compressed_a[8];
	unsigned char compressed_b[8];
	size_t compressed_size = make_compressed_literal(compressed_a, 'A');
	make_compressed_literal(compressed_b, 'B');
	int failures = 0;

	make_dir("sow_integrity_temp");
	make_dir("sow_integrity_temp/output");
	make_dir("sow_integrity_temp/scan");
	{
		sow_file_list_t list;
		if (sow_scan_dir("sow_integrity_temp/scan", &list) != 0 ||
		    sow_scan_dir(NULL, &list) != -1 ||
		    sow_scan_dir("sow_integrity_temp/scan", NULL) != -1) {
			fprintf(stderr, "SOW empty-scan or argument validation failed\n");
			failures++;
		}
	}

	fixture_t fixture = make_fixture(0, stored, sizeof(stored) - 1u,
	                                 stored, sizeof(stored) - 1u, 1);
	if (write_fixture("sow_integrity_temp/scan/one.SoW", &fixture) < 0) {
		fprintf(stderr, "failed to write SOW scan fixture\n");
		failures++;
	} else {
		sow_file_list_t list;
		if (sow_scan_dir("sow_integrity_temp/scan", &list) != 1 || list.count != 1 ||
		    strstr(list.paths[0], "one.SoW") == NULL) {
			fprintf(stderr, "SOW one-file scan failed\n");
			failures++;
		}
	}
	failures += run_case("valid_stored", &fixture, stored, sizeof(stored) - 1u, 1);
	fixture.data[fixture.basic_header_offset + 8u] ^= 1u;
	failures += run_case("bad_basic_header", &fixture, stored,
	                     sizeof(stored) - 1u, 0);

	for (unsigned int header_size = 1; header_size < 30; header_size++) {
		char name[32];
		fixture = make_fixture(0, stored, sizeof(stored) - 1u,
		                       stored, sizeof(stored) - 1u, 0);
		put_u16(fixture.data + 2, header_size);
		snprintf(name, sizeof(name), "basic_size_%u", header_size);
		failures += run_malformed_case(name, &fixture);
	}

	fixture = make_fixture(0, stored, sizeof(stored) - 1u,
	                       stored, sizeof(stored) - 1u, 0);
	fixture.size = 3;
	failures += run_malformed_case("truncated_basic_size", &fixture);

	fixture = make_fixture(0, stored, sizeof(stored) - 1u,
	                       stored, sizeof(stored) - 1u, 0);
	resize_basic_header(&fixture, 30);
	failures += run_malformed_case("basic_size_30_no_strings", &fixture);

	fixture = make_fixture(0, stored, sizeof(stored) - 1u,
	                       stored, sizeof(stored) - 1u, 0);
	fixture.data[fixture.basic_header_offset] = 29;
	refresh_basic_header_crc(&fixture);
	failures += run_malformed_case("first_header_too_small", &fixture);

	fixture = make_fixture(0, stored, sizeof(stored) - 1u,
	                       stored, sizeof(stored) - 1u, 0);
	fixture.data[fixture.basic_header_offset] =
	    (unsigned char) (fixture.data[2] + 1u);
	refresh_basic_header_crc(&fixture);
	failures += run_malformed_case("first_header_beyond_basic", &fixture);

	fixture = make_fixture(0, stored, sizeof(stored) - 1u,
	                       stored, sizeof(stored) - 1u, 0);
	memset(fixture.data + fixture.basic_header_offset + 30u, 'A',
	       fixture.data[2] - 30u);
	refresh_basic_header_crc(&fixture);
	failures += run_malformed_case("unterminated_filename", &fixture);

	fixture = make_fixture(0, stored, sizeof(stored) - 1u,
	                       stored, sizeof(stored) - 1u, 0);
	fixture.data[fixture.basic_header_offset + 30u] = 'A';
	fixture.data[fixture.basic_header_offset + 31u] = 0;
	memset(fixture.data + fixture.basic_header_offset + 32u, 'B',
	       fixture.data[2] - 32u);
	refresh_basic_header_crc(&fixture);
	failures += run_malformed_case("unterminated_comment", &fixture);

	fixture = make_fixture(0, stored, sizeof(stored) - 1u,
	                       stored, sizeof(stored) - 1u, 0);
	resize_basic_header(&fixture, 292);
	memset(fixture.data + fixture.basic_header_offset + 30u, 'A', 260);
	fixture.data[fixture.basic_header_offset + 290u] = 0;
	fixture.data[fixture.basic_header_offset + 291u] = 0;
	refresh_basic_header_crc(&fixture);
	failures += run_malformed_case("overlong_filename", &fixture);

	fixture = make_fixture(0, stored, sizeof(stored) - 1u,
	                       stored, sizeof(stored) - 1u, 0);
	resize_basic_header(&fixture, 292);
	fixture.data[fixture.basic_header_offset + 30u] = 0;
	memset(fixture.data + fixture.basic_header_offset + 31u, 'B', 260);
	fixture.data[fixture.basic_header_offset + 291u] = 0;
	refresh_basic_header_crc(&fixture);
	failures += run_malformed_case("overlong_comment_name", &fixture);

	fixture = make_fixture(0, stored, sizeof(stored) - 1u,
	                       stored, sizeof(stored) - 1u, 0);
	fixture.size = fixture.basic_header_offset + fixture.data[2] + 3u;
	failures += run_malformed_case("truncated_basic_crc", &fixture);

	fixture = make_fixture(0, stored, sizeof(stored) - 1u,
	                       stored, sizeof(stored) - 1u, 1);
	fixture.data[fixture.extended_header_offset] ^= 1u;
	failures += run_case("bad_extended_header", &fixture, stored,
	                     sizeof(stored) - 1u, 0);

	fixture = make_fixture(0, stored, sizeof(stored) - 1u,
	                       stored, sizeof(stored) - 1u, 1);
	fixture.size = fixture.extended_header_offset + 3u;
	failures += run_malformed_case("truncated_extended_span", &fixture);

	fixture = make_fixture(0, stored, sizeof(stored) - 1u,
	                       stored, sizeof(stored) - 1u, 1);
	fixture.size = fixture.extended_header_offset + 3u + 4u;
	failures += run_malformed_case("missing_extended_terminator", &fixture);

	fixture = make_fixture(0, stored, sizeof(stored) - 1u,
	                       stored, sizeof(stored) - 1u, 0);
	fixture.size = fixture.payload_offset + sizeof(stored) - 1u;
	failures += run_case("payload_exact_file_end", &fixture, stored,
	                     sizeof(stored) - 1u, 1);

	fixture = make_fixture(0, stored, sizeof(stored) - 1u,
	                       stored, sizeof(stored) - 1u, 0);
	fixture.size = fixture.payload_offset + sizeof(stored) - 1u;
	put_u32(fixture.data + fixture.basic_header_offset + 12u,
	        sizeof(stored));
	put_u32(fixture.data + fixture.basic_header_offset + 16u,
	        sizeof(stored));
	refresh_basic_header_crc(&fixture);
	failures += run_malformed_case("payload_one_beyond_file", &fixture);

	fixture = make_fixture(0, stored, sizeof(stored) - 1u,
	                       stored, sizeof(stored) - 1u, 0);
	fixture.data[fixture.payload_offset] ^= 1u;
	failures += run_case("bad_stored_payload", &fixture, stored,
	                     sizeof(stored) - 1u, 0);

	fixture = make_fixture(1, compressed_a, compressed_size,
	                       literal_a, sizeof(literal_a), 0);
	failures += run_case("valid_compressed", &fixture, literal_a,
	                     sizeof(literal_a), 1);

	fixture = make_fixture(1, compressed_b, compressed_size,
	                       literal_a, sizeof(literal_a), 0);
	failures += run_case("bad_compressed_payload", &fixture, literal_a,
	                     sizeof(literal_a), 0);

	fixture = make_fixture(1, compressed_a, compressed_size - 1u,
	                       literal_a, sizeof(literal_a), 0);
	failures += run_case("truncated_compressed", &fixture, literal_a,
	                     sizeof(literal_a), 0);

	fixture = make_fixture(0, stored, sizeof(stored) - 1u,
	                       stored, sizeof(stored) - 1u, 0);
	if (write_fixture("sow_integrity_temp/cancel.sow", &fixture) < 0) {
		fprintf(stderr, "failed to write SOW cancellation fixture\n");
		failures++;
	} else {
		static const char *extensions[] = { "hog", NULL };
		int callbacks = 0;
		remove("sow_integrity_temp/output/crc_test.hog");
		if (sow_extract("sow_integrity_temp/cancel.sow", "sow_integrity_temp/output",
		                extensions, cancel_progress, &callbacks) != 0 ||
		    callbacks != 1 ||
		    file_exists("sow_integrity_temp/output/crc_test.hog")) {
			fprintf(stderr, "SOW cancellation did not stop before output\n");
			failures++;
		}
		remove("sow_integrity_temp/cancel.sow");
	}

	remove("sow_integrity_temp/scan/one.SoW");
	remove_dir("sow_integrity_temp/scan");
	remove_dir("sow_integrity_temp/output");
	remove_dir("sow_integrity_temp");
	if (failures != 0) return 1;
	printf("SOW ARJ integrity tests passed\n");
	return 0;
}
