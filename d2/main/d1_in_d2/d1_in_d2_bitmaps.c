/*
THE COMPUTER CODE CONTAINED HEREIN IS THE SOLE PROPERTY OF PARALLAX
SOFTWARE CORPORATION ("PARALLAX").  PARALLAX, IN DISTRIBUTING THE CODE TO
END-USERS, AND SUBJECT TO ALL OF THE TERMS AND CONDITIONS HEREIN, GRANTS A
ROYALTY-FREE, PERPETUAL LICENSE TO SUCH END-USERS FOR USE BY SUCH END-USERS
IN USING, DISPLAYING,  AND CREATING DERIVATIVE WORKS THEREOF, SO LONG AS
SUCH USE, DISPLAY OR CREATION IS FOR NON-COMMERCIAL, ROYALTY OR REVENUE
FREE PURPOSES.  IN NO EVENT SHALL THE END-USER USE THE COMPUTER CODE
CONTAINED HEREIN FOR REVENUE-BEARING PURPOSES.  THE END-USER UNDERSTANDS
AND AGREES TO THE TERMS HEREIN AND ACCEPTS THE SAME BY USE OF THIS FILE.
COPYRIGHT 1993-1999 PARALLAX SOFTWARE CORPORATION.  ALL RIGHTS RESERVED.
*/

/* D1 bitmap decoding and legacy replacement mapping, extracted from piggy.c */

#include <stdio.h>
#include <string.h>
#include "pstypes.h"
#include "inferno.h"
#include "strutil.h"
#include "gr.h"
#include "grdef.h"
#include "u_mem.h"
#include "dxxerror.h"
#include "bm.h"
#include "palette.h"
#include "gamepal.h"
#include "rle.h"
#include "piggy.h"
#include "gamemine.h"
#include "d1_in_d2_levels.h"
#include "gamesave.h"
#include "textures.h"
#include "texmerge.h"
#include "text.h"
#include "byteswap.h"
#include "makesig.h"
#include "console.h"
#include "effects.h"
#include "d1_pig_validation.h"
#include "d1_in_d2_bitmaps.h"
#include "d1_in_d2.h"

#define D1_PALETTE "palette.256"
#define DBM_FLAG_ABM 64
#define DISKBITMAPHEADER_D1_SIZE 17
#define BM_FLAGS_TO_COPY (BM_FLAG_TRANSPARENT | BM_FLAG_SUPER_TRANSPARENT \
                         | BM_FLAG_NO_LIGHTING | BM_FLAG_RLE | BM_FLAG_RLE_BIG)

/* The legacy POG/replacement arena is still released by piggy */
extern ubyte *Bitmap_replacement_data;
static ubyte *Bitmap_replacement_next;
static ubyte *Bitmap_replacement_end;
void swap_0_255(grs_bitmap *bmp);

typedef struct DiskBitmapHeader {
	char name[8];
	ubyte dflags;           // bits 0-5 anim frame num, bit 6 abm flag
	ubyte width;            // low 8 bits here, 4 more bits in wh_extra
	ubyte height;           // low 8 bits here, 4 more bits in wh_extra
	ubyte wh_extra;         // bits 0-3 width, bits 4-7 height
	ubyte flags;
	ubyte avg_color;
	int offset;
} __pack__ DiskBitmapHeader;


typedef struct DiskSoundHeader {
	char name[8];
	int length;
	int data_length;
	int offset;
} __pack__ DiskSoundHeader;

/*
 * reads a descent 1 DiskBitmapHeader structure from a PHYSFS_file
 */
static void DiskBitmapHeader_d1_read(DiskBitmapHeader *dbh, PHYSFS_file *fp)
{
	PHYSFS_read(fp, dbh->name, 8, 1);
	dbh->dflags = PHYSFSX_readByte(fp);
	dbh->width = PHYSFSX_readByte(fp);
	dbh->height = PHYSFSX_readByte(fp);
	dbh->wh_extra = 0;
	dbh->flags = PHYSFSX_readByte(fp);
	dbh->avg_color = PHYSFSX_readByte(fp);
	dbh->offset = PHYSFSX_readInt(fp);
}

static struct {
	ubyte *data;
	int offset;
	ubyte flags, avg_color;
} D1_light_bitmap_backups[MAX_BITMAP_FILES];
void d1_in_d2_reset_bitmap_replacements(void)
{
	// D1 levels can reuse the same PIG; restore paging before freeing converted lights
	for (int i = 1; i < MAX_BITMAP_FILES; i++) {
		grs_bitmap *bmp = &GameBitmaps[i];
		if (D1_light_bitmap_backups[i].data && bmp->bm_data == D1_light_bitmap_backups[i].data &&
		    piggy_bitmap_get_offset(i) == 0) {
			piggy_bitmap_set_file_state(i, D1_light_bitmap_backups[i].offset, D1_light_bitmap_backups[i].flags);
			bmp->avg_color = D1_light_bitmap_backups[i].avg_color;
			gr_set_bitmap_flags(bmp, BM_FLAG_PAGED_OUT);
			gr_set_bitmap_data(bmp, Piggy_bitmap_cache_data);
		}
		D1_light_bitmap_backups[i].data = NULL;
	}
	Bitmap_replacement_next = NULL;
	Bitmap_replacement_end = NULL;
}

/* calculate table to translate d1 bitmaps to current palette,
 * return -1 on error
 */
static int get_d1_colormap( ubyte *d1_palette, ubyte *colormap )
{
	int freq[256];
	PHYSFS_file * palette_file = PHYSFSX_openReadBuffered(D1_PALETTE);
	if (!palette_file || PHYSFS_fileLength(palette_file) != 9472)
		return -1;
	PHYSFS_read( palette_file, d1_palette, 256, 3 );
	PHYSFS_close( palette_file );
	build_colormap_good( d1_palette, colormap, freq );
	// don't change transparencies:
	colormap[254] = 254;
	colormap[255] = 255;
	return 0;
}

