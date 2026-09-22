/*
 * Prepare D1 per-level custom assets without changing the live engine registry.
 */

#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "bounded_rle.h"
#include "pstypes.h"
#include "strutil.h"
#include "physfsx.h"
#include "gr.h"
#include "u_mem.h"
#include "piggy.h"
#include "makesig.h"
#include "d1_custom.h"
#include "d1_in_d2_assets.h"
#include "d1_pig_validation.h"
#include "ai.h"

#define D1_CUSTOM_DBM_FLAG_ABM            64
#define D1_CUSTOM_DBM_FLAG_LARGE          128
#define D1_CUSTOM_BITMAP_FLAGS_TO_COPY    (BM_FLAG_TRANSPARENT | BM_FLAG_SUPER_TRANSPARENT | BM_FLAG_NO_LIGHTING | BM_FLAG_RLE)
#define D1_CUSTOM_PIG1_BITMAP_HEADER_SIZE 17
#define D1_CUSTOM_PIG1_SOUND_HEADER_SIZE  20
#define D1_CUSTOM_POG_BITMAP_HEADER_SIZE  18

typedef struct d1_custom_bitmap_header {
	char name[8];
	ubyte dflags;
	ubyte width;
	ubyte height;
	ubyte flags;
	ubyte avg_color;
	int offset;
} d1_custom_bitmap_header;

typedef struct d1_custom_bitmap_header2 {
	char name[8];
	ubyte dflags;
	ubyte width;
	ubyte height;
	ubyte hi_wh;
	ubyte flags;
	ubyte avg_color;
	int offset;
} d1_custom_bitmap_header2;

typedef struct d1_custom_sound_header {
	char name[8];
	int length;
	int data_length;
	int offset;
} d1_custom_sound_header;

typedef struct d1_custom_bitmap_info {
	PHYSFS_sint64 offset;
	int repl_idx;
	ubyte flags;
	ubyte avg_color;
	int width;
	int height;
	int rle_big;
	ubyte *data;
} d1_custom_bitmap_info;

typedef struct d1_custom_sound_info {
	PHYSFS_sint64 offset;
	int repl_idx;
	int length;
	ubyte *data;
} d1_custom_sound_info;

/* HX1 uses the HXM disk layout, but native D1 ignores the D2-only robot fields
 * Reuse the format reader and retain only the native semantics */
static void d1_custom_read_robot(robot_info *robot, PHYSFS_file *fp, int weapon_count)
{
	robot_info_read_n(robot, 1, fp);
	if (robot->weapon_type >= weapon_count)
		robot->weapon_type = 0;
	robot->weapon_type2 = -1;
	robot->kamikaze = robot->badass = robot->energy_drain = 0;
	memset(robot->firing_wait2, 0, sizeof(robot->firing_wait2));
	robot->taunt_sound = robot->companion = robot->smart_blobs = robot->energy_blobs = 0;
	robot->thief = robot->pursuit = robot->death_roll = robot->flags = 0;
	memset(robot->pad, 0, sizeof(robot->pad));
	robot->deathroll_sound = robot->glow = 0;
	robot->lightcast = 1;
	robot->behavior = AIB_NORMAL;
	robot->aim = 255;
}

static int d1_custom_has_bytes(PHYSFS_file *fp, PHYSFS_sint64 size, PHYSFS_sint64 count)
{
	return d1_pig_validate_span(size, PHYSFS_tell(fp), count);
}

/* Counts are bounded by the remaining file, including each entry's index
 * Models have additional variable-length data checked before allocation */
static int d1_custom_definition_count(PHYSFS_file *fp, PHYSFS_sint64 size, int record_size, int *count)
{
	if (!d1_custom_has_bytes(fp, size, 4))
		return 0;
	*count = PHYSFSX_readInt(fp);
	return *count >= 0 && d1_custom_has_bytes(fp, size, (PHYSFS_sint64)*count * record_size);
}

