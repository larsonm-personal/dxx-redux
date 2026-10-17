#include "d2_ham_validation.h"

#include "d1_in_d2/d1_pig_validation.h"

#include <limits.h>

#define D2_HAM_SIGNATURE         0x214d4148
#define D2_EXTRA_HAM_SIGNATURE   0x5848414d
#define D2_HXM_SIGNATURE         0x21584d48
#define D2_HAM_MAX_FILE_BYTES    (64u * 1024u * 1024u)
#define D2_HAM_MAX_MODEL_BYTES   (16u * 1024u * 1024u)
#define D2_HAM_MAX_MODELS        256
#define D2_HAM_MAX_ROBOTS        256
#define D2_HAM_MODEL_RECORD_SIZE 734u
#define D2_HAM_ROBOT_RECORD_SIZE 480u
#define D2_HAM_SUBMODELS         10

typedef struct d2_ham_cursor {
	const uint8_t *data;
	size_t size;
	size_t position;
	size_t end;
} d2_ham_cursor;

typedef struct d2_ham_model_meta {
	int model_count;
	int data_size;
	int submodel_ptrs[D2_HAM_SUBMODELS];
	int texture_count;
	int first_texture;
	int simpler_model;
} d2_ham_model_meta;

typedef struct d2_ham_robot_meta {
	int model;
	int guns;
	int joint_count[45];
	int joint_offset[45];
} d2_ham_robot_meta;

static int d2_ham_span(size_t size, size_t offset, size_t count)
{
	return offset <= size && count <= size - offset;
}

static int d2_ham_take(d2_ham_cursor *cursor, size_t count, const uint8_t **result)
{
	if (!cursor || cursor->end > cursor->size || !d2_ham_span(cursor->end, cursor->position, count))
		return 0;
	if (result) *result = cursor->data + cursor->position;
	cursor->position += count;
	return 1;
}

static uint16_t d2_ham_u16(const uint8_t *data)
{
	return (uint16_t) (data[0] | ((uint16_t) data[1] << 8));
}

static int16_t d2_ham_i16(const uint8_t *data)
{
	return (int16_t) d2_ham_u16(data);
}

static uint32_t d2_ham_u32(const uint8_t *data)
{
	return (uint32_t) data[0] | ((uint32_t) data[1] << 8) |
	       ((uint32_t) data[2] << 16) | ((uint32_t) data[3] << 24);
}

static int32_t d2_ham_i32(const uint8_t *data)
{
	return (int32_t) d2_ham_u32(data);
}

static int d2_ham_read_i32(d2_ham_cursor *cursor, int *value)
{
	const uint8_t *data;
	if (!value || !d2_ham_take(cursor, 4, &data)) return 0;
	*value = d2_ham_i32(data);
	return 1;
}

static int d2_ham_read_count(d2_ham_cursor *cursor, int maximum, int *count)
{
	return maximum >= 0 && d2_ham_read_i32(cursor, count) && *count >= 0 && *count <= maximum;
}

static int d2_ham_skip_records(d2_ham_cursor *cursor, int maximum, size_t record_size,
                               int *count)
{
	int value;
	if (!d2_ham_read_count(cursor, maximum, &value) ||
	    (record_size && (size_t) value > SIZE_MAX / record_size) ||
	    !d2_ham_take(cursor, (size_t) value * record_size, NULL))
		return 0;
	if (count) *count = value;
	return 1;
}

static int d2_ham_valid_optional_index(int value, int count)
{
	return value == -1 || (value >= 0 && value < count);
}

static int d2_ham_read_robot_meta(const uint8_t *record, d2_ham_robot_meta *meta)
{
	int joint;
	if (!record || !meta) return 0;
	meta->model = d2_ham_i32(record);
	meta->guns = record[118];
	if (meta->guns > 8) return 0;
	for (joint = 0; joint < 45; ++joint) {
		meta->joint_count[joint] = d2_ham_i16(record + 296 + joint * 4);
		meta->joint_offset[joint] = d2_ham_i16(record + 298 + joint * 4);
		if (meta->joint_count[joint] < 0 || meta->joint_offset[joint] < 0)
			return 0;
	}
	for (joint = 0; joint < meta->guns; ++joint)
		if (record[100 + joint] >= D2_HAM_SUBMODELS) return 0;
	return 1;
}