static int bitmap_read_source( grs_bitmap *bitmap, /* read into this bitmap */
                     PHYSFS_file *d1_Piggy_fp, /* read from this file */
                     int bitmap_data_start, /* specific to file */
                     DiskBitmapHeader *bmh, /* header info for bitmap */
                     ubyte **next_bitmap, /* where to write it (if 0, use malloc) */
		     ubyte *bitmap_end, /* end of caller-owned replacement storage */
		     ubyte *d1_palette, /* what palette the bitmap has */
                     int preserve_palette, /* native generations retain D1 indices */
                     ubyte *colormap, /* how to translate bitmap's colors */
                     int mac_data)
{
	PHYSFS_sint64 bitmap_offset, pigsize = PHYSFS_fileLength(d1_Piggy_fp);
	int zsize, new_size, width, height;
	size_t remapped_size = 0, remap_limit, work_size;
	ubyte *data, *final_data;
	ubyte mac_colormap[256];
	ubyte *remap_colormap = colormap;
	grs_bitmap staged_bitmap;

	width = bmh->width + ((short) (bmh->wh_extra & 0x0f) << 8);
	height = bmh->height + ((short) (bmh->wh_extra & 0xf0) << 4);
	if (bitmap_data_start < 0 || bmh->offset < 0 || width <= 0 || height <= 0)
		return 0;
	bitmap_offset = (PHYSFS_sint64)bitmap_data_start + bmh->offset;
	if (!d1_pig_validate_span(pigsize, bitmap_offset, 0))
		return 0;
	if (PHYSFSX_fseek(d1_Piggy_fp, (long)bitmap_offset, SEEK_SET))
		return 0;
	if (bmh->flags & BM_FLAG_RLE) {
		if (pigsize - bitmap_offset < (PHYSFS_sint64)sizeof(int))
			return 0;
		zsize = PHYSFSX_readInt(d1_Piggy_fp);
		if (PHYSFSX_fseek(d1_Piggy_fp, -(long)sizeof(int), SEEK_CUR))
			return 0;
	} else {
		PHYSFS_sint64 uncompressed_size = (PHYSFS_sint64)width * height;
		if (uncompressed_size <= 0 || uncompressed_size > 0x7fffffff)
			return 0;
		zsize = (int)uncompressed_size;
	}
	if (!d1_pig_validate_span(pigsize, bitmap_offset, zsize))
		return 0;
	work_size = zsize;
	data = d_malloc(work_size);
	if (!data)
		return 0;

	if (PHYSFS_read(d1_Piggy_fp, data, 1, zsize) != zsize) {
		d_free(data);
		return 0;
	}
	gr_init_bitmap(&staged_bitmap, 0, 0, 0, width, height, width, data);
	staged_bitmap.avg_color = bmh->avg_color;
	gr_set_bitmap_flags(&staged_bitmap, bmh->flags & BM_FLAGS_TO_COPY);
	if (bmh->flags & BM_FLAG_RLE) {
		if (!d1_pig_validate_rle(data, zsize, width, height, bmh->flags & BM_FLAG_RLE_BIG) ||
		    (size_t)height > (SIZE_MAX - 4 - 30000) / ((size_t)width + 2)) {
			d_free(data);
			return 0;
		}
		remap_limit = 4 + ((size_t)width + 2) * height + 30000;
	}
	if (mac_data) {
		if (bmh->flags & BM_FLAG_RLE) {
			memcpy(mac_colormap, colormap, sizeof(mac_colormap));
			mac_colormap[0] = colormap[255];
			mac_colormap[255] = colormap[0];
			remap_colormap = mac_colormap;
		} else
			swap_0_255(&staged_bitmap);
	}
	if ((bmh->flags & BM_FLAG_RLE) && (!preserve_palette || remap_colormap == mac_colormap)) {
		if (!d1_pig_measure_remapped_rle(data, zsize, width, height,
		                                   bmh->flags & BM_FLAG_RLE_BIG,
		                                   remap_colormap, &remapped_size) ||
		    remapped_size > remap_limit) {
			d_free(data);
			return 0;
		}
		if (remapped_size > work_size) {
			ubyte *grown_data = d_realloc(data, remapped_size);
			if (!grown_data) {
				d_free(data);
				return 0;
			}
			data = grown_data;
			staged_bitmap.bm_data = data;
			work_size = remapped_size;
		}
		rle_remap(&staged_bitmap, remap_colormap);
	} else if (!(bmh->flags & BM_FLAG_RLE) && !preserve_palette)
		gr_remap_bitmap_good(&staged_bitmap, d1_palette, TRANSPARENCY_COLOR, -1);
	if (bmh->flags & BM_FLAG_RLE) { // size of bitmap could have changed!
		memcpy(&new_size, staged_bitmap.bm_data, sizeof(new_size));
		if (new_size <= 0 || (size_t)new_size > work_size) {
			d_free(data);
			return 0;
		}
	} else
		new_size = zsize;
	if (next_bitmap) {
		if (!*next_bitmap || !bitmap_end || *next_bitmap > bitmap_end ||
		    !d1_pig_validate_arena((size_t)(bitmap_end - *next_bitmap), new_size, 0)) {
			d_free(data);
			return 0;
		}
		final_data = *next_bitmap;
		memcpy(final_data, data, new_size);
		*next_bitmap += new_size;
		d_free(data);
	} else {
		final_data = d_realloc(data, new_size);
		if (!final_data)
			final_data = data;
	}
	staged_bitmap.bm_data = final_data;
	gr_set_bitmap_data(bitmap, NULL);	// free ogl texture
	*bitmap = staged_bitmap;
	return 1;
}

static int bitmap_read_d1(grs_bitmap *bitmap, PHYSFS_file *fp, int data_start,
	DiskBitmapHeader *header, ubyte **next, ubyte *end, ubyte *palette,
	int preserve_palette, ubyte *colormap)
{
	PHYSFS_sint64 size = PHYSFS_fileLength(fp);
	return bitmap_read_source(bitmap, fp, data_start, header, next, end, palette,
		preserve_palette, colormap, size == D1_MAC_PIGSIZE || size == D1_MAC_SHARE_PIGSIZE);
}

