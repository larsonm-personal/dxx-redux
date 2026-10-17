#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "iff_safe_decode.h"

#define CHECK(condition)                                                 \
	do {                                                                 \
		if (!(condition)) {                                              \
			fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); \
			return 1;                                                    \
		}                                                                \
	} while (0)

typedef struct test_buffer {
	uint8_t data[4096];
	size_t size;
} test_buffer;

static void put_be16(uint8_t *data, uint16_t value)
{
	data[0] = (uint8_t) (value >> 8);
	data[1] = (uint8_t) value;
}

static void put_be32(uint8_t *data, uint32_t value)
{
	data[0] = (uint8_t) (value >> 24);
	data[1] = (uint8_t) (value >> 16);
	data[2] = (uint8_t) (value >> 8);
	data[3] = (uint8_t) value;
}

static void append(test_buffer *buffer, const void *data, size_t size)
{
	if (size)
		memcpy(buffer->data + buffer->size, data, size);
	buffer->size += size;
}

static size_t begin_form(test_buffer *buffer, const char *type)
{
	const size_t start = buffer->size;
	static const uint8_t zero[4] = { 0 };
	append(buffer, "FORM", 4);
	append(buffer, zero, 4);
	append(buffer, type, 4);
	return start;
}

static void finish_form(test_buffer *buffer, size_t start)
{
	put_be32(buffer->data + start + 4, (uint32_t) (buffer->size - start - 8));
}

static void append_chunk(test_buffer *buffer, const char *type, const uint8_t *data, size_t size)
{
	static const uint8_t zero = 0;
	append(buffer, type, 4);
	put_be32(buffer->data + buffer->size, (uint32_t) size);
	buffer->size += 4;
	append(buffer, data, size);
	if (size & 1)
		append(buffer, &zero, 1);
}

static void append_bmhd(test_buffer *buffer, unsigned width, unsigned height, unsigned planes,
                        unsigned masking, unsigned compression)
{
	uint8_t header[20] = { 0 };
	put_be16(header, (uint16_t) width);
	put_be16(header + 2, (uint16_t) height);
	header[8] = (uint8_t) planes;
	header[9] = (uint8_t) masking;
	header[10] = (uint8_t) compression;
	append_chunk(buffer, "BMHD", header, sizeof(header));
}

static test_buffer make_pbm(unsigned width, unsigned height, unsigned compression, const uint8_t *body,
                            size_t body_size)
{
	test_buffer buffer = { { 0 }, 0 };
	const size_t form = begin_form(&buffer, "PBM ");
	append_bmhd(&buffer, width, height, 8, 0, compression);
	append_chunk(&buffer, "BODY", body, body_size);
	finish_form(&buffer, form);
	return buffer;
}

static test_buffer make_delta_anim(uint8_t delta_code)
{
	const uint8_t first_body[] = { 1, 2 };
	const uint8_t delta[] = { 0, 0, 0, 0, 1, delta_code, 2, 9 };
	test_buffer first = make_pbm(2, 1, 0, first_body, sizeof(first_body));
	test_buffer second = { { 0 }, 0 };
	test_buffer anim = { { 0 }, 0 };
	size_t form = begin_form(&second, "PBM ");
	append_chunk(&second, "ANHD", NULL, 0);
	append_chunk(&second, "DLTA", delta, sizeof(delta));
	finish_form(&second, form);
	form = begin_form(&anim, "ANIM");
	append(&anim, first.data, first.size);
	append(&anim, second.data, second.size);
	finish_form(&anim, form);
	return anim;
}

