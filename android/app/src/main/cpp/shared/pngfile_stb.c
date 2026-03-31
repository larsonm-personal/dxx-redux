/*
 * pngfile_stb.c -- PNG/TGA/JPG texture loading via stb_image
 *
 * Android port: replacement for pngfile.c (which requires libpng).
 * Implements the same read_png() interface so ogl.c texture replacement
 * works without libpng.  Uses stb_image which supports PNG, TGA, JPG,
 * and BMP in a single public-domain header.
 *
 * write_png() is stubbed out (screenshots use TGA fallback on Android).
 */

#include <stdlib.h>
#include <string.h>
#include <physfs.h>

#include "pngfile.h"
#include "pstypes.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO  /* we use PhysFS, not fopen */
#define STBI_NO_HDR    /* no HDR support needed */
#define STBI_NO_LINEAR /* no linear float output needed */
#define STBI_NO_PSD
#define STBI_NO_GIF
#include "stb_image.h"

int read_png(const char *filename, png_data *pdata)
{
	PHYSFS_File *fp;
	PHYSFS_sint64 fsize;
	unsigned char *fbuf;
	int w, h, channels;
	unsigned char *pixels;

	if (!filename || !pdata)
		return 0;

	fp = PHYSFS_openRead(filename);
	if (!fp)
		return 0;

	fsize = PHYSFS_fileLength(fp);
	if (fsize <= 0 || fsize > 256 * 1024 * 1024) { /* sanity: 256 MB max */
		PHYSFS_close(fp);
		return 0;
	}

	fbuf = (unsigned char *) malloc((size_t) fsize);
	if (!fbuf) {
		PHYSFS_close(fp);
		return 0;
	}

	if (PHYSFS_readBytes(fp, fbuf, (PHYSFS_uint64) fsize) != fsize) {
		free(fbuf);
		PHYSFS_close(fp);
		return 0;
	}
	PHYSFS_close(fp);

	/* Ask stb_image for RGBA or RGB depending on what's in the file.
	 * channels=0 means "give me whatever the file has". */
	pixels = stbi_load_from_memory(fbuf, (int) fsize, &w, &h, &channels, 0);
	free(fbuf);

	if (!pixels)
		return 0;

	memset(pdata, 0, sizeof(*pdata));
	pdata->width = (unsigned int) w;
	pdata->height = (unsigned int) h;
	pdata->depth = 8;
	pdata->channels = channels;
	pdata->data = pixels; /* caller frees with free() */
	pdata->palette = NULL;
	pdata->num_palette = 0;
	pdata->paletted = 0;
	pdata->color = (channels >= 3) ? 1 : 0;
	pdata->alpha = (channels == 4 || channels == 2) ? 1 : 0;

	return 1;
}

/* Screenshots: on Android without libpng, gr.c falls back to TGA output
 * (the #else branch in write_bmp).  Provide a stub so linking succeeds
 * if anything references write_png. */
int write_png(const char *filename, png_data *pdata)
{
	(void) filename;
	(void) pdata;
	return 0;
}

/* Read a pre-compressed .etc2 file via PhysFS.
 * Returns 1 on success, 0 on failure.
 * On success, caller must free edata->filedata. */
int read_etc2_file(const char *filename, etc2_file_data *edata)
{
	PHYSFS_File *fp;
	PHYSFS_sint64 fsize;
	unsigned char hdr[16];

	if (!filename || !edata)
		return 0;

	fp = PHYSFS_openRead(filename);
	if (!fp)
		return 0;

	fsize = PHYSFS_fileLength(fp);
	if (fsize < 16 || fsize > 64 * 1024 * 1024) {
		PHYSFS_close(fp);
		return 0;
	}

	/* Read and validate header */
	if (PHYSFS_readBytes(fp, hdr, 16) != 16) {
		PHYSFS_close(fp);
		return 0;
	}
	if (hdr[0] != 'E' || hdr[1] != 'T' || hdr[2] != 'C' || hdr[3] != '2') {
		PHYSFS_close(fp);
		return 0;
	}

	memset(edata, 0, sizeof(*edata));
	edata->width = hdr[4] | ((unsigned int) hdr[5] << 8);
	edata->height = hdr[6] | ((unsigned int) hdr[7] << 8);
	edata->format = hdr[8];
	edata->mip_count = hdr[9];
	/* Original (pre-padding) dimensions; 0 means same as padded */
	edata->orig_width = hdr[10] | ((unsigned int) hdr[11] << 8);
	edata->orig_height = hdr[12] | ((unsigned int) hdr[13] << 8);
	if (edata->orig_width == 0) edata->orig_width = edata->width;
	if (edata->orig_height == 0) edata->orig_height = edata->height;

	if (edata->mip_count == 0 || edata->format > 1) {
		PHYSFS_close(fp);
		return 0;
	}

	/* Read remaining file data (mip entries) */
	edata->filedata_size = (unsigned int) (fsize - 16);
	edata->filedata = (unsigned char *) malloc(edata->filedata_size);
	if (!edata->filedata) {
		PHYSFS_close(fp);
		return 0;
	}

	if (PHYSFS_readBytes(fp, edata->filedata, edata->filedata_size) != (PHYSFS_sint64) edata->filedata_size) {
		free(edata->filedata);
		edata->filedata = NULL;
		PHYSFS_close(fp);
		return 0;
	}

	PHYSFS_close(fp);
	return 1;
}