#define D1_MAX_TEXTURES 800
#define D1_MAX_TMAP_NUM 1630 // 1621 in descent.pig Mac registered

void d1_in_d2_free_bitmaps(d1_bitmap_generation *generation)
{
	int i;
	if (!generation)
		return;
	if (generation->bitmaps) {
		for (i = 0; i <= generation->bitmap_count; i++) {
			ubyte *data = generation->bitmaps[i].bm_data;
			gr_set_bitmap_data(&generation->bitmaps[i], NULL);
			if (data)
				d_free(data);
		}
		d_free(generation->bitmaps);
	}
	if (generation->names)
		d_free(generation->names);
	d_free(generation);
}

d1_bitmap_generation *d1_in_d2_read_bitmaps(const char *pig_name, const char *palette_name)
{
	PHYSFS_file *fp = NULL, *palette_file = NULL;
	PHYSFS_sint64 size, directory_size, data_start;
	d1_bitmap_generation *generation = NULL;
	int directory, count, sound_count, i;
	ubyte identity[256];
	if (!pig_name || !palette_name)
		return NULL;
	fp = PHYSFSX_openReadBuffered(pig_name);
	palette_file = PHYSFSX_openReadBuffered(palette_name);
	if (!fp || !palette_file || PHYSFS_fileLength(palette_file) != 9472)
		goto failed;
	size = PHYSFS_fileLength(fp);
	if (size < 8 || size > 0x7fffffff)
		goto failed;
	switch (size) {
	case D1_SHARE_BIG_PIGSIZE:
	case D1_SHARE_10_PIGSIZE:
	case D1_SHARE_PIGSIZE:
	case D1_10_BIG_PIGSIZE:
	case D1_10_PIGSIZE:
		directory = 0;
		break;
	default:
		directory = PHYSFSX_readInt(fp);
		break;
	}
	if (!d1_pig_validate_span(size, directory, 8) ||
	    PHYSFSX_fseek(fp, directory, SEEK_SET))
		goto failed;
	count = PHYSFSX_readInt(fp);
	sound_count = PHYSFSX_readInt(fp);
	if (count <= 0 || count > D1_MAX_TMAP_NUM || sound_count < 0 || sound_count > MAX_SOUND_FILES)
		goto failed;
	directory_size = (PHYSFS_sint64)count * DISKBITMAPHEADER_D1_SIZE +
	                 (PHYSFS_sint64)sound_count * sizeof(DiskSoundHeader);
	if (!d1_pig_validate_span(size, (PHYSFS_sint64)directory + 8, directory_size))
		goto failed;
	data_start = directory + 8 + directory_size;
	generation = d_malloc(sizeof(*generation));
	if (!generation)
		goto failed;
	memset(generation, 0, sizeof(*generation));
	generation->bitmap_count = count;
	generation->bitmaps = d_malloc((count + 1) * sizeof(*generation->bitmaps));
	if (!generation->bitmaps)
		goto failed;
	memset(generation->bitmaps, 0, (count + 1) * sizeof(*generation->bitmaps));
	generation->names = d_malloc((count + 1) * sizeof(*generation->names));
	if (!generation->names)
		goto failed;
	memset(generation->names, 0, (count + 1) * sizeof(*generation->names));
	if (PHYSFS_read(palette_file, generation->palette, sizeof(generation->palette), 1) != 1 ||
	    PHYSFS_read(palette_file, generation->fade_table, sizeof(generation->fade_table), 1) != 1)
		goto failed;
	for (i = 0; i < 256; i++)
		identity[i] = i;
	for (i = 1; i <= count; i++) {
		DiskBitmapHeader header;
		char name[9];
		if (PHYSFSX_fseek(fp, directory + 8 + (i - 1) * DISKBITMAPHEADER_D1_SIZE, SEEK_SET))
			goto failed;
		DiskBitmapHeader_d1_read(&header, fp);
		/* D1's large-width bit is required by original 320-pixel cockpit art */
		header.wh_extra = (header.dflags & 128) ? 1 : 0;
		memcpy(name, header.name, 8);
		name[8] = 0;
		if (header.dflags & DBM_FLAG_ABM)
			snprintf(generation->names[i], sizeof(generation->names[i]), "%s#%d", name, header.dflags & 63);
		else
			snprintf(generation->names[i], sizeof(generation->names[i]), "%s", name);
		if (!bitmap_read_d1(&generation->bitmaps[i], fp, (int)data_start, &header,
		                    NULL, NULL, generation->palette, 1, identity))
			goto failed;
	}
	PHYSFS_close(palette_file);
	PHYSFS_close(fp);
	return generation;

failed:
	if (palette_file)
		PHYSFS_close(palette_file);
	if (fp)
		PHYSFS_close(fp);
	d1_in_d2_free_bitmaps(generation);
	return NULL;
}

/* Optional D2 images use the same validated pixel decoder as native images
 * Source selection and lifetime remain in the compatibility asset owner */
