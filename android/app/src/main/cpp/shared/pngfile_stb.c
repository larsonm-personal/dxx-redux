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
#include <stdint.h>
#include <physfs.h>

#include "pngfile.h"
#include "pstypes.h"

#define KHRONOS_STATIC
#include <ktx.h>

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

/* Read a pre-compressed .ktx2 texture file via PhysFS.
 * Returns 1 on success, 0 on failure.
 * On success, caller must free edata->filedata.
 * Mip data is packed as [uint32_le size][data] per level, matching
 * the layout that ogl.c expects for glCompressedTexImage2D uploads. */
int read_ktx2_file(const char *filename, etc2_file_data *edata)
{
	PHYSFS_File *fp;
	PHYSFS_sint64 fsize;
	unsigned char *fbuf;

	if (!filename || !edata)
		return 0;

	fp = PHYSFS_openRead(filename);
	if (!fp)
		return 0;

	fsize = PHYSFS_fileLength(fp);
	if (fsize < 80 || fsize > 64 * 1024 * 1024) {
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

	/* Parse KTX2 container */
	ktxTexture2 *tex = NULL;
	KTX_error_code kerr = ktxTexture2_CreateFromMemory(
		fbuf, (ktx_size_t) fsize,
		KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &tex);
	free(fbuf);
	if (kerr != KTX_SUCCESS)
		return 0;

	/* Map VkFormat to our format byte */
	unsigned char fmt;
	if (tex->vkFormat == 147)       /* VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK */
		fmt = 0;
	else if (tex->vkFormat == 151)  /* VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK */
		fmt = 1;
	else {
		ktxTexture_Destroy(ktxTexture(tex));
		return 0;
	}

	ktxTexture *base = ktxTexture(tex);

	memset(edata, 0, sizeof(*edata));
	edata->width = base->baseWidth;
	edata->height = base->baseHeight;
	edata->format = fmt;
	edata->mip_count = (unsigned char) base->numLevels;

	/* Read original dimensions from KTX key-value metadata */
	unsigned int vlen = 0;
	void *vptr = NULL;
	if (ktxHashList_FindValue(&base->kvDataHead, "OrigWidth",
	                          &vlen, &vptr) == KTX_SUCCESS && vlen >= 2) {
		uint16_t ow;
		memcpy(&ow, vptr, 2);
		edata->orig_width = ow;
	} else {
		edata->orig_width = edata->width;
	}
	if (ktxHashList_FindValue(&base->kvDataHead, "OrigHeight",
	                          &vlen, &vptr) == KTX_SUCCESS && vlen >= 2) {
		uint16_t oh;
		memcpy(&oh, vptr, 2);
		edata->orig_height = oh;
	} else {
		edata->orig_height = edata->height;
	}

	/* Build [uint32_le size][data] buffer that ogl.c expects */
	unsigned int total = 0;
	ktx_uint32_t nlev = base->numLevels;
	for (ktx_uint32_t lv = 0; lv < nlev; lv++)
		total += 4 + (unsigned int) ktxTexture_GetImageSize(base, lv);

	edata->filedata = (unsigned char *) malloc(total);
	if (!edata->filedata) {
		ktxTexture_Destroy(base);
		return 0;
	}
	edata->filedata_size = total;

	unsigned char *p = edata->filedata;
	for (ktx_uint32_t lv = 0; lv < nlev; lv++) {
		ktx_size_t offset = 0;
		ktxTexture_GetImageOffset(base, lv, 0, 0, &offset);
		ktx_size_t isz = ktxTexture_GetImageSize(base, lv);
		/* uint32 LE size prefix */
		p[0] = (unsigned char)(isz & 0xFF);
		p[1] = (unsigned char)((isz >> 8) & 0xFF);
		p[2] = (unsigned char)((isz >> 16) & 0xFF);
		p[3] = (unsigned char)((isz >> 24) & 0xFF);
		p += 4;
		memcpy(p, base->pData + offset, isz);
		p += isz;
	}

	ktxTexture_Destroy(base);
	return 1;
}
