#include "pkg_reader.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zlib.h>

#ifdef _WIN32
#include <direct.h>
#define TEST_MKDIR(path) _mkdir(path)
#define TEST_RMDIR(path) _rmdir(path)
#else
#include <sys/stat.h>
#include <unistd.h>
#define TEST_MKDIR(path) mkdir((path), 0755)
#define TEST_RMDIR(path) rmdir(path)
#endif

static int failures;
static unsigned int fixture_id;

#define CHECK(condition)                                            \
	do {                                                            \
		if (!(condition)) {                                         \
			fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, \
			        #condition);                                    \
			failures++;                                             \
		}                                                           \
	} while (0)

static void test_xar_toc_bounds(void)
{
	const uint64_t header_size = 28;
	const uint64_t small_compressed = 64;

	CHECK(pkg_test_validate_xar_toc(28, 1, small_compressed, 128,
	                                header_size + small_compressed) == 1);
	CHECK(pkg_test_validate_xar_toc(28, 1, PKG_MAX_TOC_BYTES,
	                                PKG_MAX_TOC_BYTES,
	                                header_size + PKG_MAX_TOC_BYTES) == 1);

	CHECK(pkg_test_validate_xar_toc(27, 1, 64, 128, 92) == 0);
	CHECK(pkg_test_validate_xar_toc(28, 2, 64, 128, 92) == 0);
	CHECK(pkg_test_validate_xar_toc(28, 1, 0, 128, 92) == 0);
	CHECK(pkg_test_validate_xar_toc(28, 1, 64, 0, 92) == 0);
	CHECK(pkg_test_validate_xar_toc(28, 1, 64, 128, 27) == 0);
	CHECK(pkg_test_validate_xar_toc(28, 1, 64, 128, 91) == 0);
	CHECK(pkg_test_validate_xar_toc(28, 1, PKG_MAX_TOC_BYTES + 1, 128,
	                                UINT64_MAX) == 0);
	CHECK(pkg_test_validate_xar_toc(28, 1, 64, PKG_MAX_TOC_BYTES + 1,
	                                UINT64_MAX) == 0);
	CHECK(pkg_test_validate_xar_toc(28, 1, INT_MAX, 128, UINT64_MAX) == 0);
	CHECK(pkg_test_validate_xar_toc(28, 1, UINT32_MAX, UINT32_MAX,
	                                UINT64_MAX) == 0);
	CHECK(pkg_test_validate_xar_toc(28, 1, (uint64_t) SIZE_MAX,
	                                (uint64_t) SIZE_MAX, UINT64_MAX) == 0);
	CHECK(pkg_test_validate_xar_toc(28, 1, UINT64_MAX, UINT64_MAX,
	                                UINT64_MAX) == 0);
}

static void test_xar_toc_decompression(void)
{
	static const uint8_t xml[] = "<xar><toc><file/></toc></xar>";
	uLong compressed_capacity = compressBound(sizeof(xml) - 1);
	uint8_t *compressed = (uint8_t *) malloc((size_t) compressed_capacity + 1);
	uint8_t output[sizeof(xml)];
	uLong compressed_size = compressed_capacity;

	CHECK(compressed != NULL);
	if (!compressed) return;
	CHECK(compress2(compressed, &compressed_size, xml, sizeof(xml) - 1,
	                Z_BEST_SPEED) == Z_OK);

	memset(output, 0, sizeof(output));
	CHECK(pkg_test_decompress_toc(compressed, (size_t) compressed_size, output,
	                              sizeof(xml) - 1) == 0);
	CHECK(memcmp(output, xml, sizeof(xml) - 1) == 0);

	CHECK(pkg_test_decompress_toc(compressed, (size_t) compressed_size, output,
	                              sizeof(xml) - 2) == -1);
	CHECK(pkg_test_decompress_toc(compressed, (size_t) compressed_size, output,
	                              sizeof(xml)) == -1);
	CHECK(pkg_test_decompress_toc(compressed, (size_t) compressed_size - 1,
	                              output, sizeof(xml) - 1) == -1);

	compressed[compressed_size] = 0;
	CHECK(pkg_test_decompress_toc(compressed, (size_t) compressed_size + 1,
	                              output, sizeof(xml) - 1) == -1);

	free(compressed);
}

