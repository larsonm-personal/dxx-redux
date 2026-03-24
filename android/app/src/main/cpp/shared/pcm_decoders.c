/*
 * pcm_decoders.c -- Implementation of unified PCM decoder API.
 * Includes minimp3, stb_vorbis, dr_flac as single-header implementations.
 */

#include "pcm_decoders.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* minimp3 -- MP3 decoder */
#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"
#include "minimp3_ex.h"

/* stb_vorbis -- OGG Vorbis decoder (compiled as separate TU, just declare API) */
extern int stb_vorbis_decode_filename(const char *filename, int *channels,
                                      int *sample_rate, short **output);

/* dr_flac -- FLAC decoder */
#define DR_FLAC_IMPLEMENTATION
#include "dr_flac.h"

/* ---------------------------------------------------------------------- */

static const char *get_extension(const char *path)
{
	const char *dot = strrchr(path, '.');
	return dot ? dot : "";
}

static int strcasecmp_ext(const char *a, const char *b)
{
	while (*a && *b) {
		char ca = *a >= 'A' && *a <= 'Z' ? *a + 32 : *a;
		char cb = *b >= 'A' && *b <= 'Z' ? *b + 32 : *b;
		if (ca != cb) return ca - cb;
		a++;
		b++;
	}
	return (unsigned char) *a - (unsigned char) *b;
}

static int decode_mp3(const char *path, pcm_decode_result_t *out)
{
	mp3dec_t mp3d;
	mp3dec_file_info_t info;
	memset(&info, 0, sizeof(info));

	mp3dec_init(&mp3d);
	if (mp3dec_load(&mp3d, path, &info, NULL, NULL)) {
		if (info.buffer) free(info.buffer);
		return -1;
	}
	if (!info.buffer || info.samples == 0) {
		if (info.buffer) free(info.buffer);
		return -1;
	}

	/* minimp3 outputs interleaved int16_t samples; info.samples is total
	 * sample count across all channels */
	out->pcm_data = (int16_t *) info.buffer;
	out->sample_rate = info.hz;
	out->channels = info.channels;
	out->total_samples = info.samples / info.channels;
	return 0;
}

static int decode_ogg(const char *path, pcm_decode_result_t *out)
{
	int channels = 0, sample_rate = 0;
	short *data = NULL;
	int num_samples = stb_vorbis_decode_filename(path, &channels, &sample_rate, &data);
	if (num_samples <= 0 || !data) {
		if (data) free(data);
		return -1;
	}

	out->pcm_data = data;
	out->sample_rate = sample_rate;
	out->channels = channels;
	out->total_samples = (size_t) num_samples;
	return 0;
}

static int decode_flac(const char *path, pcm_decode_result_t *out)
{
	unsigned int channels = 0, sample_rate = 0;
	drflac_uint64 total_samples = 0;
	drflac_int16 *data = drflac_open_file_and_read_pcm_frames_s16(
	    path, &channels, &sample_rate, &total_samples, NULL);
	if (!data || total_samples == 0) {
		if (data) drflac_free(data, NULL);
		return -1;
	}

	out->pcm_data = data;
	out->sample_rate = (int) sample_rate;
	out->channels = (int) channels;
	out->total_samples = (size_t) total_samples;
	return 0;
}

int pcm_decode_file(const char *path, pcm_decode_result_t *out)
{
	if (!path || !out) return -1;
	memset(out, 0, sizeof(*out));

	const char *ext = get_extension(path);

	if (strcasecmp_ext(ext, ".mp3") == 0)
		return decode_mp3(path, out);
	if (strcasecmp_ext(ext, ".ogg") == 0)
		return decode_ogg(path, out);
	if (strcasecmp_ext(ext, ".flac") == 0)
		return decode_flac(path, out);

	return -1; /* unsupported format */
}

void pcm_decode_free(pcm_decode_result_t *r)
{
	if (r && r->pcm_data) {
		free(r->pcm_data);
		r->pcm_data = NULL;
	}
}

/* CD-DA: 2352 bytes per sector, 588 stereo samples per sector (16-bit LE) */
#define CD_SECTOR_SIZE        2352
#define CD_SAMPLES_PER_SECTOR (CD_SECTOR_SIZE / 4) /* 4 bytes per stereo sample */

int pcm_decode_cd_sectors(int fd, long start_sector, long num_sectors,
                          pcm_decode_result_t *out)
{
	if (fd < 0 || num_sectors <= 0 || !out) return -1;
	memset(out, 0, sizeof(*out));

	size_t total_bytes = (size_t) num_sectors * CD_SECTOR_SIZE;
	size_t total_samples = (size_t) num_sectors * CD_SAMPLES_PER_SECTOR;

	int16_t *buf = (int16_t *) malloc(total_bytes);
	if (!buf) return -1;

	off_t offset = (off_t) start_sector * CD_SECTOR_SIZE;
	if (lseek(fd, offset, SEEK_SET) == (off_t) -1) {
		free(buf);
		return -1;
	}

	size_t bytes_read = 0;
	while (bytes_read < total_bytes) {
		ssize_t n = read(fd, (char *) buf + bytes_read, total_bytes - bytes_read);
		if (n <= 0) break;
		bytes_read += (size_t) n;
	}

	if (bytes_read < total_bytes) {
		/* Partial read -- adjust sample count to what we actually got */
		total_samples = bytes_read / 4;
	}

	out->pcm_data = buf;
	out->sample_rate = 44100;
	out->channels = 2;
	out->total_samples = total_samples;
	return 0;
}

int pcm_decode_cd_sectors_buf(const uint8_t *sector_data, int num_sectors,
                              pcm_decode_result_t *out)
{
	if (!sector_data || num_sectors <= 0 || !out) return -1;
	memset(out, 0, sizeof(*out));

	size_t total_bytes = (size_t) num_sectors * CD_SECTOR_SIZE;
	size_t total_samples = (size_t) num_sectors * CD_SAMPLES_PER_SECTOR;

	int16_t *buf = (int16_t *) malloc(total_bytes);
	if (!buf) return -1;
	memcpy(buf, sector_data, total_bytes);

	out->pcm_data = buf;
	out->sample_rate = 44100;
	out->channels = 2;
	out->total_samples = total_samples;
	return 0;
}
