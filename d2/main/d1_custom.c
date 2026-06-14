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
#include "args.h"
#include "d1_custom.h"

#define D1_CUSTOM_DBM_FLAG_ABM            64
#define D1_CUSTOM_DBM_FLAG_LARGE          128
#define D1_CUSTOM_VALID                   1
#define D1_CUSTOM_BITMAP_FLAGS_TO_COPY    (BM_FLAG_TRANSPARENT | BM_FLAG_SUPER_TRANSPARENT | BM_FLAG_NO_LIGHTING | BM_FLAG_RLE)
#define D1_CUSTOM_PIG1_BITMAP_HEADER_SIZE 17
#define D1_CUSTOM_PIG1_SOUND_HEADER_SIZE  20
#define D1_CUSTOM_POG_BITMAP_HEADER_SIZE  18
#define D1_CUSTOM_SOUND_SOURCE_RATE       SAMPLE_RATE_11K

extern hashtable AllBitmapsNames;
extern hashtable AllDigiSndNames;

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

typedef struct d1_custom_sound_info {
	int offset;
	int repl_idx;
	int length;
} d1_custom_sound_info;

static grs_bitmap Bitmap_original[MAX_BITMAP_FILES];
static int Bitmap_original_offset[MAX_BITMAP_FILES];
static ubyte Bitmap_original_file_flags[MAX_BITMAP_FILES];
static ubyte Bitmap_original_valid[MAX_BITMAP_FILES];
static digi_sound Sound_original[MAX_SOUND_FILES];
static ubyte Sound_original_valid[MAX_SOUND_FILES];
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

static int d1_custom_target_sample_rate(void)
{
	return GameArg.SndDigiSampleRate > 0 ? GameArg.SndDigiSampleRate : SAMPLE_RATE_22K;
}

static void d1_custom_sound_decompress(ubyte *data, int size, ubyte *outp)
{
	static int index_table[16] = { -1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8 };
	static int step_table[89] = {
		7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28,
		31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
		130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494,
		544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
		2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630,
		9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
	};
	int newtoken = 1;
	int predicted = 0, index = 0, step = 7;

	while (size) {
		int code, diff, out;
		if (newtoken)
			code = (*data) & 15;
		else {
			code = (*(data++)) >> 4;
			size--;
		}
		newtoken ^= 1;
		diff = 0;
		if (code & 4)
			diff += step;
		if (code & 2)
			diff += (step >> 1);
		if (code & 1)
			diff += (step >> 2);
		diff += (step >> 3);
		if (code & 8)
			diff = -diff;
		out = predicted + diff;
		if (out > 32767)
			out = 32767;
		if (out < -32768)
			out = -32768;
		predicted = out;
		*(outp++) = ((out >> 8) & 255) ^ 0x80;
		index += index_table[code];
		if (index < 0)
			index = 0;
		if (index > 88)
			index = 88;
		step = step_table[index];
	}
}

static int d1_custom_resample_sound_data(ubyte *data, int length, ubyte **out_data, int *out_length)
{
	ubyte *resampled;
	int target_rate = d1_custom_target_sample_rate();
	int resampled_length;
	int i;

	*out_data = NULL;
	*out_length = 0;
	if (!data || length <= 0)
		return 0;

	if (target_rate == D1_CUSTOM_SOUND_SOURCE_RATE) {
		*out_data = data;
		*out_length = length;
		return 1;
	}

	resampled_length = (int) (((PHYSFS_sint64) length * target_rate + D1_CUSTOM_SOUND_SOURCE_RATE / 2) / D1_CUSTOM_SOUND_SOURCE_RATE);
	if (resampled_length <= 0) {
		d_free(data);
		return 0;
	}
	MALLOC(resampled, ubyte, resampled_length);
	if (!resampled) {
		d_free(data);
		return 0;
	}

	for (i = 0; i < resampled_length; i++) {
		int src = (int) (((PHYSFS_sint64) i * D1_CUSTOM_SOUND_SOURCE_RATE) / target_rate);
		if (src >= length)
			src = length - 1;
		resampled[i] = data[src];
	}
	d_free(data);
	*out_data = resampled;
	*out_length = resampled_length;
	return 1;
}