int main(void)
{
	iff_safe_image image;
	iff_safe_image frames[2] = { 0 };
	size_t frame_count;
	const uint8_t raw_body[] = { 4, 5, 6, 0 };
	const uint8_t rle_body[] = { 0xfd, 7 };
	test_buffer buffer = make_pbm(3, 1, 0, raw_body, sizeof(raw_body));
	CHECK(iff_safe_decode_bitmap(buffer.data, buffer.size, &image) == IFF_SAFE_OK);
	CHECK(image.width == 3 && image.height == 1 && image.pixels[0] == 4 && image.pixels[2] == 6);
	iff_safe_free_image(&image);

	buffer = make_pbm(4, 1, 1, rle_body, sizeof(rle_body));
	CHECK(iff_safe_decode_bitmap(buffer.data, buffer.size, &image) == IFF_SAFE_OK);
	CHECK(image.pixels[0] == 7 && image.pixels[3] == 7);
	iff_safe_free_image(&image);

	buffer = (test_buffer) { { 0 }, 0 };
	{
		const uint8_t planar_body[] = { 0x80, 0 };
		const size_t form = begin_form(&buffer, "ILBM");
		append_bmhd(&buffer, 2, 1, 1, 0, 0);
		append_chunk(&buffer, "BODY", planar_body, sizeof(planar_body));
		finish_form(&buffer, form);
	}
	CHECK(iff_safe_decode_bitmap(buffer.data, buffer.size, &image) == IFF_SAFE_OK);
	CHECK(image.pixels[0] == 1 && image.pixels[1] == 0);
	iff_safe_free_image(&image);

	buffer = make_pbm(4, 1, 1, (const uint8_t[]) { 4, 1, 2, 3, 4, 5 }, 6);
	CHECK(iff_safe_decode_bitmap(buffer.data, buffer.size, &image) == IFF_SAFE_CORRUPT);
	buffer = make_pbm(4, 1, 1, (const uint8_t[]) { 0xfb, 3 }, 2);
	CHECK(iff_safe_decode_bitmap(buffer.data, buffer.size, &image) == IFF_SAFE_CORRUPT);
	buffer = make_pbm(0, 1, 0, raw_body, sizeof(raw_body));
	CHECK(iff_safe_decode_bitmap(buffer.data, buffer.size, &image) == IFF_SAFE_CORRUPT);
	buffer = make_pbm(8193, 1, 0, raw_body, sizeof(raw_body));
	CHECK(iff_safe_decode_bitmap(buffer.data, buffer.size, &image) == IFF_SAFE_CORRUPT);

	buffer = make_pbm(3, 1, 0, raw_body, sizeof(raw_body));
	CHECK(iff_safe_decode_bitmap(buffer.data, buffer.size - 1, &image) == IFF_SAFE_CORRUPT);
	put_be32(buffer.data + 44, 0xffffffffu);
	CHECK(iff_safe_decode_bitmap(buffer.data, buffer.size, &image) == IFF_SAFE_CORRUPT);

	buffer = make_pbm(2, 1, 0, raw_body, 2);
	append_chunk(&buffer, "CMAP", (const uint8_t[771]) { 0 }, 771);
	finish_form(&buffer, 0);
	CHECK(iff_safe_decode_bitmap(buffer.data, buffer.size, &image) == IFF_SAFE_CORRUPT);

	buffer = make_delta_anim(0);
	CHECK(iff_safe_decode_anim(buffer.data, buffer.size, frames, 2, &frame_count) == IFF_SAFE_OK);
	CHECK(frame_count == 2 && frames[0].pixels[0] == 1 && frames[1].pixels[0] == 9 && frames[1].pixels[1] == 9);
	iff_safe_free_frames(frames, frame_count);
	memset(frames, 0, sizeof(frames));
	CHECK(iff_safe_decode_anim(buffer.data, buffer.size, frames, 1, &frame_count) == IFF_SAFE_TOO_MANY_FRAMES);

	buffer = make_delta_anim(0x83);
	CHECK(iff_safe_decode_anim(buffer.data, buffer.size, frames, 2, &frame_count) == IFF_SAFE_CORRUPT);

	puts("PASS: bounded IFF/ABM decoding");
	return 0;
}