static int d2_ham_validate_robot_meta(const d2_ham_robot_meta *meta, int model_count,
                                      int joint_count)
{
	int joint;
	if (!meta || meta->model < 0 || meta->model >= model_count) return 0;
	for (joint = 0; joint < 45; ++joint)
		if (meta->joint_offset[joint] > joint_count ||
		    meta->joint_count[joint] > joint_count - meta->joint_offset[joint])
			return 0;
	return 1;
}

static int d2_ham_read_model_meta(const uint8_t *record, d2_ham_model_meta *meta,
                                  int model_limit, int object_bitmap_limit, int submodel_limit)
{
	int submodel;
	if (!record || !meta || submodel_limit <= 0 || submodel_limit > D2_HAM_SUBMODELS)
		return 0;
	meta->model_count = d2_ham_i32(record);
	meta->data_size = d2_ham_i32(record + 4);
	meta->texture_count = record[730];
	meta->first_texture = d2_ham_u16(record + 731);
	meta->simpler_model = record[733];
	if (meta->model_count <= 0 || meta->model_count > submodel_limit ||
	    meta->data_size <= 0 || (size_t) meta->data_size > D2_HAM_MAX_MODEL_BYTES ||
	    meta->first_texture > object_bitmap_limit ||
	    meta->texture_count > object_bitmap_limit - meta->first_texture ||
	    (meta->simpler_model && meta->simpler_model > model_limit))
		return 0;
	for (submodel = 0; submodel < D2_HAM_SUBMODELS; ++submodel)
		meta->submodel_ptrs[submodel] = d2_ham_i32(record + 12 + submodel * 4);
	return 1;
}

static int d2_ham_validate_model_data(d2_ham_cursor *cursor,
                                      const d2_ham_model_meta *meta, size_t *total_model_bytes)
{
	const uint8_t *model_data;
	int submodel;
	if (!cursor || !meta || !total_model_bytes ||
	    (size_t) meta->data_size > D2_HAM_MAX_FILE_BYTES - *total_model_bytes ||
	    !d2_ham_take(cursor, (size_t) meta->data_size, &model_data))
		return 0;
	*total_model_bytes += (size_t) meta->data_size;
	for (submodel = 0; submodel < meta->model_count; ++submodel) {
		int offset = meta->submodel_ptrs[submodel];
		if (offset < 0 ||
		    !d1_pig_validate_model_textures(model_data, (size_t) meta->data_size,
		                                    (size_t) offset, meta->texture_count))
			return 0;
	}
	return 1;
}

static int d2_ham_read_model_records(d2_ham_cursor *cursor, int count,
                                     d2_ham_model_meta *models, int model_limit, int object_bitmap_limit,
                                     int submodel_limit)
{
	int i;
	for (i = 0; i < count; ++i) {
		const uint8_t *record;
		if (!d2_ham_take(cursor, D2_HAM_MODEL_RECORD_SIZE, &record) ||
		    !d2_ham_read_model_meta(record, &models[i], model_limit,
		                            object_bitmap_limit, submodel_limit))
			return 0;
	}
	return 1;
}

static int d2_ham_validate_model_trailing(d2_ham_cursor *cursor, int count,
                                          int model_limit)
{
	int i, value;
	for (i = 0; i < count * 2; ++i)
		if (!d2_ham_read_i32(cursor, &value) || !d2_ham_valid_optional_index(value, model_limit))
			return 0;
	return 1;
}

static int d2_ham_validate_bitmap_table(d2_ham_cursor *cursor, int count,
                                        int max_bitmaps, const uint8_t **table)
{
	const uint8_t *values;
	int i;
	if (!d2_ham_take(cursor, (size_t) count * 2, &values)) return 0;
	for (i = 0; i < count; ++i)
		if (d2_ham_u16(values + (size_t) i * 2) >= max_bitmaps) return 0;
	if (table) *table = values;
	return 1;
}