static int d1_custom_read_sound_data(PHYSFS_file *fp, int offset, int read_length, int output_length, int compressed, ubyte **out_data, int *out_length)
{
	ubyte *data;
	PHYSFS_sint64 file_len = PHYSFS_fileLength(fp);

	if (read_length <= 0 || output_length <= 0 || offset < 0 || (PHYSFS_sint64) offset + read_length > file_len)
		return 0;
	if (compressed && output_length != read_length * 2)
		return 0;

	MALLOC(data, ubyte, output_length);
	if (!data)
		return 0;

	PHYSFSX_fseek(fp, offset, SEEK_SET);
	if (compressed) {
		ubyte *compressed_data;

		MALLOC(compressed_data, ubyte, read_length);
		if (!compressed_data) {
			d_free(data);
			return 0;
		}
		if (PHYSFS_read(fp, compressed_data, 1, read_length) < read_length) {
			d_free(compressed_data);
			d_free(data);
			return 0;
		}
		d1_custom_sound_decompress(compressed_data, read_length, data);
		d_free(compressed_data);
	} else if (PHYSFS_read(fp, data, 1, output_length) < output_length) {
		d_free(data);
		return 0;
	}

	return d1_custom_resample_sound_data(data, output_length, out_data, out_length);
}

static int d1_custom_replace_sound(int sound_index, ubyte *data, int length)
{
	digi_sound *snd;

	if (sound_index < 0 || sound_index >= MAX_SOUND_FILES || !data || length <= 0)
		return 0;

	snd = &GameSounds[sound_index];
	if (Sound_original_valid[sound_index])
		d_free(snd->data);
	else {
		Sound_original[sound_index] = *snd;
		Sound_original_valid[sound_index] = D1_CUSTOM_VALID;
	}

	snd->length = length;
	snd->data = data;
	return 1;
}

static int d1_custom_apply_sound(PHYSFS_file *fp, d1_custom_sound_info *info)
{
	ubyte *data;
	int length;

	if (info->repl_idx < 0 || info->repl_idx >= MAX_SOUND_FILES)
		return 0;
	if (!d1_custom_read_sound_data(fp, info->offset, info->length, info->length, 0, &data, &length))
		return 0;
	if (!d1_custom_replace_sound(info->repl_idx, data, length)) {
		d_free(data);
		return 0;
	}
	return 1;
}

static int d1_custom_load_raw_sound_file(char *filename, int sound_index)
{
	PHYSFS_file *fp;
	ubyte *data;
	int length;
	int file_len;

	fp = PHYSFSX_openReadBuffered(filename);
	if (!fp)
		return 0;

	file_len = (int) PHYSFS_fileLength(fp);
	if (!d1_custom_read_sound_data(fp, 0, file_len, file_len, 0, &data, &length)) {
		PHYSFS_close(fp);
		return 0;
	}
	PHYSFS_close(fp);

	if (!d1_custom_replace_sound(sound_index, data, length)) {
		d_free(data);
		return 0;
	}
	return 1;
}

static int d1_custom_d1_pig_data_start(PHYSFS_file *fp, int *compressed_sounds, int *mac_pig)
{
	int pigsize = (int) PHYSFS_fileLength(fp);

	*compressed_sounds = 0;
	*mac_pig = 0;
	switch (pigsize) {
		case D1_SHARE_BIG_PIGSIZE:
		case D1_SHARE_10_PIGSIZE:
		case D1_SHARE_PIGSIZE:
			*compressed_sounds = 1;
			return 0;
		case D1_10_BIG_PIGSIZE:
		case D1_10_PIGSIZE:
			return 0;
		case D1_MAC_PIGSIZE:
		case D1_MAC_SHARE_PIGSIZE:
			*mac_pig = 1;
			return 0;
		default:
			PHYSFSX_fseek(fp, 0, SEEK_SET);
			return PHYSFSX_readInt(fp);
	}
}