d1_bitmap_generation *d1_in_d2_read_feature_bitmaps(const char *pig_name, const char *palette_name,
	const ubyte *selected, int selected_count)
{
	PHYSFS_file *fp = NULL, *palette_file = NULL;
	d1_bitmap_generation *generation = NULL;
	PHYSFS_sint64 size, data_start;
	int count, i;
	ubyte identity[256];
	if (!pig_name || !palette_name || !selected || selected_count <= 0 || selected_count > MAX_BITMAP_FILES)
		return NULL;
	fp = PHYSFSX_openReadBuffered(pig_name);
	palette_file = PHYSFSX_openReadBuffered(palette_name);
	if (!fp || !palette_file || PHYSFS_fileLength(palette_file) != 9472)
		goto failed;
	size = PHYSFS_fileLength(fp);
	if (size < 12 || size > 0x7fffffff || PHYSFSX_readInt(fp) != MAKE_SIG('G','I','P','P') ||
	    PHYSFSX_readInt(fp) != 2)
		goto failed;
	count = PHYSFSX_readInt(fp);
	if (count <= 0 || count >= MAX_BITMAP_FILES ||
	    !d1_pig_validate_span(size, 12, (PHYSFS_sint64)count * 18))
		goto failed;
	for (i = count + 1; i < selected_count; i++)
		if (selected[i])
			goto failed;
	data_start = 12 + (PHYSFS_sint64)count * 18;
	generation = d_malloc(sizeof(*generation));
	if (!generation)
		goto failed;
	memset(generation, 0, sizeof(*generation));
	generation->bitmap_count = count;
	generation->bitmaps = d_malloc((count + 1) * sizeof(*generation->bitmaps));
	if (!generation->bitmaps)
		goto failed;
	memset(generation->bitmaps, 0, (count + 1) * sizeof(*generation->bitmaps));
	generation->names = d_malloc((count + 1) * sizeof(*generation->names));
	if (!generation->names)
		goto failed;
	memset(generation->names, 0, (count + 1) * sizeof(*generation->names));
	if (PHYSFS_read(palette_file, generation->palette, sizeof(generation->palette), 1) != 1 ||
	    PHYSFS_read(palette_file, generation->fade_table, sizeof(generation->fade_table), 1) != 1)
		goto failed;
	for (i = 0; i < 768; i++)
		if (generation->palette[i] > 63)
			goto failed;
	for (i = 0; i < 256; i++)
		identity[i] = i;
	for (i = 1; i <= count && i < selected_count; i++) {
		DiskBitmapHeader header;
		char name[9];
		if (!selected[i])
			continue;
		if (!PHYSFS_seek(fp, 12 + (i - 1) * 18))
			goto failed;
		if (PHYSFS_read(fp, header.name, 8, 1) != 1)
			goto failed;
		header.dflags = PHYSFSX_readByte(fp);
		header.width = PHYSFSX_readByte(fp);
		header.height = PHYSFSX_readByte(fp);
		header.wh_extra = PHYSFSX_readByte(fp);
		header.flags = PHYSFSX_readByte(fp);
		header.avg_color = PHYSFSX_readByte(fp);
		header.offset = PHYSFSX_readInt(fp);
		memcpy(name, header.name, 8);
		name[8] = 0;
		if (header.dflags & DBM_FLAG_ABM)
			snprintf(generation->names[i], sizeof(generation->names[i]), "%s#%d", name, header.dflags & 63);
		else
			snprintf(generation->names[i], sizeof(generation->names[i]), "%s", name);
		if (!name[0] || !bitmap_read_source(&generation->bitmaps[i], fp, (int)data_start, &header,
			NULL, NULL, generation->palette, 1, identity, 0))
			goto failed;
	}
	PHYSFS_close(palette_file);
	PHYSFS_close(fp);
	return generation;
failed:
	if (palette_file)
		PHYSFS_close(palette_file);
	if (fp)
		PHYSFS_close(fp);
	d1_in_d2_free_bitmaps(generation);
	return NULL;
}

/* Only unpublished owned images enter here; live palette state is irrelevant */
int d1_in_d2_remap_feature_bitmaps(d1_bitmap_generation *images, const ubyte *palette)
{
	ubyte map[256];
	int i, j;
	if (!images || !palette)
		return 0;
	for (i = 0; i < 254; i++) {
		int best = 0, distance = 0x7fffffff;
		for (j = 0; j < 254; j++) {
			int r = images->palette[i * 3] - palette[j * 3];
			int g = images->palette[i * 3 + 1] - palette[j * 3 + 1];
			int b = images->palette[i * 3 + 2] - palette[j * 3 + 2];
			int candidate = r*r + g*g + b*b;
			if (candidate < distance) {
				best = j;
				distance = candidate;
			}
		}
		map[i] = best;
	}
	map[254] = 254;
	map[255] = 255;
	for (i = 1; i <= images->bitmap_count; i++) {
		grs_bitmap *bitmap = &images->bitmaps[i];
		if (!bitmap->bm_data)
			continue;
		if (bitmap->bm_flags & BM_FLAG_RLE) {
			int size;
			size_t output_size;
			ubyte *output;
			memcpy(&size, bitmap->bm_data, sizeof(size));
			if (size <= 0 || !d1_pig_measure_remapped_rle(bitmap->bm_data, size, bitmap->bm_w, bitmap->bm_h,
				bitmap->bm_flags & BM_FLAG_RLE_BIG, map, &output_size))
				return 0;
			output = d_realloc(bitmap->bm_data, max(output_size, (size_t)size));
			if (!output)
				return 0;
			bitmap->bm_data = output;
			rle_remap(bitmap, map);
		} else
			for (j = 0; j < bitmap->bm_rowsize * bitmap->bm_h; j++)
				bitmap->bm_data[j] = map[bitmap->bm_data[j]];
		bitmap->avg_color = map[bitmap->avg_color];
	}
	memcpy(images->palette, palette, sizeof(images->palette));
	return 1;
}

/* the inverse of the d2 Textures array, but for the descent 1 pigfile.
 * "Textures" looks up a d2 bitmap index given a d2 tmap_num.
 * "d1_tmap_nums" looks up a d1 tmap_num given a d1 bitmap. "-1" means "None".
 */
static short *d1_tmap_nums = NULL;
static d1_bitmap_replacement_stats Last_d1_bitmap_replacement_stats;

void free_d1_tmap_nums() {
	if (d1_tmap_nums) {
		d_free(d1_tmap_nums);
		d1_tmap_nums = NULL;
	}
}

void d1_bitmap_replacement_get_stats(d1_bitmap_replacement_stats *stats)
{
	if (stats)
		*stats = Last_d1_bitmap_replacement_stats;
}