int d2_ham_validate_main(const uint8_t *data, size_t size,
                         const d2_ham_validation_limits *limits)
{
	d2_ham_cursor cursor;
	d2_ham_model_meta models[D2_HAM_MAX_MODELS];
	d2_ham_robot_meta robots[D2_HAM_MAX_ROBOTS];
	const uint8_t *records, *pointers;
	size_t total_model_bytes = 0;
	int version, sound_offset = 0, count, robot_count, joint_count, model_count, i, value;
	if (!data || !limits || size < 8 || size > D2_HAM_MAX_FILE_BYTES ||
	    limits->max_models > D2_HAM_MAX_MODELS || limits->max_robots > D2_HAM_MAX_ROBOTS)
		return 0;
	cursor.data = data;
	cursor.size = size;
	cursor.position = 0;
	cursor.end = size;
	if (!d2_ham_read_i32(&cursor, &value) || value != D2_HAM_SIGNATURE ||
	    !d2_ham_read_i32(&cursor, &version) || version < 1 || version > 3)
		return 0;
	if (version < 3) {
		if (!d2_ham_read_i32(&cursor, &sound_offset) || sound_offset < (int) cursor.position ||
		    (size_t) sound_offset > size)
			return 0;
		cursor.end = (size_t) sound_offset;
	}
	if (!d2_ham_skip_records(&cursor, limits->max_textures, 22, NULL) ||
	    !d2_ham_skip_records(&cursor, limits->max_sounds, 2, NULL) ||
	    !d2_ham_skip_records(&cursor, limits->max_vclips, 82, NULL) ||
	    !d2_ham_skip_records(&cursor, limits->max_effects, 130, NULL) ||
	    !d2_ham_skip_records(&cursor, limits->max_wall_anims, 126, NULL) ||
	    !d2_ham_read_count(&cursor, limits->max_robots, &robot_count))
		return 0;
	for (i = 0; i < robot_count; ++i) {
		if (!d2_ham_take(&cursor, D2_HAM_ROBOT_RECORD_SIZE, &records) ||
		    !d2_ham_read_robot_meta(records, &robots[i]))
			return 0;
	}
	if (!d2_ham_skip_records(&cursor, limits->max_joints, 8, &joint_count) ||
	    !d2_ham_skip_records(&cursor, limits->max_weapons, version >= 3 ? 125 : 118, NULL) ||
	    !d2_ham_skip_records(&cursor, limits->max_powerups, 16, NULL) ||
	    !d2_ham_read_count(&cursor, limits->max_models, &model_count) ||
	    !d2_ham_read_model_records(&cursor, model_count, models, model_count,
	                               limits->max_object_bitmaps, limits->max_submodels))
		return 0;
	for (i = 0; i < robot_count; ++i)
		if (!d2_ham_validate_robot_meta(&robots[i], model_count, joint_count)) return 0;
	for (i = 0; i < model_count; ++i)
		if (!d2_ham_validate_model_data(&cursor, &models[i], &total_model_bytes)) return 0;
	if (!d2_ham_validate_model_trailing(&cursor, model_count, model_count) ||
	    !d2_ham_skip_records(&cursor, limits->max_gauges, 4, NULL) ||
	    !d2_ham_read_count(&cursor, limits->max_object_bitmaps, &count) ||
	    !d2_ham_validate_bitmap_table(&cursor, count, limits->max_bitmaps, NULL) ||
	    !d2_ham_take(&cursor, (size_t) count * 2, &pointers))
		return 0;
	for (i = 0; i < count; ++i)
		if (d2_ham_u16(pointers + (size_t) i * 2) >= count) return 0;
	for (i = 0; i < model_count; ++i)
		if (models[i].first_texture > count || models[i].texture_count > count - models[i].first_texture)
			return 0;
	if (!d2_ham_take(&cursor, 132, NULL) ||
	    !d2_ham_skip_records(&cursor, limits->max_cockpits, 2, NULL) ||
	    !d2_ham_read_i32(&cursor, &value) ||
	    (value != 0 && !d2_ham_valid_optional_index(value, count)) ||
	    !d2_ham_skip_records(&cursor, limits->max_reactors, 200, NULL) ||
	    !d2_ham_read_i32(&cursor, &value) ||
	    (value != 0 && !d2_ham_valid_optional_index(value, model_count)))
		return 0;
	if (version < 3)
		for (i = 0; i < 2; ++i)
			if (!d2_ham_read_i32(&cursor, &value) || !d2_ham_valid_optional_index(value, model_count))
				return 0;
	return cursor.position <= cursor.end;
}

