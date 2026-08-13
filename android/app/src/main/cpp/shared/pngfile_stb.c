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
#include <limits.h>
#include <time.h>
#include <physfs.h>

#include "pngfile.h"
#include "pstypes.h"
#include "android_log.h"

#define KHRONOS_STATIC
#include <ktx.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO  /* we use PhysFS, not fopen */
#define STBI_NO_HDR    /* no HDR support needed */
#define STBI_NO_LINEAR /* no linear float output needed */
#define STBI_NO_PSD
#define STBI_NO_GIF
#define STBI_MAX_DIMENSIONS 2048
#include "stb_image.h"

#define ANDROID_REPLACEMENT_TEXTURE_MAX_DIMENSION 2048u
#define ANDROID_REPLACEMENT_TEXTURE_MAX_DECODED_BYTES \
	(ANDROID_REPLACEMENT_TEXTURE_MAX_DIMENSION * ANDROID_REPLACEMENT_TEXTURE_MAX_DIMENSION * 4u)

#define TEXTURE_LOOKUP_MISS_CACHE_SIZE      16384u
#define TEXTURE_LOOKUP_MISS_CACHE_MAX_COUNT ((TEXTURE_LOOKUP_MISS_CACHE_SIZE * 3u) / 4u)
#define TEXTURE_LOOKUP_FILE_INDEX_SIZE      65536u
#define TEXTURE_LOOKUP_FILE_INDEX_MAX_COUNT ((TEXTURE_LOOKUP_FILE_INDEX_SIZE * 3u) / 4u)
/* Traversal limits must match DxaTextureScanner.kt launcher preflight. */
#define TEXTURE_LOOKUP_MAX_DEPTH               64u
#define TEXTURE_LOOKUP_MAX_ENTRIES             65536u
#define TEXTURE_LOOKUP_MAX_DIRECTORIES         16384u
#define TEXTURE_LOOKUP_MAX_PATH_BYTES          4096u
#define TEXTURE_LOOKUP_MAX_RETAINED_PATH_BYTES (8u * 1024u * 1024u)
#define TEXTURE_LOOKUP_MAX_INDEXED_PATH_BYTES  (16u * 1024u * 1024u)

static char *g_texture_lookup_miss_cache[TEXTURE_LOOKUP_MISS_CACHE_SIZE];
static unsigned int g_texture_lookup_miss_cache_count = 0;
static char *g_texture_lookup_file_index[TEXTURE_LOOKUP_FILE_INDEX_SIZE];
static unsigned int g_texture_lookup_file_index_count = 0;
static size_t g_texture_lookup_file_index_bytes = 0;
static int g_texture_lookup_file_index_ready = 0;

typedef struct texture_lookup_directory {
	char *path;
	unsigned int depth;
} texture_lookup_directory;

typedef struct texture_lookup_walk {
	texture_lookup_directory *directories;
	size_t directory_count;
	size_t directory_capacity;
	size_t retained_path_bytes;
	unsigned int visited_entries;
	unsigned int discovered_directories;
	unsigned int current_depth;
	int failed;
	char failure_path[256];
} texture_lookup_walk;

static unsigned char texture_lookup_ascii_fold(unsigned char c)
{
	if (c == '\\')
		return '/';
	if (c >= 'A' && c <= 'Z')
		return (unsigned char) (c - 'A' + 'a');
	return c;
}

static unsigned int texture_lookup_miss_cache_hash(const char *filename)
{
	unsigned int hash = 2166136261u;

	while (*filename) {
		hash ^= texture_lookup_ascii_fold((unsigned char) *filename++);
		hash *= 16777619u;
	}
	return hash;
}

static int texture_lookup_path_equals_ci(const char *left, const char *right)
{
	for (;;) {
		unsigned char left_c = texture_lookup_ascii_fold((unsigned char) *left++);
		unsigned char right_c = texture_lookup_ascii_fold((unsigned char) *right++);

		if (left_c != right_c)
			return 0;
		if (!left_c)
			return 1;
	}
}

static int texture_lookup_is_indexed_extension(const char *filename)
{
	const char *dot = strrchr(filename, '.');

	if (!dot)
		return 0;

	return texture_lookup_path_equals_ci(dot, ".ktx2") ||
	       texture_lookup_path_equals_ci(dot, ".png") ||
	       texture_lookup_path_equals_ci(dot, ".jpg") ||
	       texture_lookup_path_equals_ci(dot, ".tga");
}