static void bm_read_d1_tmap_nums(PHYSFS_file *d1pig)
{
	int i, d1_index;

	free_d1_tmap_nums();
	PHYSFSX_fseek(d1pig, 8, SEEK_SET);
	MALLOC(d1_tmap_nums, short, D1_MAX_TMAP_NUM);
	for (i = 0; i < D1_MAX_TMAP_NUM; i++)
		d1_tmap_nums[i] = -1;
	for (i = 0; i < D1_MAX_TEXTURES; i++) {
		d1_index = PHYSFSX_readShort(d1pig);
		Assert(d1_index >= 0 && d1_index < D1_MAX_TMAP_NUM);
		d1_tmap_nums[d1_index] = i;
		if (PHYSFS_eof(d1pig))
			break;
	}
}

// this function is at the same position in the d1 shareware piggy loading 
// algorithm as bm_load_sub in main/bmread.c
static int get_d1_bm_index(char *filename, PHYSFS_file *d1_pig) {
	int i, N_bitmaps;
	DiskBitmapHeader bmh;
	if (strchr (filename, '.'))
		*strchr (filename, '.') = '\0'; // remove extension
	PHYSFSX_fseek (d1_pig, 0, SEEK_SET);
	N_bitmaps = PHYSFSX_readInt (d1_pig);
	PHYSFSX_fseek (d1_pig, 8, SEEK_SET);
	for (i = 1; i <= N_bitmaps; i++) {
		DiskBitmapHeader_d1_read(&bmh, d1_pig);
		if (!d_strnicmp(bmh.name, filename, 8))
			return i;
	}
	return -1;
}

// imitate the algorithm of gamedata_read_tbl in main/bmread.c
static void read_d1_tmap_nums_from_hog(PHYSFS_file *d1_pig)
{
#define LINEBUF_SIZE 600
	int reading_textures = 0;
	short texture_count = 0;
	char inputline[LINEBUF_SIZE];
	PHYSFS_file * bitmaps;
	int bitmaps_tbl_is_binary = 0;
	int i;

	bitmaps = PHYSFSX_openReadBuffered ("bitmaps.tbl");
	if (!bitmaps) {
		bitmaps = PHYSFSX_openReadBuffered ("bitmaps.bin");
		bitmaps_tbl_is_binary = 1;
	}

	if (!bitmaps) {
		Warning ("Could not find bitmaps.* for reading d1 textures");
		return;
	}

	free_d1_tmap_nums();
	MALLOC(d1_tmap_nums, short, D1_MAX_TMAP_NUM);
	for (i = 0; i < D1_MAX_TMAP_NUM; i++)
		d1_tmap_nums[i] = -1;

	while (PHYSFSX_fgets (inputline, LINEBUF_SIZE, bitmaps)) {
		char *arg;

		if (bitmaps_tbl_is_binary)
			decode_text_line((inputline));
		else
			while (inputline[(i=strlen(inputline))-2]=='\\')
				PHYSFSX_fgets(inputline+i-2,LINEBUF_SIZE-(i-2), bitmaps); // strip comments
		REMOVE_EOL(inputline);
                if (strchr(inputline, ';')!=NULL) REMOVE_COMMENTS(inputline);
		if (strlen(inputline) == LINEBUF_SIZE-1) {
			Warning("Possible line truncation in BITMAPS.TBL");
			return;
		}
		arg = strtok( inputline, space );
                if (arg && arg[0] == '@') {
			arg++;
			//Registered_only = 1;
		}

                while (arg != NULL) {
			if (*arg == '$')
				reading_textures = 0; // default
			if (!strcmp(arg, "$TEXTURES")) // BM_TEXTURES
				reading_textures = 1;
			else if (! d_stricmp(arg, "$ECLIP") // BM_ECLIP
				   || ! d_stricmp(arg, "$WCLIP")) // BM_WCLIP
					texture_count++;
			else // not a special token, must be a bitmap!
				if (reading_textures) {
					while (*arg == '\t' || *arg == ' ')
						arg++;//remove unwanted blanks
					if (*arg == '\0')
						break;
					{
						int d1_index = get_d1_bm_index(arg, d1_pig);
						if (d1_index >= 0 && d1_index < D1_MAX_TMAP_NUM)
							d1_tmap_nums[d1_index] = texture_count;
					}
				Assert (texture_count < D1_MAX_TEXTURES);
				texture_count++;
			}

			arg = strtok (NULL, equal_space);
		}
	}
	PHYSFS_close (bitmaps);
}

/* If the given d1_index is used by a D1 texture, returns the D2 bitmap index
 * that should be replaced while emulating D1.
 * Returns -1 if the given d1_index is not used by a D1 texture.
 */
int d2_index_for_d1_index(int d1_index)
{
	int d2_tmap_num;
	int d2_bitmap_index;
	const char *extension = strrchr(Gamesave_current_filename, '.');
	if (d1_in_d2_has_native_assets())
		return d1_index > 0 && d1_index < Num_bitmap_files ? d1_index : -1;

	if (d1_index < 0 || d1_index >= D1_MAX_TMAP_NUM || !d1_tmap_nums || d1_tmap_nums[d1_index] < 0)
		return -1;
	/* Legacy bitmap replacement follows the source level's RDL/SDL layout */
	d2_tmap_num = d1_in_d2_legacy_texture(d1_tmap_nums[d1_index], 1,
	                                    !extension || strcmp(extension, ".sdl"));
	if (d2_tmap_num < 0 || d2_tmap_num >= NumTextures)
		return -1;
	d2_bitmap_index = Textures[d2_tmap_num].index;
	if (d2_bitmap_index < 0 || d2_bitmap_index >= MAX_BITMAP_FILES)
		return -1;

	return d2_bitmap_index;
}

#define D1_BITMAPS_SIZE (5 * 1024 * 1024)