int d2_ham_validate_extra(const uint8_t *data, size_t size, int type,
                          const d2_ham_validation_limits *limits, int base_weapons, int base_robots,
                          int base_joints, int base_models, int base_object_bitmaps,
                          int base_object_bitmap_ptrs)
{
	d2_ham_cursor cursor;
	d2_ham_model_meta models[D2_HAM_MAX_MODELS];
	d2_ham_robot_meta robots[D2_HAM_MAX_ROBOTS];
	const uint8_t *record, *pointers;
	size_t total_model_bytes = 0;
	int robot_count, joint_count, model_count, bitmaps, pointer_count, i, value;
	if (!data || !limits || (type != 1 && type != 2) || size > D2_HAM_MAX_FILE_BYTES ||
	    limits->max_models > D2_HAM_MAX_MODELS || limits->max_robots > D2_HAM_MAX_ROBOTS ||
	    base_weapons < 0 || base_robots < 0 || base_joints < 0 || base_models < 0 ||
	    base_object_bitmaps < 0 || base_object_bitmap_ptrs < 0)
		return 0;
	cursor.data = data;
	cursor.size = cursor.end = size;
	cursor.position = 0;
	if (type == 2 && (!d2_ham_read_i32(&cursor, &value) || value != D2_EXTRA_HAM_SIGNATURE ||
	                  !d2_ham_read_i32(&cursor, &value) || value != 1))
		return 0;
	if (!d2_ham_skip_records(&cursor, limits->max_weapons - base_weapons, 125, NULL) ||
	    !d2_ham_read_count(&cursor, limits->max_robots - base_robots, &robot_count))
		return 0;
	for (i = 0; i < robot_count; ++i) {
		if (!d2_ham_take(&cursor, D2_HAM_ROBOT_RECORD_SIZE, &record) ||
		    !d2_ham_read_robot_meta(record, &robots[i]))
			return 0;
	}
	if (!d2_ham_skip_records(&cursor, limits->max_joints - base_joints, 8, &joint_count) ||
	    !d2_ham_read_count(&cursor, limits->max_models - base_models, &model_count) ||
	    !d2_ham_read_model_records(&cursor, model_count, models, base_models + model_count,
	                               limits->max_object_bitmaps, limits->max_submodels))
		return 0;
	for (i = 0; i < robot_count; ++i)
		if (!d2_ham_validate_robot_meta(&robots[i], base_models + model_count,
		                                base_joints + joint_count))
			return 0;
	for (i = 0; i < model_count; ++i)
		if (!d2_ham_validate_model_data(&cursor, &models[i], &total_model_bytes)) return 0;
	if (!d2_ham_validate_model_trailing(&cursor, model_count, base_models + model_count) ||
	    !d2_ham_read_count(&cursor, limits->max_object_bitmaps - base_object_bitmaps, &bitmaps) ||
	    !d2_ham_validate_bitmap_table(&cursor, bitmaps, limits->max_bitmaps, NULL) ||
	    !d2_ham_read_count(&cursor, limits->max_object_bitmaps - base_object_bitmap_ptrs,
	                       &pointer_count) ||
	    !d2_ham_take(&cursor, (size_t) pointer_count * 2, &pointers))
		return 0;
	for (i = 0; i < pointer_count; ++i)
		if (d2_ham_u16(pointers + (size_t) i * 2) >= limits->max_object_bitmaps) return 0;
	if (cursor.position == cursor.end) return 1;
	/* The legacy type-2 writer appended a second pointer table that the loader ignored */
	return type == 2 &&
	       d2_ham_validate_bitmap_table(&cursor, pointer_count,
	                                    limits->max_object_bitmaps, NULL) &&
	       cursor.position == cursor.end;
}

