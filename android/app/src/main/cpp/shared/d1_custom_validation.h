#ifndef DXX_REDUX_D1_CUSTOM_VALIDATION_H
#define DXX_REDUX_D1_CUSTOM_VALIDATION_H

#include <stddef.h>
#include <stdint.h>

typedef int (*d1_custom_name_lookup)(const char *name, void *context);

typedef struct d1_custom_validation_limits {
	int max_bitmaps;
	int max_sounds;
	int max_robots;
	int max_joints;
	int max_models;
	int max_object_bitmaps;
	int max_submodels;
	unsigned int rle_flag;
	unsigned int rle_big_flag;
} d1_custom_validation_limits;

int d1_custom_validate_pigpog(const uint8_t *data, size_t size,
                              const d1_custom_validation_limits *limits,
                              d1_custom_name_lookup bitmap_lookup, d1_custom_name_lookup sound_lookup,
                              void *lookup_context);
int d1_custom_validate_hx1(const uint8_t *data, size_t size,
                           const d1_custom_validation_limits *limits);

#endif