static void d1_custom_load_mac_base_sounds(d1_custom_texture_stats *stats)
{
	PHYSFS_file *array;
	ubyte mac_sounds[MAX_SOUNDS];
	int i;

	array = PHYSFSX_openReadBuffered("Sounds/sounds.array");
	if (!array) {
		stats->base_sound_skipped++;
		return;
	}
	if (PHYSFS_read(array, mac_sounds, MAX_SOUNDS, 1) != 1) {
		PHYSFS_close(array);
		stats->base_sound_skipped++;
		return;
	}
	PHYSFS_close(array);

	for (i = 0; i < MAX_SOUNDS; i++) {
		char soundfile[32];
		int d1_sound_index = mac_sounds[i];
		int d2_sound_index = Sounds[i];

		if (d1_sound_index == 255 || d2_sound_index == 255)
			continue;

		stats->base_sound_entries++;
		sprintf(soundfile, "Sounds/SND%04d.raw", d1_sound_index);
		if (d1_custom_load_raw_sound_file(soundfile, d2_sound_index))
			stats->base_sound_applied++;
		else
			stats->base_sound_skipped++;
	}
}

static void d1_custom_load_base_sounds(d1_custom_texture_stats *stats)
{
	PHYSFS_file *fp;
	int compressed_sounds, mac_pig;
	int pig_data_start, num_bitmaps, num_sounds;
	int sound_header_start, sound_data_start;
	int i;

	fp = PHYSFSX_openReadBuffered(D1_PIGFILE);
	if (!fp)
		return;

	pig_data_start = d1_custom_d1_pig_data_start(fp, &compressed_sounds, &mac_pig);
	if (mac_pig) {
		PHYSFS_close(fp);
		d1_custom_load_mac_base_sounds(stats);
		return;
	}

	PHYSFSX_fseek(fp, pig_data_start, SEEK_SET);
	num_bitmaps = PHYSFSX_readInt(fp);
	num_sounds = PHYSFSX_readInt(fp);
	if (num_bitmaps < 0 || num_bitmaps > MAX_BITMAP_FILES || num_sounds < 0 || num_sounds > MAX_SOUND_FILES) {
		PHYSFS_close(fp);
		return;
	}

	sound_header_start = pig_data_start + 8 + num_bitmaps * D1_CUSTOM_PIG1_BITMAP_HEADER_SIZE;
	sound_data_start = sound_header_start + num_sounds * D1_CUSTOM_PIG1_SOUND_HEADER_SIZE;
	PHYSFSX_fseek(fp, sound_header_start, SEEK_SET);

	for (i = 0; i < num_sounds; i++) {
		d1_custom_sound_header sndh;
		char name[9];
		int repl_idx;
		ubyte *data;
		int length;
		long next_header_pos;

		if (!d1_custom_read_sound_header(fp, &sndh))
			break;
		next_header_pos = (long) PHYSFS_tell(fp);

		stats->base_sound_entries++;
		memcpy(name, sndh.name, 8);
		name[8] = 0;
		repl_idx = hashtable_search(&AllDigiSndNames, name);
		if (repl_idx < 0) {
			stats->base_sound_unresolved++;
			continue;
		}

		if (compressed_sounds && sndh.data_length <= 0) {
			stats->base_sound_skipped++;
			continue;
		}

		if (!d1_custom_read_sound_data(fp, sound_data_start + sndh.offset,
		                               compressed_sounds ? sndh.data_length : sndh.length, sndh.length,
		                               compressed_sounds && sndh.data_length != sndh.length, &data, &length)) {
			stats->base_sound_skipped++;
			PHYSFSX_fseek(fp, next_header_pos, SEEK_SET);
			continue;
		}
		if (d1_custom_replace_sound(repl_idx, data, length))
			stats->base_sound_applied++;
		else {
			d_free(data);
			stats->base_sound_skipped++;
		}
		PHYSFSX_fseek(fp, next_header_pos, SEEK_SET);
	}

	PHYSFS_close(fp);
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
	d1_custom_sound_info *sound_info = NULL;

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

	if (num_bitmaps) {
		MALLOC(bitmap_info, d1_custom_bitmap_info, num_bitmaps);
		if (!bitmap_info) {
			PHYSFS_close(fp);
			return 0;
		}
	}

	for (i = 0; i < num_bitmaps; i++) {
		d1_custom_bitmap_header bmh;
		char name[15];

		if (!d1_custom_read_bitmap_header(fp, &bmh)) {
			if (bitmap_info)
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

	if (num_sounds) {
		MALLOC(sound_info, d1_custom_sound_info, num_sounds);
		if (!sound_info) {
			if (bitmap_info)
				d_free(bitmap_info);
			PHYSFS_close(fp);
			return 0;
		}
	}

	for (i = 0; i < num_sounds; i++) {
		d1_custom_sound_header sndh;
		char name[9];

		if (!d1_custom_read_sound_header(fp, &sndh)) {
			if (sound_info)
				d_free(sound_info);
			if (bitmap_info)
				d_free(bitmap_info);
			PHYSFS_close(fp);
			return 0;
		}

		memcpy(name, sndh.name, 8);
		name[8] = 0;
		sound_info[i].offset = sndh.offset + data_ofs;
		sound_info[i].repl_idx = hashtable_search(&AllDigiSndNames, name);
		sound_info[i].length = sndh.length;
		stats->sound_entries++;
		if (sound_info[i].repl_idx < 0)
			stats->sound_unresolved++;
	}

	for (i = 0; i < num_bitmaps; i++) {
		if (d1_custom_apply_bitmap(fp, &bitmap_info[i])) {
			stats->bitmap_applied++;
			applied++;
		}
	}

	for (i = 0; i < num_sounds; i++) {
		if (d1_custom_apply_sound(fp, &sound_info[i]))
			stats->sound_applied++;
	}

	if (sound_info)
		d_free(sound_info);
	if (bitmap_info)
		d_free(bitmap_info);
	PHYSFS_close(fp);
	return applied;
}

void d1_custom_remove(void)
{
	int i;
	int sound_changed = 0;

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

	for (i = 0; i < MAX_SOUND_FILES; i++) {
		if (!Sound_original_valid[i])
			continue;

		d_free(GameSounds[i].data);
		GameSounds[i] = Sound_original[i];
		Sound_original_valid[i] = 0;
		sound_changed = 1;
	}
	if (sound_changed)
		digi_free_cached_sounds();
}

void d1_custom_load_data(char *level_name)
{
	char custom_file[FILENAME_LEN];
	d1_custom_texture_stats stats;

	memset(&stats, 0, sizeof(stats));
	d1_custom_remove();
	d1_custom_load_base_sounds(&stats);

	change_filename_extension(custom_file, level_name, ".pg1");
	d1_custom_load_file(custom_file, &stats);
	change_filename_extension(custom_file, level_name, ".dtx");
	d1_custom_load_file(custom_file, &stats);

	if (stats.files_found || stats.base_sound_applied) {
		con_printf(CON_NORMAL,
		           "D1 custom textures: files=%d bitmaps=%d applied=%d unresolved=%d sounds=%d sound_applied=%d sound_unresolved=%d base_sound_applied=%d base_sound_unresolved=%d base_sound_skipped=%d\n",
		           stats.files_found, stats.bitmap_entries, stats.bitmap_applied, stats.bitmap_unresolved, stats.sound_entries,
		           stats.sound_applied, stats.sound_unresolved, stats.base_sound_applied, stats.base_sound_unresolved,
		           stats.base_sound_skipped);
		if (stats.files_found)
			texmerge_flush();
		if (stats.sound_applied || stats.base_sound_applied)
			digi_free_cached_sounds();
	}
	Last_stats = stats;
}

void d1_custom_get_stats(d1_custom_texture_stats *stats)
{
	if (stats)
		*stats = Last_stats;
}