typedef struct {
	uint8_t *data;
	size_t size;
	size_t capacity;
} test_buffer_t;

static int buffer_append(test_buffer_t *buffer, const void *data, size_t size)
{
	size_t required;
	uint8_t *grown;
	if (size == 0) return 0;
	if (size > SIZE_MAX - buffer->size) return -1;
	required = buffer->size + size;
	if (required > buffer->capacity) {
		size_t capacity = buffer->capacity ? buffer->capacity : 256;
		while (capacity < required) {
			if (capacity > SIZE_MAX / 2) return -1;
			capacity *= 2;
		}
		grown = (uint8_t *) realloc(buffer->data, capacity);
		if (!grown) return -1;
		buffer->data = grown;
		buffer->capacity = capacity;
	}
	memcpy(buffer->data + buffer->size, data, size);
	buffer->size = required;
	return 0;
}

static void write_octal(char *field, size_t width, uint64_t value)
{
	char text[32];
	snprintf(text, sizeof(text), "%0*llo", (int) width,
	         (unsigned long long) value);
	memcpy(field, text, width);
}

static int append_cpio_entry(test_buffer_t *cpio, const char *name,
                             const uint8_t *data, size_t data_size)
{
	char header[76];
	size_t name_size = strlen(name) + 1;
	memset(header, '0', sizeof(header));
	memcpy(header, "070707", 6);
	write_octal(header + 18, 6, 0100644);
	write_octal(header + 59, 6, name_size);
	write_octal(header + 65, 11, data_size);
	return buffer_append(cpio, header, sizeof(header)) < 0 ||
	               buffer_append(cpio, name, name_size) < 0 ||
	               buffer_append(cpio, data, data_size) < 0
	           ? -1
	           : 0;
}

static int gzip_buffer(const uint8_t *input, size_t input_size,
                       test_buffer_t *output)
{
	z_stream stream;
	size_t capacity = compressBound((uLong) input_size) + 32u;
	memset(&stream, 0, sizeof(stream));
	output->data = (uint8_t *) malloc(capacity);
	if (!output->data) return -1;
	output->capacity = capacity;
	stream.next_in = (Bytef *) input;
	stream.avail_in = (uInt) input_size;
	stream.next_out = output->data;
	stream.avail_out = (uInt) capacity;
	if (deflateInit2(&stream, Z_BEST_SPEED, Z_DEFLATED, MAX_WBITS + 16, 8,
	                 Z_DEFAULT_STRATEGY) != Z_OK) {
		free(output->data);
		memset(output, 0, sizeof(*output));
		return -1;
	}
	if (deflate(&stream, Z_FINISH) != Z_STREAM_END) {
		deflateEnd(&stream);
		free(output->data);
		memset(output, 0, sizeof(*output));
		return -1;
	}
	output->size = (size_t) stream.total_out;
	deflateEnd(&stream);
	return 0;
}

static void write_be16(uint8_t *p, uint16_t value)
{
	p[0] = (uint8_t) (value >> 8);
	p[1] = (uint8_t) value;
}

static void write_be32(uint8_t *p, uint32_t value)
{
	p[0] = (uint8_t) (value >> 24);
	p[1] = (uint8_t) (value >> 16);
	p[2] = (uint8_t) (value >> 8);
	p[3] = (uint8_t) value;
}

static void write_be64(uint8_t *p, uint64_t value)
{
	write_be32(p, (uint32_t) (value >> 32));
	write_be32(p + 4, (uint32_t) value);
}