static int d1_custom_read_definitions(d1_asset_generation *generation, const char *level_name, const char **error)
{
	char filename[PATH_MAX];
	PHYSFS_file *fp;
	PHYSFS_sint64 size;
	const char *stage = "HX1 header";
	int count, i, index, valid = 0;

	if (error)
		*error = NULL;
	if (!level_name || strlen(level_name) + 4 >= sizeof(filename)) {
		if (error)
			*error = "HX1 filename";
		return 0;
	}
	change_filename_extension(filename, level_name, ".hx1");
	if (!(fp = PHYSFSX_openReadBuffered(filename))) {
		if (!PHYSFSX_exists(filename, 1))
			return 1;
		if (error)
			*error = "HX1 file";
		return 0;
	}
	size = PHYSFS_fileLength(fp);
	if (!d1_custom_has_bytes(fp, size, 8) || PHYSFSX_readInt(fp) != 0x21584d48 || PHYSFSX_readInt(fp) != 1)
		goto done;
	stage = "HX1 robots";
	if (!d1_custom_definition_count(fp, size, 484, &count))
		goto done;
	for (i = 0; i < count; i++) {
		index = PHYSFSX_readInt(fp);
		if (index < 0 || index >= generation->num_robot_types)
			goto done;
		d1_custom_read_robot(&generation->robots[index], fp, generation->num_weapon_types);
	}
	stage = "HX1 joints";
	if (!d1_custom_definition_count(fp, size, 12, &count))
		goto done;
	for (i = 0; i < count; i++) {
		index = PHYSFSX_readInt(fp);
		if (index < 0 || index >= generation->num_robot_joints)
			goto done;
		jointpos_read_n(&generation->joints[index], 1, fp);
	}
	stage = "HX1 models";
	if (!d1_custom_definition_count(fp, size, 4 + 734 + 8, &count))
		goto done;
	for (i = 0; i < count; i++) {
		polymodel *model;
		if (!d1_custom_has_bytes(fp, size, 4 + 734))
			goto done;
		index = PHYSFSX_readInt(fp);
		if (index < 0 || index >= generation->num_polygon_models)
			goto done;
		model = &generation->models[index];
		d_free(model->model_data);
		polymodel_read_n(model, 1, fp);
		/* The serialized pointer is not an allocation, even on a failed read */
		model->model_data = NULL;
		if (model->model_data_size <= 0 || !d1_custom_has_bytes(fp, size, (PHYSFS_sint64)model->model_data_size + 8))
			goto done;
		model->model_data = d_malloc(model->model_data_size);
		if (!model->model_data || PHYSFS_read(fp, model->model_data, 1, model->model_data_size) != model->model_data_size)
			goto done;
		generation->dying_models[index] = PHYSFSX_readInt(fp);
		generation->dead_models[index] = PHYSFSX_readInt(fp);
	}
	stage = "HX1 object bitmaps";
	if (!d1_custom_definition_count(fp, size, 6, &count))
		goto done;
	for (i = 0; i < count; i++) {
		index = PHYSFSX_readInt(fp);
		if (index < 0 || index >= D1_MAX_OBJ_BITMAPS)
			goto done;
		bitmap_index_read(&generation->obj_bitmaps[index], fp);
	}
	valid = d1_in_d2_validate_asset_references(generation, &stage);
done:
	PHYSFS_close(fp);
	if (error)
		*error = valid ? NULL : stage;
	return valid;
}

static int d1_custom_read_bitmap_header(PHYSFS_file *fp, d1_custom_bitmap_header *bmh)
{
	if (PHYSFS_read(fp, bmh->name, 8, 1) < 1)
		return 0;
	bmh->dflags = PHYSFSX_readByte(fp);
	bmh->width = PHYSFSX_readByte(fp);
	bmh->height = PHYSFSX_readByte(fp);
	bmh->flags = PHYSFSX_readByte(fp);
	bmh->avg_color = PHYSFSX_readByte(fp);
	bmh->offset = PHYSFSX_readInt(fp);
	return 1;
}

static int d1_custom_read_bitmap_header2(PHYSFS_file *fp, d1_custom_bitmap_header2 *bmh)
{
	if (PHYSFS_read(fp, bmh->name, 8, 1) < 1)
		return 0;
	bmh->dflags = PHYSFSX_readByte(fp);
	bmh->width = PHYSFSX_readByte(fp);
	bmh->height = PHYSFSX_readByte(fp);
	bmh->hi_wh = PHYSFSX_readByte(fp);
	bmh->flags = PHYSFSX_readByte(fp);
	bmh->avg_color = PHYSFSX_readByte(fp);
	bmh->offset = PHYSFSX_readInt(fp);
	return 1;
}

