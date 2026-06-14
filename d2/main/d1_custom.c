/*
 * Load D1 per-level custom textures while running D1 missions in D2.
 */

#include <stdio.h>
#include <string.h>

#include "pstypes.h"
#include "strutil.h"
#include "physfsx.h"
#include "gr.h"
#include "hash.h"
#include "u_mem.h"
#include "piggy.h"
#include "texmerge.h"
#include "console.h"
#include "makesig.h"
#include "d1_custom.h"

#define D1_CUSTOM_DBM_FLAG_ABM           64
#define D1_CUSTOM_DBM_FLAG_LARGE         128
#define D1_CUSTOM_VALID                  1
#define D1_CUSTOM_BITMAP_FLAGS_TO_COPY   (BM_FLAG_TRANSPARENT | BM_FLAG_SUPER_TRANSPARENT | BM_FLAG_NO_LIGHTING | BM_FLAG_RLE)
#define D1_CUSTOM_POG_BITMAP_HEADER_SIZE 18

extern hashtable AllBitmapsNames;

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
	int offset;
	int repl_idx;
	ubyte flags;
	ubyte avg_color;
	int width;
	int height;
} d1_custom_bitmap_info;

static grs_bitmap Bitmap_original[MAX_BITMAP_FILES];
static int Bitmap_original_offset[MAX_BITMAP_FILES];
static ubyte Bitmap_original_file_flags[MAX_BITMAP_FILES];
static ubyte Bitmap_original_valid[MAX_BITMAP_FILES];
static d1_custom_texture_stats Last_stats;

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

static int d1_custom_parse_pig1(PHYSFS_file *fp, int first, int second, int *num_bitmaps, int *num_sounds, int *data_ofs)
{
	int file_len = (int) PHYSFS_fileLength(fp);

	if (first >= 0 && first <= MAX_BITMAP_FILES && second >= 0 && second <= MAX_SOUND_FILES) {
		*num_bitmaps = first;
		*num_sounds = second;
		*data_ofs = 8;
		PHYSFSX_fseek(fp, 8, SEEK_SET);
	} else if (first > 0 && first < file_len) {
		PHYSFSX_fseek(fp, first, SEEK_SET);
		*num_bitmaps = PHYSFSX_readInt(fp);
		*num_sounds = PHYSFSX_readInt(fp);
		if (*num_bitmaps < 0 || *num_bitmaps > MAX_BITMAP_FILES || *num_sounds < 0 || *num_sounds > MAX_SOUND_FILES)
			return 0;
		*data_ofs = first + 8;
	} else {
		return 0;
	}

	*data_ofs += *num_bitmaps * 17 + *num_sounds * 20;
	if (*data_ofs < 0 || *data_ofs > file_len)
		return 0;
	return 1;
}

static void d1_custom_save_original(int bitmap_index)
{
	grs_bitmap *bmp;
	int offset;

	if (bitmap_index < 0 || bitmap_index >= MAX_BITMAP_FILES)
		return;
	if (Bitmap_original_valid[bitmap_index])
		return;

	bmp = &GameBitmaps[bitmap_index];
	offset = piggy_bitmap_get_offset(bitmap_index);
	Bitmap_original[bitmap_index] = *bmp;
#ifdef OGL
	Bitmap_original[bitmap_index].gltexture = NULL;
	Bitmap_original[bitmap_index].gltexture_mask = NULL;
#endif
	Bitmap_original_offset[bitmap_index] = offset;
	Bitmap_original_file_flags[bitmap_index] = piggy_bitmap_get_file_flags(bitmap_index);
	if (offset) {
		Bitmap_original[bitmap_index].bm_flags = BM_FLAG_PAGED_OUT;
		Bitmap_original[bitmap_index].bm_data = (ubyte *) (size_t) offset;
	}
	Bitmap_original_valid[bitmap_index] = D1_CUSTOM_VALID;
}

