/* etc2tool.cpp -- offline ETC2 texture compressor
 *
 * Reads PNG/JPG/TGA/BMP via stb_image, compresses to ETC2 via etc2comp
 * at effort 80 (out of 100), writes a KTX2 file with full mip chain.
 *
 * Output is standard KTX2 viewable in any KTX-compatible tool.
 * Original (pre-padding) image dimensions stored in KTX key-value
 * metadata as "OrigWidth" and "OrigHeight" (uint16 LE each).
 *
 * Build: cmake -B build && cmake --build build --config Release
 * Usage: etc2tool input.png output.ktx2 [--no-mips]
 */

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_PSD
#define STBI_NO_GIF
#include "stb_image.h"
#include "Etc.h"
#define KHRONOS_STATIC
#include <ktx.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>

/* VkFormat values for ETC2 */
#define VK_FMT_ETC2_RGB8   147  /* VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK */
#define VK_FMT_ETC2_RGBA8  151  /* VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK */

/* Round up to next power of 2 */
static int pow2ize(int v)
{
	int p = 1;
	while (p < v)
		p <<= 1;
	return p;
}


/* Box-filter downsample RGBA by 2x */
static uint8_t *downsample_rgba(const uint8_t *src, int w, int h,
                                int *out_w, int *out_h)
{
	int nw = w / 2, nh = h / 2;
	if (nw < 1) nw = 1;
	if (nh < 1) nh = 1;
	uint8_t *dst = (uint8_t *)malloc(nw * nh * 4);
	if (!dst) return NULL;
	for (int y = 0; y < nh; y++)
		for (int x = 0; x < nw; x++)
			for (int c = 0; c < 4; c++) {
				int x0 = x * 2, y0 = y * 2;
				int x1 = (x0 + 1 < w) ? x0 + 1 : x0;
				int y1 = (y0 + 1 < h) ? y0 + 1 : y0;
				int v = src[(y0 * w + x0) * 4 + c]
				      + src[(y0 * w + x1) * 4 + c]
				      + src[(y1 * w + x0) * 4 + c]
				      + src[(y1 * w + x1) * 4 + c];
				dst[(y * nw + x) * 4 + c] = (uint8_t)((v + 2) / 4);
			}
	*out_w = nw;
	*out_h = nh;
	return dst;
}

/* Compress RGBA pixels to ETC2 via etc2comp, max effort */
static unsigned char *compress_etc2(const uint8_t *rgba, int w, int h,
                                    bool has_alpha, unsigned int *out_bytes)
{
	int npix = w * h;
	float *fsrc = (float *)malloc((size_t)npix * 4 * sizeof(float));
	if (!fsrc) return NULL;
	for (int i = 0; i < npix * 4; i++)
		fsrc[i] = rgba[i] / 255.0f;

	unsigned char *enc_bits = NULL;
	unsigned int enc_bytes = 0;
	unsigned int ext_w = 0, ext_h = 0;
	int enc_time_ms = 0;

	Etc::Image::Format fmt = has_alpha ? Etc::Image::Format::RGBA8
	                                   : Etc::Image::Format::RGB8;

	Etc::Encode(fsrc, (unsigned int)w, (unsigned int)h,
	            fmt, Etc::RGBA,
	            80.0f, /* high effort for offline quality */
	            16, 16, /* use multiple threads for speed */
	            &enc_bits, &enc_bytes,
	            &ext_w, &ext_h,
	            &enc_time_ms);

	free(fsrc);

	if (!enc_bits || enc_bytes == 0)
		return NULL;

	*out_bytes = enc_bytes;
	return enc_bits;
}

/* Pad image to power-of-2 dimensions with transparent black */
static uint8_t *pad_to_pow2(const uint8_t *src, int w, int h, int channels,
                            int tw, int th, bool *has_alpha_out)
{
	uint8_t *dst = (uint8_t *)calloc(tw * th * 4, 1);
	if (!dst) return NULL;

	bool has_alpha = (channels == 4 || channels == 2);
	*has_alpha_out = has_alpha;

	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			int si = (y * w + x) * channels;
			int di = (y * tw + x) * 4;
			if (channels >= 3) {
				dst[di + 0] = src[si + 0];
				dst[di + 1] = src[si + 1];
				dst[di + 2] = src[si + 2];
				dst[di + 3] = (channels == 4) ? src[si + 3] : 255;
			} else if (channels == 2) {
				dst[di + 0] = dst[di + 1] = dst[di + 2] = src[si + 0];
				dst[di + 3] = src[si + 1];
			} else {
				dst[di + 0] = dst[di + 1] = dst[di + 2] = src[si + 0];
				dst[di + 3] = 255;
			}
		}
	}
	return dst;
}

