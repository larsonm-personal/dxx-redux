#ifndef DXX_REDUX_D2_HAM_VALIDATION_H
#define DXX_REDUX_D2_HAM_VALIDATION_H

#include <stddef.h>
#include <stdint.h>

typedef struct d2_ham_validation_limits {
	int max_textures;
	int max_sounds;
	int max_vclips;
	int max_effects;
	int max_wall_anims;
	int max_robots;
	int max_joints;
	int max_weapons;
	int max_powerups;
	int max_models;
	int max_gauges;
	int max_object_bitmaps;
	int max_cockpits;
	int max_reactors;
	int max_bitmaps;
	int max_submodels;
} d2_ham_validation_limits;

int d2_ham_validate_main(const uint8_t *data, size_t size,
                         const d2_ham_validation_limits *limits);
int d2_ham_validate_extra(const uint8_t *data, size_t size, int type,
                          const d2_ham_validation_limits *limits, int base_weapons, int base_robots,
                          int base_joints, int base_models, int base_object_bitmaps,
                          int base_object_bitmap_ptrs);
int d2_ham_validate_hxm(const uint8_t *data, size_t size,
                        const d2_ham_validation_limits *limits, int robot_count, int joint_count,
                        int model_count);

#endif