static int texture_lookup_file_index_add(const char *filename)
{
	unsigned int index;
	unsigned int probe;
	char *copy;
	size_t len;

	if (!filename || !filename[0] || !texture_lookup_is_indexed_extension(filename))
		return 1;
	if (g_texture_lookup_file_index_count >= TEXTURE_LOOKUP_FILE_INDEX_MAX_COUNT)
		return 0;

	index = texture_lookup_miss_cache_hash(filename) & (TEXTURE_LOOKUP_FILE_INDEX_SIZE - 1u);
	for (probe = 0; probe < TEXTURE_LOOKUP_FILE_INDEX_SIZE; probe++) {
		char *entry = g_texture_lookup_file_index[index];

		if (!entry)
			break;
		if (texture_lookup_path_equals_ci(entry, filename))
			return 1;
		index = (index + 1u) & (TEXTURE_LOOKUP_FILE_INDEX_SIZE - 1u);
	}
	if (probe >= TEXTURE_LOOKUP_FILE_INDEX_SIZE)
		return 0;

	len = strlen(filename) + 1u;
	if (len > TEXTURE_LOOKUP_MAX_PATH_BYTES + 1u ||
	    len > TEXTURE_LOOKUP_MAX_INDEXED_PATH_BYTES - g_texture_lookup_file_index_bytes)
		return 0;
	copy = (char *) malloc(len);
	if (!copy)
		return 0;
	memcpy(copy, filename, len);
	g_texture_lookup_file_index[index] = copy;
	g_texture_lookup_file_index_count++;
	g_texture_lookup_file_index_bytes += len;
	return 1;
}

static void texture_lookup_walk_fail(texture_lookup_walk *walk, const char *path)
{
	if (!walk || walk->failed)
		return;
	walk->failed = 1;
	snprintf(walk->failure_path, sizeof(walk->failure_path), "%s", path ? path : "");
}

static int texture_lookup_walk_push(texture_lookup_walk *walk, char *path, unsigned int depth)
{
	texture_lookup_directory *next;
	size_t path_bytes = strlen(path) + 1u;

	if (depth > TEXTURE_LOOKUP_MAX_DEPTH ||
	    (depth && walk->discovered_directories >= TEXTURE_LOOKUP_MAX_DIRECTORIES) ||
	    path_bytes > TEXTURE_LOOKUP_MAX_RETAINED_PATH_BYTES - walk->retained_path_bytes) {
		texture_lookup_walk_fail(walk, path);
		return 0;
	}
	if (walk->directory_count == walk->directory_capacity) {
		size_t next_capacity = walk->directory_capacity ? walk->directory_capacity * 2u : 64u;

		if (next_capacity > TEXTURE_LOOKUP_MAX_DIRECTORIES)
			next_capacity = TEXTURE_LOOKUP_MAX_DIRECTORIES;
		next = (texture_lookup_directory *) realloc(
		    walk->directories, next_capacity * sizeof(*walk->directories));
		if (!next) {
			texture_lookup_walk_fail(walk, path);
			return 0;
		}
		walk->directories = next;
		walk->directory_capacity = next_capacity;
	}
	walk->directories[walk->directory_count].path = path;
	walk->directories[walk->directory_count].depth = depth;
	walk->directory_count++;
	if (depth)
		walk->discovered_directories++;
	walk->retained_path_bytes += path_bytes;
	return 1;
}

static PHYSFS_EnumerateCallbackResult texture_lookup_walk_entry(void *data,
                                                                const char *origdir, const char *leaf)
{
	texture_lookup_walk *walk = (texture_lookup_walk *) data;
	PHYSFS_Stat statbuf;
	char *child_path;
	size_t path_len = origdir && origdir[0] ? strlen(origdir) : 0u;
	size_t leaf_len = leaf ? strlen(leaf) : 0u;
	size_t child_len;
	unsigned int depth;

	if (!walk || walk->failed)
		return PHYSFS_ENUM_STOP;
	if (++walk->visited_entries > TEXTURE_LOOKUP_MAX_ENTRIES ||
	    !leaf || !leaf[0] || strchr(leaf, '/') || strchr(leaf, '\\') ||
	    leaf_len > TEXTURE_LOOKUP_MAX_PATH_BYTES ||
	    (path_len && (leaf_len > TEXTURE_LOOKUP_MAX_PATH_BYTES - 1u ||
	                  path_len > TEXTURE_LOOKUP_MAX_PATH_BYTES - leaf_len - 1u))) {
		texture_lookup_walk_fail(walk, origdir);
		return PHYSFS_ENUM_STOP;
	}
	child_len = path_len ? path_len + 1u + leaf_len + 1u : leaf_len + 1u;
	child_path = (char *) malloc(child_len);
	if (!child_path) {
		texture_lookup_walk_fail(walk, origdir);
		return PHYSFS_ENUM_STOP;
	}
	if (path_len) {
		snprintf(child_path, child_len, "%s/%s", origdir, leaf);
	} else {
		memcpy(child_path, leaf, child_len);
	}
	depth = walk->current_depth + 1u;
	if (!PHYSFS_stat(child_path, &statbuf)) {
		texture_lookup_walk_fail(walk, child_path);
		free(child_path);
		return PHYSFS_ENUM_STOP;
	}
	if (statbuf.filetype == PHYSFS_FILETYPE_DIRECTORY) {
		/* Replacement lookups are either bare archive entries or paths below
		 * textures/.  Do not recursively index unrelated mounted game data. */
		if (!walk->current_depth &&
		    !texture_lookup_path_equals_ci(leaf, "textures")) {
			free(child_path);
			return PHYSFS_ENUM_OK;
		}
		if (!texture_lookup_walk_push(walk, child_path, depth)) {
			free(child_path);
			return PHYSFS_ENUM_STOP;
		}
	} else {
		if (statbuf.filetype == PHYSFS_FILETYPE_REGULAR &&
		    !texture_lookup_file_index_add(child_path))
			texture_lookup_walk_fail(walk, child_path);
		free(child_path);
	}
	return walk->failed ? PHYSFS_ENUM_STOP : PHYSFS_ENUM_OK;
}