static void remap_d1_destroyed_lights(ubyte *protected_bitmaps)
{
	ubyte source_palette[256 * 3], colormap[256];
	int freq[256], t;
	char palette_name[FILENAME_LEN], pig_name[FILENAME_LEN];
	PHYSFS_file *palette_file;

	// D1 has no blown lights, so their retained D2 images need palette conversion
	if (piggy_current_pigfile()[0]) {
		snprintf(pig_name, sizeof(pig_name), "%s", piggy_current_pigfile());
		d_splitpath(pig_name, NULL, NULL, palette_name, NULL);
		strcat(palette_name, ".256");
	} else
		strcpy(palette_name, DEFAULT_LEVEL_PALETTE);
	palette_file = PHYSFSX_openReadBuffered(palette_name);
	if (!palette_file)
		return;
	if (PHYSFS_read(palette_file, source_palette, 256, 3) != 3) {
		PHYSFS_close(palette_file);
		return;
	}
	PHYSFS_close(palette_file);
	build_colormap_good(source_palette, colormap, freq);
	colormap[254] = 254;
	colormap[TRANSPARENCY_COLOR] = TRANSPARENCY_COLOR;

	for (t = 0; t < NumTextures && t < MAX_TEXTURES; t++) {
		int dest = TmapInfo[t].destroyed;
		int index, size;
		size_t remapped_size;
		grs_bitmap *bmp, remapped;

		if (dest <= 0 || dest >= NumTextures || dest >= MAX_TEXTURES)
			continue;
		index = Textures[dest].index;
		if (index <= 0 || index >= Num_bitmap_files || index >= MAX_BITMAP_FILES || protected_bitmaps[index])
			continue;
		protected_bitmaps[index] = 1;
		if (piggy_bitmap_get_offset(index) <= 0)
			continue;
		PIGGY_PAGE_IN(Textures[dest]);
		bmp = &GameBitmaps[index];
		if (!bmp->bm_data || bmp->bm_w <= 0 || bmp->bm_h <= 0)
			continue;
		if (bmp->bm_flags & BM_FLAG_RLE) {
			memcpy(&size, bmp->bm_data, sizeof(size));
			if (size <= 0 || !d1_pig_measure_remapped_rle(bmp->bm_data, size, bmp->bm_w, bmp->bm_h,
			                                               bmp->bm_flags & BM_FLAG_RLE_BIG, colormap, &remapped_size))
				continue;
		} else {
			size = bmp->bm_rowsize * bmp->bm_h;
			remapped_size = size;
		}
		if (size <= 0 || (size_t)size > (size_t)(Bitmap_replacement_end - Bitmap_replacement_next) ||
		    remapped_size > (size_t)(Bitmap_replacement_end - Bitmap_replacement_next))
			continue;
		remapped = *bmp;
		remapped.bm_data = Bitmap_replacement_next;
		memcpy(remapped.bm_data, bmp->bm_data, size);
		if (bmp->bm_flags & BM_FLAG_RLE)
			rle_remap(&remapped, colormap);
		else
			for (int p = 0; p < size; p++)
				remapped.bm_data[p] = colormap[remapped.bm_data[p]];
		remapped.avg_color = colormap[bmp->avg_color];
#ifdef OGL
		remapped.gltexture = NULL;
		remapped.gltexture_mask = NULL;
#endif
		D1_light_bitmap_backups[index].data = remapped.bm_data;
		D1_light_bitmap_backups[index].offset = piggy_bitmap_get_offset(index);
		D1_light_bitmap_backups[index].flags = piggy_bitmap_get_file_flags(index);
		D1_light_bitmap_backups[index].avg_color = bmp->avg_color;
		gr_set_bitmap_data(bmp, NULL);
		*bmp = remapped;
		piggy_bitmap_set_file_state(index, 0, bmp->bm_flags);
		Bitmap_replacement_next += remapped_size;
	}
}