static int d1_custom_read_sound_header(PHYSFS_file *fp, d1_custom_sound_header *sndh)
{
	if (PHYSFS_read(fp, sndh->name, 8, 1) < 1)
		return 0;
	sndh->length = PHYSFSX_readInt(fp);
	sndh->data_length = PHYSFSX_readInt(fp);
	sndh->offset = PHYSFSX_readInt(fp);
	return 1;
}

static void d1_custom_bitmap_name(char *name, const char *disk_name, ubyte dflags)
{
	memcpy(name, disk_name, 8);
	name[8] = 0;
	if (dflags & D1_CUSTOM_DBM_FLAG_ABM)
		sprintf(strchr(name, 0), "#%d", dflags & 63);
}

static int d1_custom_stage_bitmap(PHYSFS_file *fp, d1_custom_bitmap_info *info)
{
	ubyte *data;
	int data_size;
	PHYSFS_sint64 file_len = PHYSFS_fileLength(fp);
	PHYSFS_sint64 raw_size;

	if (info->repl_idx < 0 || info->repl_idx >= MAX_BITMAP_FILES)
		return info->repl_idx < 0;
	if (info->width <= 0 || info->height <= 0 || info->offset < 0 || info->offset >= file_len)
		return 0;

	if (!PHYSFS_seek(fp, info->offset))
		return 0;
	if (info->flags & BM_FLAG_RLE) {
		if (info->rle_big || !d1_custom_has_bytes(fp, file_len, 4))
			return 0;
		data_size = PHYSFSX_readInt(fp);
		if (data_size < 4 || info->height > data_size - 4)
			return 0;
		if (!PHYSFS_seek(fp, info->offset))
			return 0;
	} else {
		raw_size = (PHYSFS_sint64)info->width * info->height;
		if (raw_size <= 0 || raw_size > INT_MAX)
			return 0;
		data_size = (int)raw_size;
	}
	if (data_size <= 0 || data_size > file_len - info->offset)
		return 0;

	MALLOC(data, ubyte, data_size);
	if (!data)
		return 0;
	if (PHYSFS_read(fp, data, 1, data_size) < data_size) {
		d_free(data);
		return 0;
	}
	if ((info->flags & BM_FLAG_RLE) &&
		!bounded_rle_validate_bitmap(data, (size_t)data_size,
			info->width, info->height, 1)) {
		d_free(data);
		return 0;
	}
	info->data = data;
	return 1;
}

static int d1_custom_bitmap_index(const d1_bitmap_generation *images, const char *name)
{
	int i;
	for (i = 1; i <= images->bitmap_count; i++)
		if (!d_stricmp(images->names[i], name))
			return i;
	return -1;
}

static int d1_custom_sound_index(const d1_sound_generation *sounds, const char *name)
{
	int i;
	for (i = 0; i < sounds->count; i++)
		if (!d_stricmp(sounds->names[i], name))
			return i;
	return -1;
}

/* Keep samples in the same arena ownership used by base publication
 * Custom samples are still 11 kHz; output conversion happens exactly once */
static int d1_custom_pack_sounds(d1_sound_generation *bank, const d1_custom_sound_info *info, int count)
{
	const d1_custom_sound_info *replacements[MAX_SOUND_FILES] = {0};
	PHYSFS_sint64 bytes = 0;
	ubyte *data;
	int i;
	for (i = 0; i < count; i++)
		if (info[i].data)
			replacements[info[i].repl_idx] = &info[i];
	for (i = 0; i < bank->count; i++) {
		bytes += replacements[i] ? replacements[i]->length : bank->samples[i].length;
		if (bytes > INT_MAX)
			return 0;
	}
	data = d_malloc((size_t)bytes + 16);
	if (!data)
		return 0;
	bytes = 0;
	for (i = 0; i < bank->count; i++) {
		digi_sound *sample = &bank->samples[i];
		const ubyte *source = replacements[i] ? replacements[i]->data : sample->data;
		int length = replacements[i] ? replacements[i]->length : sample->length;
		memcpy(data + bytes, source, length);
		sample->data = data + bytes;
		sample->length = length;
		bytes += length;
	}
	d_free(bank->data);
	bank->data = data;
	bank->bytes = (size_t)bytes;
	return 1;
}