static int d1_custom_apply_bitmap(PHYSFS_file *fp, d1_custom_bitmap_info *info)
{
	grs_bitmap *bmp;
	ubyte *data;
	int data_size;
	PHYSFS_sint64 file_len = PHYSFS_fileLength(fp);

	if (info->repl_idx < 0 || info->repl_idx >= MAX_BITMAP_FILES)
		return 0;
	if (info->width <= 0 || info->height <= 0 || info->offset < 0 || info->offset >= file_len)
		return 0;

	PHYSFSX_fseek(fp, info->offset, SEEK_SET);
	if (info->flags & BM_FLAG_RLE) {
		data_size = PHYSFSX_readInt(fp);
		if (data_size < 4)
			return 0;
		PHYSFSX_fseek(fp, -4, SEEK_CUR);
	} else {
		data_size = info->width * info->height;
	}
	if (data_size <= 0 || (PHYSFS_sint64) info->offset + data_size > file_len)
		return 0;

	MALLOC(data, ubyte, data_size);
	if (!data)
		return 0;
	if (PHYSFS_read(fp, data, 1, data_size) < data_size) {
		d_free(data);
		return 0;
	}

	bmp = &GameBitmaps[info->repl_idx];
	if (Bitmap_original_valid[info->repl_idx])
		gr_free_bitmap_data(bmp);
	else {
		d1_custom_save_original(info->repl_idx);
		gr_set_bitmap_data(bmp, NULL);
	}

	piggy_bitmap_set_file_state(info->repl_idx, 0, info->flags);
	memset(bmp, 0, sizeof(*bmp));
	gr_init_bitmap(bmp, 0, 0, 0, info->width, info->height, info->width, data);
	gr_set_bitmap_flags(bmp, info->flags);
	bmp->avg_color = info->avg_color;
	return 1;
}

static int d1_custom_load_pog_data(PHYSFS_file *fp, int sig, int version, d1_custom_texture_stats *stats)
{
	int num_bitmaps;
	int has_repl = 0;
	int data_ofs;
	int i;
	int applied = 0;
	ushort *indices = NULL;
	d1_custom_bitmap_info *bitmap_info = NULL;
	PHYSFS_sint64 file_len = PHYSFS_fileLength(fp);

	if (sig == MAKE_SIG('G', 'I', 'P', 'P') && version == 2)
		has_repl = 0;
	else if (sig == MAKE_SIG('G', 'O', 'P', 'D') && version == 1)
		has_repl = 1;
	else
		return -1;

	num_bitmaps = PHYSFSX_readInt(fp);
	if (num_bitmaps < 0 || num_bitmaps > MAX_BITMAP_FILES)
		return -1;
	if (!num_bitmaps)
		return 0;

	data_ofs = 12 + num_bitmaps * D1_CUSTOM_POG_BITMAP_HEADER_SIZE + (has_repl ? num_bitmaps * 2 : 0);
	if (data_ofs < 0 || data_ofs > file_len)
		return -1;

	MALLOC(bitmap_info, d1_custom_bitmap_info, num_bitmaps);
	if (!bitmap_info)
		return -1;
	memset(bitmap_info, 0, num_bitmaps * sizeof(*bitmap_info));

	if (has_repl) {
		MALLOC(indices, ushort, num_bitmaps);
		if (!indices) {
			d_free(bitmap_info);
			return -1;
		}
		for (i = 0; i < num_bitmaps; i++)
			indices[i] = PHYSFSX_readShort(fp);
	}

	for (i = 0; i < num_bitmaps; i++) {
		d1_custom_bitmap_header2 bmh;
		char name[15];
		int name_idx;
		int repl_idx = -1;

		if (!d1_custom_read_bitmap_header2(fp, &bmh)) {
			if (indices)
				d_free(indices);
			d_free(bitmap_info);
			return -1;
		}

		d1_custom_bitmap_name(name, bmh.name, bmh.dflags);
		name_idx = hashtable_search(&AllBitmapsNames, name);
		if (name_idx >= 0)
			repl_idx = name_idx;
		else if (has_repl && indices[i] < MAX_BITMAP_FILES)
			repl_idx = indices[i];

		bitmap_info[i].offset = bmh.offset + data_ofs;
		bitmap_info[i].repl_idx = repl_idx;
		bitmap_info[i].flags = bmh.flags & D1_CUSTOM_BITMAP_FLAGS_TO_COPY;
		bitmap_info[i].avg_color = bmh.avg_color;
		bitmap_info[i].width = bmh.width + ((bmh.hi_wh & 15) << 8);
		bitmap_info[i].height = bmh.height + ((bmh.hi_wh >> 4) << 8);

		stats->bitmap_entries++;
		if (bitmap_info[i].repl_idx < 0)
			stats->bitmap_unresolved++;
	}

	for (i = 0; i < num_bitmaps; i++) {
		if (d1_custom_apply_bitmap(fp, &bitmap_info[i])) {
			stats->bitmap_applied++;
			applied++;
		}
	}

	if (indices)
		d_free(indices);
	d_free(bitmap_info);
	return applied;
}

