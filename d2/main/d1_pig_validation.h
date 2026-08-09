#ifndef _D1_PIG_VALIDATION_H
#define _D1_PIG_VALIDATION_H

#include <stddef.h>
#include <stdint.h>

int d1_pig_validate_span(int64_t file_size, int64_t offset, int64_t size);
int d1_pig_validate_arena(size_t remaining, int data_size, int slack);
int d1_pig_validate_rle(const uint8_t *data, size_t size, int width, int height, int big_rows);
int d1_pig_measure_remapped_rle(const uint8_t *data, size_t size, int width, int height,
                                int big_rows, const uint8_t colormap[256], size_t *remapped_size);
int d1_pig_validate_model_stream(const uint8_t *data, size_t size, size_t offset);
int d1_pig_validate_sound_map(const uint8_t *map, size_t count, int sound_count);
int d1_pig_validate_timed_clip(int frame_count, int frame_capacity, int play_time, int frame_time);
int d1_pig_valid_model_index(int index, int model_count);
int d1_pig_valid_optional_model_index(int index, int model_count);

#endif
