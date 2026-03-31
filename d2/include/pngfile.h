#ifndef PNGFILE_H
#define PNGFILE_H
#include "pstypes.h" // for consistent struct packing

typedef struct _png_data {
	unsigned int width;
	unsigned int height;
	unsigned int depth;
	unsigned int channels;
	unsigned paletted:1;
	unsigned color:1;
	unsigned alpha:1;

	unsigned char *data;
	unsigned char *palette;
	int num_palette;
} png_data;

extern int read_png(const char *filename, png_data *pdata);
extern int write_png(const char *filename, png_data *pdata);

#ifdef ANDROID
/* Pre-compressed ETC2 texture file (.etc2 format) */
typedef struct _etc2_file_data {
	unsigned int width;
	unsigned int height;
	unsigned char format;    /* 0 = RGB8_ETC2, 1 = RGBA8_ETC2_EAC */
	unsigned char mip_count;
	unsigned int orig_width;  /* pre-padding width (0 = same as width) */
	unsigned int orig_height; /* pre-padding height (0 = same as height) */
	unsigned char *filedata; /* raw file contents after header */
	unsigned int filedata_size;
} etc2_file_data;

extern int read_etc2_file(const char *filename, etc2_file_data *edata);
#endif

#endif
