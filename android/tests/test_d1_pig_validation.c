#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "d1_pig_validation.h"

#define CHECK(condition)                                                            \
	do {                                                                            \
		if (!(condition)) {                                                         \
			fprintf(stderr, "CHECK failed at line %d: %s\n", __LINE__, #condition); \
			return 1;                                                               \
		}                                                                           \
	} while (0)

static void put_word(uint8_t *data, size_t offset, uint16_t value)
{
	data[offset] = (uint8_t) value;
	data[offset + 1] = (uint8_t) (value >> 8);
}

int main(void)
{
	uint8_t model[64];
	uint8_t rle[] = { 13, 0, 0, 0, 3, 4, 0xe3, 7, 0xe0, 1, 2, 3, 0xe0 };
	uint8_t colormap[256];
	uint8_t sound_map[] = { 0, 4, 255 };
	size_t remapped_size;

	CHECK(d1_pig_validate_span(100, 20, 80));
	CHECK(!d1_pig_validate_span(100, 20, 81));
	CHECK(!d1_pig_validate_span(100, -1, 1));
	CHECK(!d1_pig_validate_span(INT64_MAX, INT64_MAX - 1, 2));
	CHECK(d1_pig_validate_arena(132, 100, 32));
	CHECK(!d1_pig_validate_arena(131, 100, 32));
	CHECK(!d1_pig_validate_arena(100, -1, 0));
	CHECK(d1_pig_validate_sound_map(sound_map, sizeof(sound_map), 5));
	CHECK(!d1_pig_validate_sound_map(sound_map, sizeof(sound_map), 4));
	CHECK(d1_pig_validate_timed_clip(1, 30, 1, 1));
	CHECK(!d1_pig_validate_timed_clip(0, 30, 1, 1));
	CHECK(!d1_pig_validate_timed_clip(31, 30, 1, 1));
	CHECK(!d1_pig_validate_timed_clip(1, 30, 0, 1));
	memset(colormap, 0, sizeof(colormap));
	CHECK(d1_pig_validate_rle(rle, sizeof(rle), 3, 2, 0));
	CHECK(d1_pig_measure_remapped_rle(rle, sizeof(rle), 3, 2, 0, colormap, &remapped_size));
	CHECK(remapped_size == 13);
	colormap[1] = 0xe1;
	CHECK(d1_pig_measure_remapped_rle(rle, sizeof(rle), 3, 2, 0, colormap, &remapped_size));
	CHECK(remapped_size == 14);
	rle[0] = 12;
	CHECK(!d1_pig_validate_rle(rle, sizeof(rle), 3, 2, 0));
	rle[0] = sizeof(rle);
	rle[4] = 2;
	CHECK(!d1_pig_validate_rle(rle, sizeof(rle), 3, 2, 0));
	rle[4] = 3;
	rle[6] = 0xe4;
	CHECK(!d1_pig_validate_rle(rle, sizeof(rle), 3, 2, 0));
	CHECK(d1_pig_valid_model_index(0, 1));
	CHECK(!d1_pig_valid_model_index(-1, 1));
	CHECK(d1_pig_valid_optional_model_index(-1, 1));
	CHECK(!d1_pig_valid_optional_model_index(1, 1));

	memset(model, 0, sizeof(model));
	CHECK(d1_pig_validate_model_stream(model, sizeof(model), 0));

	put_word(model, 0, 8);
	put_word(model, 4, 0);
	CHECK(d1_pig_validate_model_stream(model, sizeof(model), 0));

	memset(model, 0, sizeof(model));
	put_word(model, 0, 6);
	put_word(model, 16, 20);
	put_word(model, 20, 0);
	CHECK(d1_pig_validate_model_stream(model, sizeof(model), 0));
	put_word(model, 16, 64);
	CHECK(!d1_pig_validate_model_stream(model, sizeof(model), 0));

	memset(model, 0, sizeof(model));
	put_word(model, 0, 1);
	put_word(model, 2, 6);
	CHECK(!d1_pig_validate_model_stream(model, 16, 0));
	put_word(model, 0, 99);
	CHECK(!d1_pig_validate_model_stream(model, sizeof(model), 0));
	CHECK(!d1_pig_validate_model_stream(model, sizeof(model), sizeof(model)));

	puts("D1 PIG validation tests passed");
	return 0;
}