static int write_xar_fixture(const char *path, const char *xml,
                             const uint8_t *heap, size_t heap_size)
{
	uLong toc_capacity = compressBound((uLong) strlen(xml));
	uLong toc_size = toc_capacity;
	uint8_t *toc = (uint8_t *) malloc((size_t) toc_capacity);
	uint8_t header[28] = { 0 };
	FILE *file;
	int writes_ok;
	int close_ok;
	if (!toc) return -1;
	if (compress2(toc, &toc_size, (const Bytef *) xml, (uLong) strlen(xml),
	              Z_BEST_SPEED) != Z_OK) {
		free(toc);
		return -1;
	}
	write_be32(header, 0x78617221u);
	write_be16(header + 4, 28);
	write_be16(header + 6, 1);
	write_be64(header + 8, toc_size);
	write_be64(header + 16, strlen(xml));
	file = fopen(path, "wb");
	if (!file) {
		free(toc);
		return -1;
	}
	writes_ok = fwrite(header, 1, sizeof(header), file) == sizeof(header) &&
	            fwrite(toc, 1, (size_t) toc_size, file) == (size_t) toc_size &&
	            fwrite(heap, 1, heap_size, file) == heap_size;
	close_ok = fclose(file) == 0;
	free(toc);
	if (!writes_ok || !close_ok) {
		remove(path);
		return -1;
	}
	return 0;
}

static int run_xar_fixture(const char *xml, const uint8_t *heap,
                           size_t heap_size)
{
	char path[64];
	pkg_archive_t archive;
	int result;
	snprintf(path, sizeof(path), "test_pkg_scripts_%u.pkg", fixture_id++);
	if (write_xar_fixture(path, xml, heap, heap_size) < 0)
		return -2;
	result = pkg_open(path, &archive);
	if (result >= 0) pkg_close(&archive);
	remove(path);
	return result;
}

static int run_xar_extraction(const char *xml, const uint8_t *heap,
                              size_t heap_size)
{
	char path[64];
	char output_dir[64];
	char output_path[96];
	pkg_archive_t archive;
	FILE *output;
	int opened;
	int result = -2;
	int value;
	unsigned int id = fixture_id++;
	snprintf(path, sizeof(path), "test_pkg_extract_%u.pkg", id);
	snprintf(output_dir, sizeof(output_dir), "test_pkg_extract_%u", id);
	snprintf(output_path, sizeof(output_path), "%s/DESCENT.HOG", output_dir);
	if (write_xar_fixture(path, xml, heap, heap_size) < 0 ||
	    TEST_MKDIR(output_dir) != 0)
		goto cleanup;
	opened = pkg_open(path, &archive);
	if (opened != 1) {
		if (opened >= 0) pkg_close(&archive);
		goto cleanup;
	}
	result = pkg_extract_all(&archive, output_dir, NULL, NULL, 0);
	pkg_close(&archive);
	if (result == 1) {
		output = fopen(output_path, "rb");
		if (!output) {
			result = -2;
		} else {
			value = fgetc(output);
			if (value != 0x42 || fgetc(output) != EOF)
				result = -2;
			fclose(output);
		}
	}

cleanup:
	remove(output_path);
	TEST_RMDIR(output_dir);
	remove(path);
	return result;
}

static void scripts_xml(char *xml, size_t capacity, const char *offset,
                        const char *size, const char *length,
                        const char *encoding)
{
	snprintf(xml, capacity,
	         "<xar><toc><file id=\"1\"><type>directory</type>"
	         "<name>package.pkg</name><file id=\"2\"><data>"
	         "<encoding style=\"%s\"/><offset>%s</offset><size>%s</size>"
	         "<length>%s</length></data><type>file</type>"
	         "<name>Scripts</name></file></file></toc></xar>",
	         encoding, offset, size, length);
}

