#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "music_soundfont.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __ANDROID__
#include <android/asset_manager.h>
#include <android/log.h>
#endif

struct sf_table {
	const unsigned char *data;
	size_t count;
	size_t stride;
};
static unsigned sf16(const unsigned char *p)
{
	return p[0] | ((unsigned) p[1] << 8);
}
static uint32_t sf32(const unsigned char *p)
{
	return sf16(p) | ((uint32_t) sf16(p + 2) << 16);
}

static int sf_indices(struct sf_table a, size_t offset, struct sf_table b)
{
	size_t i;
	unsigned previous = 0;
	for (i = 0; i < a.count; ++i) {
		unsigned value = sf16(a.data + i * a.stride + offset);
		if (value < previous || value >= b.count) return 0;
		previous = value;
	}
	return 1;
}

static int sf_region_limit(const struct sf_table *tables)
{
	size_t i, regions = 0;
	unsigned *sample_counts = calloc(tables[7].count, sizeof(*sample_counts));
	int valid = 0;
	if (!sample_counts) return 0;
	/* TSF allocates a region for each sampleID, not each generator (envelope,
	 * filter, key range, etc). Prefix counts keep repeated instruments cheap */
	for (i = 1; i < tables[7].count; ++i)
		sample_counts[i] = sample_counts[i - 1] + (sf16(tables[7].data + (i - 1) * 4) == 53);
	for (i = 0; i + 1 < tables[3].count; ++i) {
		const unsigned char *g = tables[3].data + i * 4;
		if (sf16(g) == 41) {
			unsigned inst = sf16(g + 2), first, last;
			if (inst >= tables[4].count - 1) goto done;
			first = sf16(tables[4].data + inst * 22 + 20);
			last = sf16(tables[4].data + (inst + 1) * 22 + 20);
			first = sf16(tables[5].data + first * 4);
			last = sf16(tables[5].data + last * 4);
			regions += sample_counts[last] - sample_counts[first];
			if (regions > 65536) goto done;
		}
	}
	valid = 1;
done:
	free(sample_counts);
	return valid;
}