static int texture_lookup_build_file_index(void)
{
	texture_lookup_walk walk = { 0 };
	char *root = (char *) malloc(1u);
	struct timespec start, end;
	long long elapsed_us;

	if (!root)
		return 0;
	clock_gettime(CLOCK_MONOTONIC, &start);
	root[0] = '\0';
	if (!texture_lookup_walk_push(&walk, root, 0u)) {
		free(root);
		free(walk.directories);
		return 0;
	}
	while (walk.directory_count && !walk.failed) {
		texture_lookup_directory directory = walk.directories[--walk.directory_count];

		walk.retained_path_bytes -= strlen(directory.path) + 1u;
		walk.current_depth = directory.depth;
		if (!PHYSFS_enumerate(directory.path, texture_lookup_walk_entry, &walk))
			texture_lookup_walk_fail(&walk, directory.path);
		free(directory.path);
	}
	while (walk.directory_count)
		free(walk.directories[--walk.directory_count].path);
	if (walk.failed) {
		const char *real_dir = walk.failure_path[0] ? PHYSFS_getRealDir(walk.failure_path) : NULL;

		debug_log(DLOG_TEXTURE,
		          "texture index rejected at '%s' from '%s': hierarchy exceeds safe limits or cannot be read",
		          walk.failure_path, real_dir ? real_dir : "unknown mount");
	}
	clock_gettime(CLOCK_MONOTONIC, &end);
	elapsed_us = ((long long) end.tv_sec - (long long) start.tv_sec) * 1000000LL +
	             ((long long) end.tv_nsec - (long long) start.tv_nsec) / 1000LL;
	debug_log_force(DLOG_PROFILING,
	                "loadprof_v=1 type=texture_index total_us=%lld visited=%u directories=%u indexed=%u result=%s",
	                elapsed_us, walk.visited_entries, walk.discovered_directories,
	                g_texture_lookup_file_index_count, walk.failed ? "failed" : "ok");
	free(walk.directories);
	return !walk.failed;
}

static void texture_lookup_ensure_file_index(void)
{
	if (g_texture_lookup_file_index_ready)
		return;

	g_texture_lookup_file_index_ready = 1;
	if (!texture_lookup_build_file_index()) {
		unsigned int i;

		for (i = 0; i < TEXTURE_LOOKUP_FILE_INDEX_SIZE; i++) {
			free(g_texture_lookup_file_index[i]);
			g_texture_lookup_file_index[i] = NULL;
		}
		g_texture_lookup_file_index_count = 0;
		g_texture_lookup_file_index_bytes = 0;
	}
}

static const char *texture_lookup_resolve_indexed_path(const char *filename)
{
	unsigned int index;
	unsigned int probe;

	if (!filename || !filename[0])
		return NULL;
	if (!texture_lookup_is_indexed_extension(filename))
		return filename;

	texture_lookup_ensure_file_index();
	index = texture_lookup_miss_cache_hash(filename) & (TEXTURE_LOOKUP_FILE_INDEX_SIZE - 1u);
	for (probe = 0; probe < TEXTURE_LOOKUP_FILE_INDEX_SIZE; probe++) {
		const char *entry = g_texture_lookup_file_index[index];

		if (!entry)
			return NULL;
		if (texture_lookup_path_equals_ci(entry, filename))
			return entry;
		index = (index + 1u) & (TEXTURE_LOOKUP_FILE_INDEX_SIZE - 1u);
	}
	return NULL;
}