static void test_scripts_member_completion(void)
{
	static const uint8_t payload[] = { 0x42 };
	test_buffer_t entry = { 0 };
	test_buffer_t complete = { 0 };
	test_buffer_t duplicate = { 0 };
	test_buffer_t padded = { 0 };
	test_buffer_t excessive_padding = { 0 };
	test_buffer_t entry_gzip = { 0 };
	test_buffer_t complete_gzip = { 0 };
	test_buffer_t duplicate_gzip = { 0 };
	test_buffer_t padded_gzip = { 0 };
	test_buffer_t excessive_padding_gzip = { 0 };
	test_buffer_t combined = { 0 };
	test_buffer_t offset_heap = { 0 };
	char xml[1024];
	char size_text[32];
	char short_text[32];
	char long_text[32];
	uint8_t zero_padding[512] = { 0 };
	uint8_t *corrupt;

	CHECK(append_cpio_entry(&entry,
	                        "./payload/Contents/Resources/game/DESCENT.HOG",
	                        payload, sizeof(payload)) == 0);
	CHECK(buffer_append(&complete, entry.data, entry.size) == 0);
	CHECK(append_cpio_entry(&complete, "TRAILER!!!", NULL, 0) == 0);
	CHECK(buffer_append(&duplicate, complete.data, complete.size) == 0);
	CHECK(append_cpio_entry(&duplicate, "TRAILER!!!", NULL, 0) == 0);
	CHECK(buffer_append(&padded, complete.data, complete.size) == 0);
	CHECK(buffer_append(&padded, zero_padding, 7) == 0);
	CHECK(buffer_append(&excessive_padding, complete.data, complete.size) == 0);
	CHECK(buffer_append(&excessive_padding, zero_padding,
	                    sizeof(zero_padding)) == 0);
	CHECK(gzip_buffer(entry.data, entry.size, &entry_gzip) == 0);
	CHECK(gzip_buffer(complete.data, complete.size, &complete_gzip) == 0);
	CHECK(gzip_buffer(duplicate.data, duplicate.size, &duplicate_gzip) == 0);
	CHECK(gzip_buffer(padded.data, padded.size, &padded_gzip) == 0);
	CHECK(gzip_buffer(excessive_padding.data, excessive_padding.size,
	                  &excessive_padding_gzip) == 0);
	if (!complete_gzip.data || !entry_gzip.data || !duplicate_gzip.data ||
	    !padded_gzip.data || !excessive_padding_gzip.data)
		goto cleanup;

	snprintf(size_text, sizeof(size_text), "%llu",
	         (unsigned long long) complete_gzip.size);
	snprintf(short_text, sizeof(short_text), "%llu",
	         (unsigned long long) (complete_gzip.size - 1));
	snprintf(long_text, sizeof(long_text), "%llu",
	         (unsigned long long) (complete_gzip.size + 1));
	scripts_xml(xml, sizeof(xml), "0", size_text, size_text,
	            "application/octet-stream");
	CHECK(run_xar_fixture(xml, complete_gzip.data, complete_gzip.size) == 1);
	CHECK(run_xar_extraction(xml, complete_gzip.data, complete_gzip.size) == 1);
	snprintf(size_text, sizeof(size_text), "%llu",
	         (unsigned long long) padded_gzip.size);
	scripts_xml(xml, sizeof(xml), "0", size_text, size_text,
	            "application/octet-stream");
	CHECK(run_xar_fixture(xml, padded_gzip.data, padded_gzip.size) == 1);
	snprintf(size_text, sizeof(size_text), "%llu",
	         (unsigned long long) excessive_padding_gzip.size);
	scripts_xml(xml, sizeof(xml), "0", size_text, size_text,
	            "application/octet-stream");
	CHECK(run_xar_fixture(xml, excessive_padding_gzip.data,
	                      excessive_padding_gzip.size) == -1);

	snprintf(size_text, sizeof(size_text), "%llu",
	         (unsigned long long) complete_gzip.size);
	CHECK(buffer_append(&offset_heap, "pad", 3) == 0);
	CHECK(buffer_append(&offset_heap, complete_gzip.data, complete_gzip.size) == 0);
	scripts_xml(xml, sizeof(xml), "3", size_text, size_text,
	            "application/octet-stream");
	CHECK(run_xar_fixture(xml, offset_heap.data, offset_heap.size) == 1);

	scripts_xml(xml, sizeof(xml), "0", short_text, short_text,
	            "application/octet-stream");
	CHECK(run_xar_fixture(xml, complete_gzip.data, complete_gzip.size) == -1);

	scripts_xml(xml, sizeof(xml), "0", long_text, long_text,
	            "application/octet-stream");
	CHECK(run_xar_fixture(xml, complete_gzip.data, complete_gzip.size) == -1);

	scripts_xml(xml, sizeof(xml), "0", size_text, size_text,
	            "application/x-gzip");
	CHECK(run_xar_fixture(xml, complete_gzip.data, complete_gzip.size) == -1);
	scripts_xml(xml, sizeof(xml), "12junk", size_text, size_text,
	            "application/octet-stream");
	CHECK(run_xar_fixture(xml, complete_gzip.data, complete_gzip.size) == -1);
	scripts_xml(xml, sizeof(xml), "18446744073709551616", size_text, size_text,
	            "application/octet-stream");
	CHECK(run_xar_fixture(xml, complete_gzip.data, complete_gzip.size) == -1);
	scripts_xml(xml, sizeof(xml), "18446744073709551615", size_text, size_text,
	            "application/octet-stream");
	CHECK(run_xar_fixture(xml, complete_gzip.data, complete_gzip.size) == -1);
	scripts_xml(xml, sizeof(xml), "0", "1", size_text,
	            "application/octet-stream");
	CHECK(run_xar_fixture(xml, complete_gzip.data, complete_gzip.size) == -1);

	snprintf(xml, sizeof(xml),
	         "<xar><toc><file><name>package.pkg</name></file>"
	         "<file><data><encoding style=\"application/octet-stream\"/>"
	         "<offset>0</offset><size>%s</size><length>%s</length></data>"
	         "<name>Scripts</name></file></toc></xar>",
	         size_text, size_text);
	CHECK(run_xar_fixture(xml, complete_gzip.data, complete_gzip.size) == -1);

	snprintf(size_text, sizeof(size_text), "%llu",
	         (unsigned long long) entry_gzip.size);
	scripts_xml(xml, sizeof(xml), "0", size_text, size_text,
	            "application/octet-stream");
	CHECK(run_xar_fixture(xml, entry_gzip.data, entry_gzip.size) == -1);

	snprintf(size_text, sizeof(size_text), "%llu",
	         (unsigned long long) duplicate_gzip.size);
	scripts_xml(xml, sizeof(xml), "0", size_text, size_text,
	            "application/octet-stream");
	CHECK(run_xar_fixture(xml, duplicate_gzip.data, duplicate_gzip.size) == -1);

	corrupt = (uint8_t *) malloc(complete_gzip.size);
	CHECK(corrupt != NULL);
	if (corrupt) {
		memcpy(corrupt, complete_gzip.data, complete_gzip.size);
		corrupt[complete_gzip.size - 8] ^= 1;
		snprintf(size_text, sizeof(size_text), "%llu",
		         (unsigned long long) complete_gzip.size);
		scripts_xml(xml, sizeof(xml), "0", size_text, size_text,
		            "application/octet-stream");
		CHECK(run_xar_fixture(xml, corrupt, complete_gzip.size) == -1);
		free(corrupt);
	}

	CHECK(buffer_append(&combined, complete_gzip.data, complete_gzip.size) == 0);
	CHECK(buffer_append(&combined, complete_gzip.data, complete_gzip.size) == 0);
	snprintf(size_text, sizeof(size_text), "%llu",
	         (unsigned long long) combined.size);
	scripts_xml(xml, sizeof(xml), "0", size_text, size_text,
	            "application/octet-stream");
	CHECK(run_xar_fixture(xml, combined.data, combined.size) == -1);

	CHECK(buffer_append(&complete_gzip, "X", 1) == 0);
	snprintf(size_text, sizeof(size_text), "%llu",
	         (unsigned long long) complete_gzip.size);
	scripts_xml(xml, sizeof(xml), "0", size_text, size_text,
	            "application/octet-stream");
	CHECK(run_xar_fixture(xml, complete_gzip.data, complete_gzip.size) == -1);

cleanup:
	free(entry.data);
	free(complete.data);
	free(duplicate.data);
	free(padded.data);
	free(excessive_padding.data);
	free(entry_gzip.data);
	free(complete_gzip.data);
	free(duplicate_gzip.data);
	free(padded_gzip.data);
	free(excessive_padding_gzip.data);
	free(combined.data);
	free(offset_heap.data);
}

int main(void)
{
	test_xar_toc_bounds();
	test_xar_toc_decompression();
	test_scripts_member_completion();
	if (failures) {
		fprintf(stderr, "%d PKG reader test(s) failed\n", failures);
		return 1;
	}
	puts("PKG reader tests passed");
	return 0;
}