void load_d1_bitmap_replacements()
{
	PHYSFS_file * d1_Piggy_fp;
	DiskBitmapHeader bmh;
	int pig_data_start, bitmap_header_start, bitmap_data_start;
	int N_bitmaps;
	PHYSFS_sint64 header_size;
	short d1_index;
	int d2_index;
	ubyte colormap[256];
	ubyte d1_palette[256*3];
	char *p;
	int pigsize;
	ubyte is_effect[MAX_BITMAP_FILES] = {0};

	memset(&Last_d1_bitmap_replacement_stats, 0, sizeof(Last_d1_bitmap_replacement_stats));
	d1_Piggy_fp = PHYSFSX_openReadBuffered( D1_PIGFILE );

#define D1_PIG_LOAD_FAILED "Failed loading " D1_PIGFILE
	if (!d1_Piggy_fp) {
		Warning(D1_PIG_LOAD_FAILED);
		return;
	}
	Last_d1_bitmap_replacement_stats.pig_present = 1;

	//first, free up data allocated for old bitmaps
	free_bitmap_replacements();

	if (get_d1_colormap( d1_palette, colormap ) != 0) {
		Last_d1_bitmap_replacement_stats.colormap_failed = 1;
		Warning("Could not load descent 1 color palette");
	}

	pigsize = PHYSFS_fileLength(d1_Piggy_fp);
	Last_d1_bitmap_replacement_stats.pig_size = pigsize;
	switch (pigsize) {
	case D1_SHARE_BIG_PIGSIZE:
	case D1_SHARE_10_PIGSIZE:
	case D1_SHARE_PIGSIZE:
	case D1_10_BIG_PIGSIZE:
	case D1_10_PIGSIZE:
		pig_data_start = 0;
		// OK, now we need to read d1_tmap_nums by emulating d1's gamedata_read_tbl()
		read_d1_tmap_nums_from_hog(d1_Piggy_fp);
		break;

	case D1_PIGSIZE:
	case D1_OEM_PIGSIZE:
	case D1_MAC_PIGSIZE:
	case D1_MAC_SHARE_PIGSIZE:
		pig_data_start = PHYSFSX_readInt(d1_Piggy_fp );
		bm_read_d1_tmap_nums(d1_Piggy_fp); //was: bm_read_all_d1(fp);
		//for (i = 0; i < 1800; i++) GameBitmapXlat[i] = PHYSFSX_readShort(d1_Piggy_fp);
		break;

	default:
		pig_data_start = PHYSFSX_readInt(d1_Piggy_fp );
		bm_read_d1_tmap_nums(d1_Piggy_fp);	
	}

	if (pig_data_start < 0 || pig_data_start > pigsize - 2 * (int)sizeof(int) ||
	    PHYSFSX_fseek(d1_Piggy_fp, pig_data_start, SEEK_SET)) {
		PHYSFS_close(d1_Piggy_fp);
		Warning(D1_PIG_LOAD_FAILED ": invalid data offset");
		return;
	}
	N_bitmaps = PHYSFSX_readInt(d1_Piggy_fp);
	{
		int N_sounds = PHYSFSX_readInt(d1_Piggy_fp);
		header_size = (PHYSFS_sint64)N_bitmaps * DISKBITMAPHEADER_D1_SIZE
			+ (PHYSFS_sint64)N_sounds * sizeof(DiskSoundHeader);
		bitmap_header_start = pig_data_start + 2 * sizeof(int);
		if (pig_data_start < 0 || N_bitmaps < 0 || N_bitmaps > D1_MAX_TMAP_NUM ||
		    N_sounds < 0 || header_size < 0 ||
		    (PHYSFS_sint64)bitmap_header_start + header_size > pigsize) {
			PHYSFS_close(d1_Piggy_fp);
			Warning(D1_PIG_LOAD_FAILED ": invalid bitmap table");
			return;
		}
		bitmap_data_start = bitmap_header_start + (int)header_size;
	}

	MALLOC( Bitmap_replacement_data, ubyte, D1_BITMAPS_SIZE);
	if (!Bitmap_replacement_data) {
		PHYSFS_close(d1_Piggy_fp);
		Warning(D1_PIG_LOAD_FAILED);
		return;
	}

	// Leave effect frames and destroyed monitors to their dedicated asset paths
	for (int ei = 0; ei < Num_effects && ei < MAX_EFFECTS; ei++) {
		eclip *e = &Effects[ei];
		for (int i = 0; i < e->vc.num_frames && i < VCLIP_MAX_FRAMES; i++)
			if (e->vc.frames[i].index < MAX_BITMAP_FILES)
				is_effect[e->vc.frames[i].index] = 1;
		if (e->dest_bm_num >= 0 && e->dest_bm_num < NumTextures && e->dest_bm_num < MAX_TEXTURES &&
		    Textures[e->dest_bm_num].index < MAX_BITMAP_FILES)
			is_effect[Textures[e->dest_bm_num].index] = 1;
	}

	Bitmap_replacement_next = Bitmap_replacement_data;
	Bitmap_replacement_end = Bitmap_replacement_data + D1_BITMAPS_SIZE;
	remap_d1_destroyed_lights(is_effect);

	for (d1_index = 1; d1_index <= N_bitmaps; d1_index++ ) {
		d2_index = d2_index_for_d1_index(d1_index);
		if (d2_index != -1 && !is_effect[d2_index]) {
			Last_d1_bitmap_replacement_stats.wall_entries++;
			PHYSFSX_fseek(d1_Piggy_fp, bitmap_header_start + (d1_index-1) * DISKBITMAPHEADER_D1_SIZE, SEEK_SET);
			DiskBitmapHeader_d1_read(&bmh, d1_Piggy_fp);

			if (!bitmap_read_d1(&GameBitmaps[d2_index], d1_Piggy_fp, bitmap_data_start,
			                    &bmh, &Bitmap_replacement_next, Bitmap_replacement_end,
			                    d1_palette, 0, colormap)) {
				Warning("Skipped invalid or over-limit D1 bitmap replacement %d", d1_index);
				continue;
			}
			Last_d1_bitmap_replacement_stats.wall_applied++;
			piggy_bitmap_set_file_state(d2_index, 0, bmh.flags); // resident D1 replacement

			if ( (p = strchr(piggy_game_bitmap_name(&GameBitmaps[d2_index]), '#')) /* d2 BM is animated */
			     && !(bmh.dflags & DBM_FLAG_ABM) ) { /* d1 bitmap is not animated */
				int i, len = p - piggy_game_bitmap_name(&GameBitmaps[d2_index]);
				for (i = 0; i < Num_bitmap_files; i++)
					if (i != d2_index && !is_effect[i] && ! memcmp(piggy_game_bitmap_name(&GameBitmaps[d2_index]), piggy_game_bitmap_name(&GameBitmaps[i]), len))
					{
						gr_set_bitmap_data(&GameBitmaps[i], NULL);	// free ogl texture
						GameBitmaps[i] = GameBitmaps[d2_index];
						piggy_bitmap_set_file_state(i, 0, bmh.flags);
						Last_d1_bitmap_replacement_stats.animated_clones++;
					}
			}
		}
	}

	PHYSFS_close(d1_Piggy_fp);

	last_palette_loaded_pig[0]= 0;  //force pig re-load

	texmerge_flush();       //for re-merging with new textures
}