void clear_texture_lookup_cache(void)
{
	unsigned int i;

	for (i = 0; i < TEXTURE_LOOKUP_MISS_CACHE_SIZE; i++) {
		free(g_texture_lookup_miss_cache[i]);
		g_texture_lookup_miss_cache[i] = NULL;
	}
	for (i = 0; i < TEXTURE_LOOKUP_FILE_INDEX_SIZE; i++) {
		free(g_texture_lookup_file_index[i]);
		g_texture_lookup_file_index[i] = NULL;
	}
	g_texture_lookup_miss_cache_count = 0;
	g_texture_lookup_file_index_count = 0;
	g_texture_lookup_file_index_bytes = 0;
	g_texture_lookup_file_index_ready = 0;
}

static int texture_lookup_miss_cache_contains(const char *filename)
{
	unsigned int index;
	unsigned int probe;

	if (!filename || !filename[0])
		return 0;

	index = texture_lookup_miss_cache_hash(filename) & (TEXTURE_LOOKUP_MISS_CACHE_SIZE - 1u);
	for (probe = 0; probe < TEXTURE_LOOKUP_MISS_CACHE_SIZE; probe++) {
		const char *entry = g_texture_lookup_miss_cache[index];

		if (!entry)
			return 0;
		if (!strcmp(entry, filename))
			return 1;
		index = (index + 1u) & (TEXTURE_LOOKUP_MISS_CACHE_SIZE - 1u);
	}
	return 0;
}

static void texture_lookup_miss_cache_add(const char *filename)
{
	unsigned int index;
	unsigned int probe;
	size_t len;
	char *copy;

	if (!filename || !filename[0])
		return;

	if (g_texture_lookup_miss_cache_count >= TEXTURE_LOOKUP_MISS_CACHE_MAX_COUNT)
		clear_texture_lookup_cache();

	index = texture_lookup_miss_cache_hash(filename) & (TEXTURE_LOOKUP_MISS_CACHE_SIZE - 1u);
	for (probe = 0; probe < TEXTURE_LOOKUP_MISS_CACHE_SIZE; probe++) {
		char *entry = g_texture_lookup_miss_cache[index];

		if (!entry)
			break;
		if (!strcmp(entry, filename))
			return;
		index = (index + 1u) & (TEXTURE_LOOKUP_MISS_CACHE_SIZE - 1u);
	}
	if (probe >= TEXTURE_LOOKUP_MISS_CACHE_SIZE) {
		clear_texture_lookup_cache();
		index = texture_lookup_miss_cache_hash(filename) & (TEXTURE_LOOKUP_MISS_CACHE_SIZE - 1u);
	}

	len = strlen(filename) + 1u;
	copy = (char *) malloc(len);
	if (!copy)
		return;
	memcpy(copy, filename, len);
	g_texture_lookup_miss_cache[index] = copy;
	g_texture_lookup_miss_cache_count++;
}