int d2_ham_validate_hxm(const uint8_t *data, size_t size,
                        const d2_ham_validation_limits *limits, int robot_count, int joint_count,
                        int model_count)
{
	d2_ham_cursor cursor;
	d2_ham_model_meta meta;
	d2_ham_robot_meta robot;
	const uint8_t *record;
	size_t total_model_bytes = 0;
	int count, i, index, value;
	if (!data || !limits || size > D2_HAM_MAX_FILE_BYTES || robot_count < 0 ||
	    robot_count > limits->max_robots || joint_count < 0 || joint_count > limits->max_joints ||
	    model_count < 0 || model_count > limits->max_models)
		return 0;
	cursor.data = data;
	cursor.size = cursor.end = size;
	cursor.position = 0;
	if (!d2_ham_read_i32(&cursor, &value) || value != D2_HXM_SIGNATURE ||
	    !d2_ham_read_i32(&cursor, &value) || value != 1 ||
	    !d2_ham_read_count(&cursor, robot_count, &count))
		return 0;
	for (i = 0; i < count; ++i) {
		if (!d2_ham_read_i32(&cursor, &index) || index < 0 || index >= robot_count ||
		    !d2_ham_take(&cursor, D2_HAM_ROBOT_RECORD_SIZE, &record) ||
		    !d2_ham_read_robot_meta(record, &robot) ||
		    !d2_ham_validate_robot_meta(&robot, model_count, joint_count))
			return 0;
	}
	if (!d2_ham_read_count(&cursor, joint_count, &count)) return 0;
	for (i = 0; i < count; ++i)
		if (!d2_ham_read_i32(&cursor, &index) || index < 0 || index >= joint_count ||
		    !d2_ham_take(&cursor, 8, &record) || d2_ham_i16(record) < 0 ||
		    d2_ham_i16(record) >= limits->max_submodels)
			return 0;
	if (!d2_ham_read_count(&cursor, model_count, &count)) return 0;
	for (i = 0; i < count; ++i) {
		if (!d2_ham_read_i32(&cursor, &index) || index < 0 || index >= model_count ||
		    !d2_ham_take(&cursor, D2_HAM_MODEL_RECORD_SIZE, &record) ||
		    !d2_ham_read_model_meta(record, &meta, model_count,
		                            limits->max_object_bitmaps, limits->max_submodels) ||
		    !d2_ham_validate_model_data(&cursor, &meta, &total_model_bytes) ||
		    !d2_ham_read_i32(&cursor, &value) || !d2_ham_valid_optional_index(value, model_count) ||
		    !d2_ham_read_i32(&cursor, &value) || !d2_ham_valid_optional_index(value, model_count))
			return 0;
	}
	if (!d2_ham_read_count(&cursor, limits->max_object_bitmaps, &count)) return 0;
	for (i = 0; i < count; ++i)
		if (!d2_ham_read_i32(&cursor, &index) || index < 0 || index >= limits->max_object_bitmaps ||
		    !d2_ham_take(&cursor, 2, &record) || d2_ham_u16(record) >= limits->max_bitmaps)
			return 0;
	if (!d2_ham_read_count(&cursor, limits->max_object_bitmaps, &count)) return 0;
	for (i = 0; i < count; ++i)
		if (!d2_ham_read_i32(&cursor, &index) || index < 0 || index >= limits->max_object_bitmaps ||
		    !d2_ham_take(&cursor, 2, &record) || d2_ham_u16(record) >= limits->max_object_bitmaps)
			return 0;
	if (cursor.position == cursor.end) return 1;
	// Some legacy HXM writers append one unused zero word that the original loader ignored
	return cursor.end - cursor.position == 4 && d2_ham_i32(cursor.data + cursor.position) == 0;
}