int load_d1_bitmap_frame(short d1_index, bitmap_index d2_bitmap)
{
	PHYSFS_file *d1_Piggy_fp;
	DiskBitmapHeader bmh;
	ubyte colormap[256];
	ubyte d1_palette[256*3];
	int pig_data_start, bitmap_header_start, bitmap_data_start;
	int N_bitmaps, N_sounds, pigsize;
	PHYSFS_sint64 header_size;

	if (!Bitmap_replacement_next || !Bitmap_replacement_end || d2_bitmap.index >= MAX_BITMAP_FILES)
		return 0;

	d1_Piggy_fp = PHYSFSX_openReadBuffered(D1_PIGFILE);
	if (!d1_Piggy_fp)
		return 0;

	if (get_d1_colormap(d1_palette, colormap) != 0) {
		PHYSFS_close(d1_Piggy_fp);
		return 0;
	}

	pigsize = PHYSFS_fileLength(d1_Piggy_fp);
	switch (pigsize) {
	case D1_SHARE_BIG_PIGSIZE:
	case D1_SHARE_10_PIGSIZE:
	case D1_SHARE_PIGSIZE:
	case D1_10_BIG_PIGSIZE:
	case D1_10_PIGSIZE:
		pig_data_start = 0;
		break;
	default:
		pig_data_start = PHYSFSX_readInt(d1_Piggy_fp);
		break;
	}

	if (pig_data_start < 0 || pig_data_start > pigsize - 2 * (int)sizeof(int) ||
	    PHYSFSX_fseek(d1_Piggy_fp, pig_data_start, SEEK_SET)) {
		PHYSFS_close(d1_Piggy_fp);
		return 0;
	}
	N_bitmaps = PHYSFSX_readInt(d1_Piggy_fp);
	N_sounds = PHYSFSX_readInt(d1_Piggy_fp);
	if (d1_index <= 0 || d1_index > N_bitmaps) {
		PHYSFS_close(d1_Piggy_fp);
		return 0;
	}

	header_size = (PHYSFS_sint64)N_bitmaps * DISKBITMAPHEADER_D1_SIZE
		+ (PHYSFS_sint64)N_sounds * sizeof(DiskSoundHeader);
	bitmap_header_start = pig_data_start + 2 * sizeof(int);
	if (pig_data_start < 0 || N_bitmaps < 0 || N_bitmaps > D1_MAX_TMAP_NUM ||
	    N_sounds < 0 || header_size < 0 ||
	    (PHYSFS_sint64)bitmap_header_start + header_size > pigsize) {
		PHYSFS_close(d1_Piggy_fp);
		return 0;
	}
	bitmap_data_start = bitmap_header_start + (int)header_size;
	PHYSFSX_fseek(d1_Piggy_fp, bitmap_header_start + (d1_index - 1) * DISKBITMAPHEADER_D1_SIZE, SEEK_SET);
	DiskBitmapHeader_d1_read(&bmh, d1_Piggy_fp);

	if (!bitmap_read_d1(&GameBitmaps[d2_bitmap.index], d1_Piggy_fp, bitmap_data_start,
	                    &bmh, &Bitmap_replacement_next, Bitmap_replacement_end,
	                    d1_palette, 0, colormap)) {
		PHYSFS_close(d1_Piggy_fp);
		return 0;
	}
	piggy_bitmap_set_file_state(d2_bitmap.index, 0, bmh.flags);

	PHYSFS_close(d1_Piggy_fp);
	return 1;
}


extern int extra_bitmap_num;

/*
 * Find and load the named bitmap from descent.pig
 * similar to read_extra_bitmap_iff
 */
bitmap_index read_extra_bitmap_d1_pig(char *name)
{
	bitmap_index bitmap_num;
	grs_bitmap * n = &GameBitmaps[extra_bitmap_num];

	bitmap_num.index = 0;

	{
		PHYSFS_file *d1_Piggy_fp;
		DiskBitmapHeader bmh;
		int pig_data_start, bitmap_header_start, bitmap_data_start;
		int i, N_bitmaps;
		ubyte colormap[256];
		ubyte d1_palette[256*3];
		int pigsize;

		d1_Piggy_fp = PHYSFSX_openReadBuffered(D1_PIGFILE);

		if (!d1_Piggy_fp)
		{
			Warning(D1_PIG_LOAD_FAILED);
			return bitmap_num;
		}

		if (get_d1_colormap( d1_palette, colormap ) != 0)
			Warning("Could not load descent 1 color palette");

		pigsize = PHYSFS_fileLength(d1_Piggy_fp);
		switch (pigsize) {
		case D1_SHARE_BIG_PIGSIZE:
		case D1_SHARE_10_PIGSIZE:
		case D1_SHARE_PIGSIZE:
		case D1_10_BIG_PIGSIZE:
		case D1_10_PIGSIZE:
			pig_data_start = 0;
			break;
		default:
			Warning("Unknown size for " D1_PIGFILE);
			Int3();
			// fall through
		case D1_PIGSIZE:
		case D1_OEM_PIGSIZE:
		case D1_MAC_PIGSIZE:
		case D1_MAC_SHARE_PIGSIZE:
			pig_data_start = PHYSFSX_readInt(d1_Piggy_fp );

			break;
		}

		PHYSFSX_fseek( d1_Piggy_fp, pig_data_start, SEEK_SET );
		N_bitmaps = PHYSFSX_readInt(d1_Piggy_fp);
		{
			int N_sounds = PHYSFSX_readInt(d1_Piggy_fp);
			int header_size = N_bitmaps * DISKBITMAPHEADER_D1_SIZE
				+ N_sounds * sizeof(DiskSoundHeader);
			bitmap_header_start = pig_data_start + 2 * sizeof(int);
			bitmap_data_start = bitmap_header_start + header_size;
		}

		for (i = 1; i <= N_bitmaps; i++)
		{
			DiskBitmapHeader_d1_read(&bmh, d1_Piggy_fp);
			if (!d_strnicmp(bmh.name, name, 8))
				break;
		}

		if (d_strnicmp(bmh.name, name, 8))
		{
			con_printf(CON_DEBUG, "could not find bitmap %s\n", name);
			return bitmap_num;
		}

		if (!bitmap_read_d1(n, d1_Piggy_fp, bitmap_data_start, &bmh, NULL, NULL,
		                    d1_palette, 0, colormap)) {
			PHYSFS_close(d1_Piggy_fp);
			return bitmap_num;
		}

		PHYSFS_close(d1_Piggy_fp);
	}

	n->avg_color = 0;	//compute_average_pixel(n);

	bitmap_num.index = extra_bitmap_num;

	GameBitmaps[extra_bitmap_num++] = *n;

	return bitmap_num;
}