int main(int argc, char **argv)
{
	if (argc < 3) {
		fprintf(stderr, "Usage: etc2tool input.(png|jpg|tga) output.ktx2 [--no-mips]\n");
		return 1;
	}

	const char *input_path = argv[1];
	const char *output_path = argv[2];
	bool gen_mips = true;

	for (int i = 3; i < argc; i++) {
		if (strcmp(argv[i], "--no-mips") == 0)
			gen_mips = false;
	}

	/* Load image */
	int w, h, channels;
	uint8_t *pixels = stbi_load(input_path, &w, &h, &channels, 0);
	if (!pixels) {
		fprintf(stderr, "Failed to load: %s\n", input_path);
		return 1;
	}

	/* Power-of-2 dimensions (ETC2 needs multiple of 4, pow2 guarantees that) */
	int tw = pow2ize(w);
	int th = pow2ize(h);

	/* Minimum 4x4 for ETC2 blocks */
	if (tw < 4) tw = 4;
	if (th < 4) th = 4;

	/* Pad to pow2 and convert to RGBA */
	bool has_alpha;
	uint8_t *rgba = pad_to_pow2(pixels, w, h, channels, tw, th, &has_alpha);
	stbi_image_free(pixels);
	if (!rgba) {
		fprintf(stderr, "Out of memory\n");
		return 1;
	}

	/* Count mip levels */
	int mip_count = 1;
	if (gen_mips) {
		int mw = tw, mh = th;
		while (mw > 4 && mh > 4) {
			mw /= 2;
			mh /= 2;
			mip_count++;
		}
	}

	/* Compress all mip levels first (KTX2 needs all data before writing) */
	struct mip_level {
		unsigned char *data;
		unsigned int size;
	};
	mip_level *mips = new mip_level[mip_count]();

	uint8_t *cur = rgba;
	int mw = tw, mh = th;
	for (int level = 0; level < mip_count; level++) {
		mips[level].data = compress_etc2(cur, mw, mh, has_alpha, &mips[level].size);
		if (!mips[level].data) {
			fprintf(stderr, "Compression failed at mip level %d (%dx%d)\n",
			        level, mw, mh);
			for (int j = 0; j < level; j++) delete[] mips[j].data;
			delete[] mips;
			if (cur != rgba) free(cur);
			free(rgba);
			return 1;
		}
		if (level + 1 < mip_count) {
			int nmw, nmh;
			uint8_t *next = downsample_rgba(cur, mw, mh, &nmw, &nmh);
			if (cur != rgba) free(cur);
			cur = next;
			mw = nmw;
			mh = nmh;
		}
	}
	if (cur != rgba) free(cur);
	free(rgba);

	/* Create KTX2 texture */
	ktxTextureCreateInfo ci = {};
	ci.vkFormat = has_alpha ? VK_FMT_ETC2_RGBA8 : VK_FMT_ETC2_RGB8;
	ci.baseWidth = (ktx_uint32_t)tw;
	ci.baseHeight = (ktx_uint32_t)th;
	ci.baseDepth = 1;
	ci.numDimensions = 2;
	ci.numLevels = (ktx_uint32_t)mip_count;
	ci.numLayers = 1;
	ci.numFaces = 1;
	ci.isArray = KTX_FALSE;
	ci.generateMipmaps = KTX_FALSE;

	ktxTexture2 *tex = NULL;
	KTX_error_code err = ktxTexture2_Create(&ci,
		KTX_TEXTURE_CREATE_ALLOC_STORAGE, &tex);
	if (err != KTX_SUCCESS) {
		fprintf(stderr, "ktxTexture2_Create: %s\n", ktxErrorString(err));
		for (int i = 0; i < mip_count; i++) delete[] mips[i].data;
		delete[] mips;
		return 1;
	}

	/* Set compressed image data for each mip level */
	for (int level = 0; level < mip_count; level++) {
		err = ktxTexture_SetImageFromMemory(ktxTexture(tex),
			(ktx_uint32_t)level, 0, 0,
			mips[level].data, (ktx_size_t)mips[level].size);
		if (err != KTX_SUCCESS) {
			fprintf(stderr, "SetImageFromMemory mip %d: %s\n",
			        level, ktxErrorString(err));
			ktxTexture_Destroy(ktxTexture(tex));
			for (int i = 0; i < mip_count; i++) delete[] mips[i].data;
			delete[] mips;
			return 1;
		}
	}

	/* Store original (pre-padding) dimensions as key-value metadata */
	uint16_t ow = (uint16_t)w, oh = (uint16_t)h;
	ktxHashList_AddKVPair(&ktxTexture(tex)->kvDataHead,
	                      "OrigWidth", sizeof(ow), &ow);
	ktxHashList_AddKVPair(&ktxTexture(tex)->kvDataHead,
	                      "OrigHeight", sizeof(oh), &oh);

	/* Write KTX2 file */
	err = ktxTexture_WriteToNamedFile(ktxTexture(tex), output_path);
	if (err != KTX_SUCCESS) {
		fprintf(stderr, "WriteToNamedFile: %s\n", ktxErrorString(err));
		ktxTexture_Destroy(ktxTexture(tex));
		for (int i = 0; i < mip_count; i++) delete[] mips[i].data;
		delete[] mips;
		return 1;
	}

	ktxTexture_Destroy(ktxTexture(tex));
	for (int i = 0; i < mip_count; i++) delete[] mips[i].data;
	delete[] mips;

	printf("OK: %dx%d %s %d mips -> %s\n",
	       tw, th, has_alpha ? "RGBA" : "RGB", mip_count, output_path);
	return 0;
}