static int d1_custom_read_file(d1_asset_generation *generation, const char *filename)
{
	PHYSFS_file *fp = PHYSFSX_openReadBuffered(filename);
	PHYSFS_sint64 file_size, data_start;
	int first, second, i, num_bitmaps, num_sounds = 0, pog, has_indices, valid = 0;
	d1_custom_bitmap_info *bitmaps = NULL;
	d1_custom_sound_info *sounds = NULL;
	d1_custom_texture_stats *stats = &generation->custom_stats;

	if (!fp)
		return !PHYSFSX_exists(filename, 1);
	stats->files_found++;
	file_size = PHYSFS_fileLength(fp);
	if (!d1_custom_has_bytes(fp, file_size, 8))
		goto done;
	first = PHYSFSX_readInt(fp);
	second = PHYSFSX_readInt(fp);
	pog = first == MAKE_SIG('G', 'I', 'P', 'P') || first == MAKE_SIG('G', 'O', 'P', 'D');
	has_indices = first == MAKE_SIG('G', 'O', 'P', 'D');
	if (pog) {
		if (second != (has_indices ? 1 : 2) || !d1_custom_has_bytes(fp, file_size, 4))
			goto done;
		num_bitmaps = PHYSFSX_readInt(fp);
	} else if (first >= 0 && first <= D1_MAX_BITMAP_FILES && second >= 0 && second <= MAX_SOUND_FILES) {
		num_bitmaps = first;
		num_sounds = second;
	} else {
		/* Registered-style PIG directory follows its property tables */
		if (first < 8 || !d1_pig_validate_span(file_size, first, 8) || !PHYSFS_seek(fp, first))
			goto done;
		num_bitmaps = PHYSFSX_readInt(fp);
		num_sounds = PHYSFSX_readInt(fp);
	}
	if (num_bitmaps < 0 || num_bitmaps > D1_MAX_BITMAP_FILES || num_sounds < 0 || num_sounds > MAX_SOUND_FILES)
		goto done;
	data_start = PHYSFS_tell(fp) + num_bitmaps * (pog ? D1_CUSTOM_POG_BITMAP_HEADER_SIZE + 2 * has_indices : D1_CUSTOM_PIG1_BITMAP_HEADER_SIZE) +
	             num_sounds * D1_CUSTOM_PIG1_SOUND_HEADER_SIZE;
	if (data_start < 0 || data_start > file_size)
		goto done;
	if (num_bitmaps) {
		bitmaps = d_malloc(num_bitmaps * sizeof(*bitmaps));
		if (!bitmaps)
			goto done;
		memset(bitmaps, 0, num_bitmaps * sizeof(*bitmaps));
	}
	if (has_indices)
		for (i = 0; i < num_bitmaps; i++) {
			/* DPOG indices are authoritative source IDs, independent of live
			 * profile, header names, or legacy D1-to-D2 texture mappings */
			bitmaps[i].repl_idx = (ushort)PHYSFSX_readShort(fp);
			if (bitmaps[i].repl_idx <= 0 || bitmaps[i].repl_idx > generation->bitmap_data->bitmap_count)
				goto done;
		}
	for (i = 0; i < num_bitmaps; i++) {
		d1_custom_bitmap_info *info = &bitmaps[i];
		char name[15];
		int offset;
		if (pog) {
			d1_custom_bitmap_header2 header;
			if (!d1_custom_read_bitmap_header2(fp, &header))
				goto done;
			d1_custom_bitmap_name(name, header.name, header.dflags);
			offset = header.offset;
			info->width = header.width + ((header.hi_wh & 15) << 8);
			info->height = header.height + ((header.hi_wh >> 4) << 8);
			info->flags = header.flags & D1_CUSTOM_BITMAP_FLAGS_TO_COPY;
			info->rle_big = (header.flags & BM_FLAG_RLE_BIG) != 0;
			info->avg_color = header.avg_color;
		} else {
			d1_custom_bitmap_header header;
			if (!d1_custom_read_bitmap_header(fp, &header))
				goto done;
			d1_custom_bitmap_name(name, header.name, header.dflags);
			offset = header.offset;
			info->width = header.width + ((header.dflags & D1_CUSTOM_DBM_FLAG_LARGE) ? 256 : 0);
			info->height = header.height;
			info->flags = header.flags & D1_CUSTOM_BITMAP_FLAGS_TO_COPY;
			info->rle_big = (header.flags & BM_FLAG_RLE_BIG) != 0;
			info->avg_color = header.avg_color;
		}
		info->offset = data_start + offset;
		if (offset < 0 || info->offset >= file_size)
			goto done;
		if (!has_indices)
			info->repl_idx = d1_custom_bitmap_index(generation->bitmap_data, name);
		stats->bitmap_entries++;
		if (info->repl_idx < 0)
			stats->bitmap_unresolved++;
	}
	if (num_sounds) {
		sounds = d_malloc(num_sounds * sizeof(*sounds));
		if (!sounds)
			goto done;
		memset(sounds, 0, num_sounds * sizeof(*sounds));
	}
	for (i = 0; i < num_sounds; i++) {
		d1_custom_sound_header header;
		char name[9];
		if (!d1_custom_read_sound_header(fp, &header))
			goto done;
		memcpy(name, header.name, 8);
		name[8] = 0;
		sounds[i].offset = data_start + header.offset;
		sounds[i].length = header.length;
		sounds[i].repl_idx = d1_custom_sound_index(&generation->sound_bank, name);
		if (header.offset < 0 || sounds[i].offset >= file_size)
			goto done;
		stats->sound_entries++;
		if (sounds[i].repl_idx < 0)
			stats->sound_unresolved++;
	}
	for (i = 0; i < num_bitmaps; i++)
		if (!d1_custom_stage_bitmap(fp, &bitmaps[i]))
			goto done;
	for (i = 0; i < num_sounds; i++) {
		d1_custom_sound_info *info = &sounds[i];
		if (info->repl_idx < 0)
			continue;
		if (info->length <= 0 || !d1_pig_validate_span(file_size, info->offset, info->length) || !PHYSFS_seek(fp, info->offset))
			goto done;
		info->data = d_malloc(info->length);
		if (!info->data || PHYSFS_read(fp, info->data, 1, info->length) != info->length)
			goto done;
		stats->sound_applied++;
	}
	if (num_sounds && !d1_custom_pack_sounds(&generation->sound_bank, sounds, num_sounds))
		goto done;
	for (i = 0; i < num_bitmaps; i++) {
		d1_custom_bitmap_info *info = &bitmaps[i];
		grs_bitmap *bitmap;
		if (!info->data)
			continue;
		bitmap = &generation->bitmap_data->bitmaps[info->repl_idx];
		d_free(bitmap->bm_data);
		gr_init_bitmap(bitmap, BM_LINEAR, 0, 0, info->width, info->height, info->width, info->data);
		gr_set_bitmap_flags(bitmap, info->flags);
		bitmap->avg_color = info->avg_color;
		info->data = NULL;
		stats->bitmap_applied++;
	}
	valid = 1;
done:
	if (bitmaps) {
		for (i = 0; i < num_bitmaps; i++)
			if (bitmaps[i].data)
				d_free(bitmaps[i].data);
		d_free(bitmaps);
	}
	if (sounds) {
		for (i = 0; i < num_sounds; i++)
			if (sounds[i].data)
				d_free(sounds[i].data);
		d_free(sounds);
	}
	PHYSFS_close(fp);
	return valid;
}

int d1_custom_read_assets(d1_asset_generation *generation, const char *level_name, const char **error)
{
	char filename[PATH_MAX];
	const char *stage = "custom asset filename";
	int valid = 0;
	if (!level_name || strlen(level_name) + 4 >= sizeof(filename))
		goto done;
	memset(&generation->custom_stats, 0, sizeof(generation->custom_stats));
	stage = "PG1 custom assets";
	change_filename_extension(filename, level_name, ".pg1");
	if (!d1_custom_read_file(generation, filename))
		goto done;
	stage = "DTX custom assets";
	change_filename_extension(filename, level_name, ".dtx");
	if (!d1_custom_read_file(generation, filename))
		goto done;
	valid = d1_custom_read_definitions(generation, level_name, &stage);
done:
	if (error)
		*error = valid ? NULL : stage;
	return valid;
}