int read_png(const char *filename, png_data *pdata)
{
	PHYSFS_File *fp;
	PHYSFS_sint64 fsize;
	unsigned char *fbuf;
	int w, h, channels;
	unsigned char *pixels;
	const char *resolved_filename;

	if (!filename || !pdata)
		return 0;
	resolved_filename = texture_lookup_resolve_indexed_path(filename);
	if (!resolved_filename)
		return 0;
	if (texture_lookup_miss_cache_contains(filename) ||
	    (resolved_filename != filename && texture_lookup_miss_cache_contains(resolved_filename)))
		return 0;

	fp = PHYSFS_openRead(resolved_filename);
	if (!fp) {
		texture_lookup_miss_cache_add(filename);
		if (resolved_filename != filename)
			texture_lookup_miss_cache_add(resolved_filename);
		return 0;
	}

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

	if (!stbi_info_from_memory(fbuf, (int) fsize, &w, &h, &channels) ||
	    w <= 0 || h <= 0 || channels < 1 || channels > 4 ||
	    (unsigned int) w > ANDROID_REPLACEMENT_TEXTURE_MAX_DIMENSION ||
	    (unsigned int) h > ANDROID_REPLACEMENT_TEXTURE_MAX_DIMENSION ||
	    (size_t) w * (size_t) h * (size_t) channels > ANDROID_REPLACEMENT_TEXTURE_MAX_DECODED_BYTES) {
		free(fbuf);
		return 0;
	}

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
	const char *resolved_filename;

	if (!filename || !edata)
		return 0;
	resolved_filename = texture_lookup_resolve_indexed_path(filename);
	if (!resolved_filename)
		return 0;
	if (texture_lookup_miss_cache_contains(filename) ||
	    (resolved_filename != filename && texture_lookup_miss_cache_contains(resolved_filename)))
		return 0;

	fp = PHYSFS_openRead(resolved_filename);
	if (!fp) {
		texture_lookup_miss_cache_add(filename);
		if (resolved_filename != filename)
			texture_lookup_miss_cache_add(resolved_filename);
		return 0;
	}

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
	if (tex->vkFormat == 147) /* VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK */
		fmt = 0;
	else if (tex->vkFormat == 151) /* VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK */
		fmt = 1;
	else {
		ktxTexture_Destroy(ktxTexture(tex));
		return 0;
	}

	ktxTexture *base = ktxTexture(tex);
	if (base->numDimensions != 2 || base->isArray || base->numFaces != 1 ||
	    base->numLayers != 1 || base->numLevels < 1 || base->numLevels > 12 ||
	    base->baseWidth < 1 || base->baseHeight < 1 ||
	    base->baseWidth > ANDROID_REPLACEMENT_TEXTURE_MAX_DIMENSION ||
	    base->baseHeight > ANDROID_REPLACEMENT_TEXTURE_MAX_DIMENSION) {
		ktxTexture_Destroy(base);
		return 0;
	}

	memset(edata, 0, sizeof(*edata));
	edata->width = base->baseWidth;
	edata->height = base->baseHeight;
	edata->format = fmt;
	edata->mip_count = (unsigned char) base->numLevels;

	/* Read original dimensions from KTX key-value metadata */
	unsigned int vlen = 0;
	void *vptr = NULL;
	if (ktxHashList_FindValue(&base->kvDataHead, "OrigWidth",
	                          &vlen, &vptr) == KTX_SUCCESS &&
	    vlen >= 2) {
		uint16_t ow;
		memcpy(&ow, vptr, 2);
		edata->orig_width = ow;
	} else {
		edata->orig_width = edata->width;
	}
	if (ktxHashList_FindValue(&base->kvDataHead, "OrigHeight",
	                          &vlen, &vptr) == KTX_SUCCESS &&
	    vlen >= 2) {
		uint16_t oh;
		memcpy(&oh, vptr, 2);
		edata->orig_height = oh;
	} else {
		edata->orig_height = edata->height;
	}
	if (!edata->orig_width || !edata->orig_height ||
	    edata->orig_width > edata->width || edata->orig_height > edata->height) {
		ktxTexture_Destroy(base);
		return 0;
	}

	/* Build [uint32_le size][data] buffer that ogl.c expects */
	size_t total = 0;
	ktx_uint32_t nlev = base->numLevels;
	for (ktx_uint32_t lv = 0; lv < nlev; lv++) {
		ktx_size_t image_size = ktxTexture_GetImageSize(base, lv);
		unsigned int level_width = base->baseWidth >> lv;
		unsigned int level_height = base->baseHeight >> lv;
		size_t expected_size;

		if (!level_width) level_width = 1;
		if (!level_height) level_height = 1;
		expected_size = (size_t) ((level_width + 3u) / 4u) *
		                (size_t) ((level_height + 3u) / 4u) *
		                (fmt ? 16u : 8u);
		if (image_size != expected_size || image_size > UINT_MAX ||
		    total > UINT_MAX - 4u - image_size) {
			ktxTexture_Destroy(base);
			return 0;
		}
		total += 4u + image_size;
	}

	edata->filedata = (unsigned char *) malloc(total);
	if (!edata->filedata) {
		ktxTexture_Destroy(base);
		return 0;
	}
	edata->filedata_size = (unsigned int) total;

	unsigned char *p = edata->filedata;
	for (ktx_uint32_t lv = 0; lv < nlev; lv++) {
		ktx_size_t offset = 0;
		ktxTexture_GetImageOffset(base, lv, 0, 0, &offset);
		ktx_size_t isz = ktxTexture_GetImageSize(base, lv);
		/* uint32 LE size prefix */
		p[0] = (unsigned char) (isz & 0xFF);
		p[1] = (unsigned char) ((isz >> 8) & 0xFF);
		p[2] = (unsigned char) ((isz >> 16) & 0xFF);
		p[3] = (unsigned char) ((isz >> 24) & 0xFF);
		p += 4;
		memcpy(p, base->pData + offset, isz);
		p += isz;
	}

	ktxTexture_Destroy(base);
	return 1;
}