size_t music_soundfont_validate(const void *data, size_t size)
{
	static const char *names[] = { "phdr", "pbag", "pmod", "pgen", "inst", "ibag", "imod", "igen", "shdr" };
	static const size_t strides[] = { 38, 4, 10, 4, 22, 4, 10, 4, 46 };
	struct sf_table tables[9] = { { 0 } };
	const unsigned char *bytes = data;
	size_t pos, i, j, samples = 0;
	unsigned lists = 0, version = 0;
	if (!data || size < 12 || size > MUSIC_SOUNDFONT_MAX_BYTES || memcmp(bytes, "RIFF", 4) ||
	    sf32(bytes + 4) != size - 8 || memcmp(bytes + 8, "sfbk", 4)) return 0;
	for (pos = 12; pos < size;) {
		size_t length, end, at;
		unsigned flag;
		if (size - pos < 12 || memcmp(bytes + pos, "LIST", 4)) return 0;
		length = sf32(bytes + pos + 4);
		if (length < 4 || length > size - pos - 8 || (length & 1)) return 0;
		end = pos + 8 + length;
		flag = !memcmp(bytes + pos + 8, "pdta", 4) ? 1 : !memcmp(bytes + pos + 8, "sdta", 4) ? 2
		                                                                                     : 4;
		if (flag == 4 && memcmp(bytes + pos + 8, "INFO", 4)) return 0;
		if (lists & flag) return 0;
		lists |= flag;
		for (at = pos + 12; at < end;) {
			size_t n;
			if (end - at < 8) return 0;
			n = sf32(bytes + at + 4);
			if (n > end - at - 8 || (n & 1)) return 0;
			if (flag == 1) {
				for (i = 0; i < 9; ++i)
					if (!memcmp(bytes + at, names[i], 4)) break;
				if (i == 9 || tables[i].data || !n || n % strides[i] || n / strides[i] > 65536) return 0;
				tables[i] = (struct sf_table) { bytes + at + 8, n / strides[i], strides[i] };
			} else if (flag == 2 && !memcmp(bytes + at, "smpl", 4)) {
				if (samples || n < 4) return 0;
				samples = n / 2;
			} else if (flag == 4 && !memcmp(bytes + at, "ifil", 4)) {
				if (version || n != 4 || sf16(bytes + at + 8) != 2) return 0;
				version = 2;
			}
			at += 8 + n;
		}
		pos = end;
	}
	for (i = 0; i < 9; ++i)
		if (!tables[i].data) return 0;
	if (!version || !samples || tables[0].count < 2 || tables[0].count > 1025 || tables[4].count < 2 || tables[8].count < 2) return 0;
	if (!sf_indices(tables[0], 24, tables[1]) || !sf_indices(tables[1], 0, tables[3]) ||
	    !sf_indices(tables[1], 2, tables[2]) || !sf_indices(tables[4], 20, tables[5]) ||
	    !sf_indices(tables[5], 0, tables[7]) || !sf_indices(tables[5], 2, tables[6])) return 0;
	/* Duplicate bank/program pairs break the pinned parser's sorted preset indexing */
	for (i = 0; i + 1 < tables[0].count; ++i)
		for (j = 0; j < i; ++j)
			if (!memcmp(tables[0].data + i * 38 + 20, tables[0].data + j * 38 + 20, 4)) return 0;
	for (i = 0; i + 1 < tables[8].count; ++i) {
		const unsigned char *s = tables[8].data + i * 46;
		if (sf32(s + 20) >= sf32(s + 24) || sf32(s + 24) > samples || !sf32(s + 36) ||
		    sf32(s + 36) > 384000 || (sf16(s + 44) & 0x8000)) return 0;
	}
	for (i = 0; i + 1 < tables[7].count; ++i) {
		const unsigned char *g = tables[7].data + i * 4;
		if (sf16(g) == 53 && sf16(g + 2) >= tables[8].count - 1) return 0;
	}
	/* Bound instrument expansion before the synth allocates preset regions */
	if (!sf_region_limit(tables)) return 0;
	return samples;
}

void *music_soundfont_read(struct AAssetManager *assets, const char *path, size_t *size)
{
	void *data = NULL;
	*size = 0;
	if (path && *path) {
		FILE *file = fopen(path, "rb");
		long length;
		if (!file) return NULL;
		if (!fseek(file, 0, SEEK_END) && (length = ftell(file)) > 0 &&
		    (unsigned long) length <= MUSIC_SOUNDFONT_MAX_BYTES && !fseek(file, 0, SEEK_SET)) {
			data = malloc((size_t) length);
			if (data && fread(data, 1, (size_t) length, file) == (size_t) length) *size = (size_t) length;
		}
		fclose(file);
	} else {
#ifdef __ANDROID__
		AAsset *asset = assets ? AAssetManager_open(assets, "gm.sf2", AASSET_MODE_BUFFER) : NULL;
		if (asset) {
			const long length = AAsset_getLength(asset);
			const void *buffer = AAsset_getBuffer(asset);
			if (buffer && length > 0 && (unsigned long) length <= MUSIC_SOUNDFONT_MAX_BYTES) {
				data = malloc((size_t) length);
				if (data) {
					memcpy(data, buffer, (size_t) length);
					*size = (size_t) length;
				}
			}
			AAsset_close(asset);
		}
#else
		(void) assets;
#endif
	}
	if (!music_soundfont_validate(data, *size)) {
		free(data);
		*size = 0;
		return NULL;
	}
	return data;
}

tsf *music_soundfont_load(struct AAssetManager *assets, const char *path)
{
	size_t size;
	void *data = music_soundfont_read(assets, path, &size);
	tsf *synth = data ? music_soundfont_load_memory(data, size) : NULL;
	free(data);
	return synth;
}