static int d1_custom_load_file(char *filename, d1_custom_texture_stats *stats)
{
	PHYSFS_file *fp;
	int first, second;
	int num_bitmaps, num_sounds, data_ofs;
	int i;
	int applied = 0;
	d1_custom_bitmap_info *bitmap_info = NULL;

	fp = PHYSFSX_openReadBuffered(filename);
	if (!fp)
		return 0;

	stats->files_found++;
	first = PHYSFSX_readInt(fp);
	second = PHYSFSX_readInt(fp);

	applied = d1_custom_load_pog_data(fp, first, second, stats);
	if (applied >= 0) {
		PHYSFS_close(fp);
		return applied;
	}

	if (!d1_custom_parse_pig1(fp, first, second, &num_bitmaps, &num_sounds, &data_ofs)) {
		PHYSFS_close(fp);
		return 0;
	}

	MALLOC(bitmap_info, d1_custom_bitmap_info, num_bitmaps);
	if (!bitmap_info) {
		PHYSFS_close(fp);
		return 0;
	}

	for (i = 0; i < num_bitmaps; i++) {
		d1_custom_bitmap_header bmh;
		char name[15];

		if (!d1_custom_read_bitmap_header(fp, &bmh)) {
			d_free(bitmap_info);
			PHYSFS_close(fp);
			return 0;
		}

		d1_custom_bitmap_name(name, bmh.name, bmh.dflags);
		bitmap_info[i].offset = bmh.offset + data_ofs;
		bitmap_info[i].repl_idx = hashtable_search(&AllBitmapsNames, name);
		bitmap_info[i].flags = bmh.flags & D1_CUSTOM_BITMAP_FLAGS_TO_COPY;
		bitmap_info[i].avg_color = bmh.avg_color;
		bitmap_info[i].width = bmh.width + ((bmh.dflags & D1_CUSTOM_DBM_FLAG_LARGE) ? 256 : 0);
		bitmap_info[i].height = bmh.height;

		stats->bitmap_entries++;
		if (bitmap_info[i].repl_idx < 0)
			stats->bitmap_unresolved++;
	}

	for (i = 0; i < num_sounds; i++) {
		d1_custom_sound_header sndh;

		if (!d1_custom_read_sound_header(fp, &sndh)) {
			d_free(bitmap_info);
			PHYSFS_close(fp);
			return 0;
		}
		stats->sound_entries++;
	}

	for (i = 0; i < num_bitmaps; i++) {
		if (d1_custom_apply_bitmap(fp, &bitmap_info[i])) {
			stats->bitmap_applied++;
			applied++;
		}
	}

	d_free(bitmap_info);
	PHYSFS_close(fp);
	return applied;
}

void d1_custom_remove(void)
{
	int i;

	memset(&Last_stats, 0, sizeof(Last_stats));
	for (i = 0; i < MAX_BITMAP_FILES; i++) {
		grs_bitmap *bmp;

		if (!Bitmap_original_valid[i])
			continue;

		bmp = &GameBitmaps[i];
		gr_free_bitmap_data(bmp);
		*bmp = Bitmap_original[i];
		piggy_bitmap_set_file_state(i, Bitmap_original_offset[i], Bitmap_original_file_flags[i]);
		if (Bitmap_original_offset[i]) {
			gr_set_bitmap_flags(bmp, BM_FLAG_PAGED_OUT);
			gr_set_bitmap_data(bmp, Piggy_bitmap_cache_data);
		} else {
			gr_set_bitmap_flags(bmp, Bitmap_original[i].bm_flags);
		}
		Bitmap_original_valid[i] = 0;
	}
}

void d1_custom_load_data(char *level_name)
{
	char custom_file[FILENAME_LEN];
	d1_custom_texture_stats stats;

	memset(&stats, 0, sizeof(stats));
	d1_custom_remove();

	change_filename_extension(custom_file, level_name, ".pg1");
	d1_custom_load_file(custom_file, &stats);
	change_filename_extension(custom_file, level_name, ".dtx");
	d1_custom_load_file(custom_file, &stats);

	if (stats.files_found) {
		con_printf(CON_NORMAL, "D1 custom textures: files=%d bitmaps=%d applied=%d unresolved=%d sounds=%d\n",
		           stats.files_found, stats.bitmap_entries, stats.bitmap_applied, stats.bitmap_unresolved, stats.sound_entries);
		texmerge_flush();
	}
	Last_stats = stats;
}

void d1_custom_get_stats(d1_custom_texture_stats *stats)
{
	if (stats)
		*stats = Last_stats;
}
