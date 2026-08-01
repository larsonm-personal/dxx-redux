/*
 * pcm_decoders.c -- Implementation of unified PCM decoder API.
 * Includes minimp3, stb_vorbis, dr_flac as single-header implementations.
 */

#include "pcm_decoders.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#include <sys/types.h>
#define lseek _lseeki64
#define read  _read
typedef int ssize_t;
#else
#include <unistd.h>
#endif

/* minimp3 -- MP3 decoder */
#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"
#include "minimp3_ex.h"

/* stb_vorbis -- OGG Vorbis decoder (compiled as separate TU, just declare API) */
typedef struct stb_vorbis stb_vorbis;
typedef struct {
	unsigned int sample_rate;
	int channels;
	unsigned int setup_memory_required;
	unsigned int setup_temp_memory_required;
	unsigned int temp_memory_required;
	int max_frame_size;
} stb_vorbis_info;
typedef struct {
	char *alloc_buffer;
	int alloc_buffer_length_in_bytes;
} stb_vorbis_alloc;
extern int stb_vorbis_decode_filename(const char *filename, int *channels,
                                      int *sample_rate, short **output);
extern stb_vorbis *stb_vorbis_open_filename(const char *filename, int *error,
                                            const stb_vorbis_alloc *alloc_buffer);
extern stb_vorbis *stb_vorbis_open_memory(const unsigned char *data, int len,
                                          int *error,
                                          const stb_vorbis_alloc *alloc_buffer);
extern stb_vorbis_info stb_vorbis_get_info(stb_vorbis *f);
extern unsigned int stb_vorbis_stream_length_in_samples(stb_vorbis *f);
extern int stb_vorbis_get_samples_short_interleaved(stb_vorbis *f, int channels,
                                                    short *buffer, int num_shorts);
extern void stb_vorbis_close(stb_vorbis *f);

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
	out->pcm_samples = out->total_samples;
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
	out->pcm_samples = out->total_samples;
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
	out->pcm_samples = out->total_samples;
	return 0;
}

static int allocate_prefix(pcm_decode_result_t *out, size_t total_samples,
                           size_t max_seconds, int sample_rate, int channels,
                           size_t *wanted_samples)
{
	size_t wanted;
	size_t max_samples;
	if (!out || total_samples == 0 || max_seconds == 0 ||
	    sample_rate <= 0 || channels <= 0)
		return -1;
	if ((size_t) sample_rate > SIZE_MAX / max_seconds)
		return -1;
	max_samples = (size_t) sample_rate * max_seconds;
	wanted = total_samples < max_samples ? total_samples : max_samples;
	if (wanted > SIZE_MAX / (size_t) channels ||
	    wanted * (size_t) channels > SIZE_MAX / sizeof(int16_t))
		return -1;
	out->pcm_data = (int16_t *) malloc(wanted * (size_t) channels * sizeof(int16_t));
	if (!out->pcm_data) return -1;
	out->sample_rate = sample_rate;
	out->channels = channels;
	out->total_samples = total_samples;
	*wanted_samples = wanted;
	return 0;
}

static int decode_mp3_prefix(const char *path, size_t max_seconds,
                             pcm_decode_result_t *out)
{
	mp3dec_ex_t decoder;
	size_t wanted;
	size_t read_samples;
	memset(&decoder, 0, sizeof(decoder));
	if (mp3dec_ex_open(&decoder, path, MP3D_SEEK_TO_SAMPLE) != 0)
		return -1;
	if (decoder.info.channels <= 0 || decoder.samples == 0 ||
	    decoder.samples % (uint64_t) decoder.info.channels != 0 ||
	    decoder.samples / (uint64_t) decoder.info.channels > SIZE_MAX ||
	    allocate_prefix(out,
	                    (size_t) (decoder.samples / (uint64_t) decoder.info.channels),
	                    max_seconds, decoder.info.hz, decoder.info.channels,
	                    &wanted) != 0) {
		mp3dec_ex_close(&decoder);
		return -1;
	}
	read_samples = mp3dec_ex_read(&decoder, out->pcm_data,
	                              wanted * (size_t) out->channels);
	mp3dec_ex_close(&decoder);
	out->pcm_samples = read_samples / (size_t) out->channels;
	if (out->pcm_samples < wanted) {
		pcm_decode_free(out);
		return -1;
	}
	return 0;
}

static int decode_ogg_prefix_decoder(stb_vorbis *decoder, size_t max_seconds,
                                     pcm_decode_result_t *out)
{
	stb_vorbis_info info;
	size_t wanted;
	size_t decoded = 0;
	unsigned int total;
	if (!decoder) return -1;
	info = stb_vorbis_get_info(decoder);
	total = stb_vorbis_stream_length_in_samples(decoder);
	if (allocate_prefix(out, total, max_seconds, (int) info.sample_rate,
	                    info.channels, &wanted) != 0) {
		stb_vorbis_close(decoder);
		return -1;
	}
	while (decoded < wanted) {
		size_t remaining = wanted - decoded;
		size_t max_frames = (size_t) INT_MAX / (size_t) info.channels;
		int request_frames = (int) (remaining < max_frames ? remaining : max_frames);
		int frames = stb_vorbis_get_samples_short_interleaved(
		    decoder, info.channels,
		    out->pcm_data + decoded * (size_t) info.channels,
		    request_frames * info.channels);
		if (frames <= 0) break;
		decoded += (size_t) frames;
	}
	stb_vorbis_close(decoder);
	out->pcm_samples = decoded;
	if (decoded < wanted) {
		pcm_decode_free(out);
		return -1;
	}
	return 0;
}

static int decode_ogg_prefix(const char *path, size_t max_seconds,
                             pcm_decode_result_t *out)
{
	int error = 0;
	return decode_ogg_prefix_decoder(stb_vorbis_open_filename(path, &error, NULL),
	                                 max_seconds, out);
}

static int decode_flac_prefix_decoder(drflac *decoder, size_t max_seconds,
                                      pcm_decode_result_t *out)
{
	size_t wanted;
	drflac_uint64 decoded;
	if (!decoder) return -1;
	if (decoder->totalPCMFrameCount > SIZE_MAX ||
	    allocate_prefix(out, (size_t) decoder->totalPCMFrameCount, max_seconds,
	                    (int) decoder->sampleRate, (int) decoder->channels,
	                    &wanted) != 0) {
		drflac_close(decoder);
		return -1;
	}
	decoded = drflac_read_pcm_frames_s16(decoder, wanted, out->pcm_data);
	drflac_close(decoder);
	out->pcm_samples = (size_t) decoded;
	if (out->pcm_samples < wanted) {
		pcm_decode_free(out);
		return -1;
	}
	return 0;
}

static int decode_flac_prefix(const char *path, size_t max_seconds,
                              pcm_decode_result_t *out)
{
	return decode_flac_prefix_decoder(drflac_open_file(path, NULL), max_seconds, out);
}

int pcm_decode_file_prefix(const char *path, size_t max_seconds,
                           pcm_decode_result_t *out)
{
	const char *ext;
	if (!path || !out || max_seconds == 0) return -1;
	memset(out, 0, sizeof(*out));
	ext = get_extension(path);
	if (strcasecmp_ext(ext, ".mp3") == 0)
		return decode_mp3_prefix(path, max_seconds, out);
	if (strcasecmp_ext(ext, ".ogg") == 0)
		return decode_ogg_prefix(path, max_seconds, out);
	if (strcasecmp_ext(ext, ".flac") == 0)
		return decode_flac_prefix(path, max_seconds, out);
	return -1;
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

/* ── Memory-based decoders ──────────────────────────────────────────── */

/* stb_vorbis memory API (compiled as separate TU in dxx_fingerprint) */
extern int stb_vorbis_decode_memory(const unsigned char *mem, int len,
                                    int *channels, int *sample_rate,
                                    short **output);

static int decode_mp3_mem(const void *data, size_t size, pcm_decode_result_t *out)
{
	mp3dec_t mp3d;
	mp3dec_file_info_t info;
	memset(&info, 0, sizeof(info));
	mp3dec_init(&mp3d);
	if (mp3dec_load_buf(&mp3d, (const uint8_t *) data, size, &info, NULL, NULL)) {
		if (info.buffer) free(info.buffer);
		return -1;
	}
	if (!info.buffer || info.samples == 0) {
		if (info.buffer) free(info.buffer);
		return -1;
	}
	out->pcm_data = (int16_t *) info.buffer;
	out->sample_rate = info.hz;
	out->channels = info.channels;
	out->total_samples = info.samples / info.channels;
	out->pcm_samples = out->total_samples;
	return 0;
}

static int decode_ogg_mem(const void *data, size_t size, pcm_decode_result_t *out)
{
	int channels = 0, sample_rate = 0;
	short *pcm = NULL;
	int n = stb_vorbis_decode_memory((const unsigned char *) data, (int) size,
	                                 &channels, &sample_rate, &pcm);
	if (n <= 0 || !pcm) {
		if (pcm) free(pcm);
		return -1;
	}
	out->pcm_data = pcm;
	out->sample_rate = sample_rate;
	out->channels = channels;
	out->total_samples = (size_t) n;
	out->pcm_samples = out->total_samples;
	return 0;
}

static int decode_flac_mem(const void *data, size_t size, pcm_decode_result_t *out)
{
	unsigned int channels = 0, sample_rate = 0;
	drflac_uint64 total = 0;
	drflac_int16 *pcm = drflac_open_memory_and_read_pcm_frames_s16(
	    data, size, &channels, &sample_rate, &total, NULL);
	if (!pcm || total == 0) {
		if (pcm) drflac_free(pcm, NULL);
		return -1;
	}
	out->pcm_data = pcm;
	out->sample_rate = (int) sample_rate;
	out->channels = (int) channels;
	out->total_samples = (size_t) total;
	out->pcm_samples = out->total_samples;
	return 0;
}

int pcm_decode_memory(const void *data, size_t size, const char *ext,
                      pcm_decode_result_t *out)
{
	if (!data || !size || !ext || !out) return -1;
	memset(out, 0, sizeof(*out));

	if (strcasecmp_ext(ext, ".mp3") == 0)
		return decode_mp3_mem(data, size, out);
	if (strcasecmp_ext(ext, ".ogg") == 0)
		return decode_ogg_mem(data, size, out);
	if (strcasecmp_ext(ext, ".flac") == 0)
		return decode_flac_mem(data, size, out);

	return -1;
}

int pcm_decode_memory_prefix(const void *data, size_t size, const char *ext,
                             size_t max_seconds, pcm_decode_result_t *out)
{
	if (!data || size == 0 || !ext || !out || max_seconds == 0)
		return -1;
	memset(out, 0, sizeof(*out));
	if (strcasecmp_ext(ext, ".mp3") == 0) {
		mp3dec_ex_t decoder;
		size_t wanted;
		size_t read_samples;
		memset(&decoder, 0, sizeof(decoder));
		if (mp3dec_ex_open_buf(&decoder, data, size, MP3D_SEEK_TO_SAMPLE) != 0)
			return -1;
		if (decoder.info.channels <= 0 || decoder.samples == 0 ||
		    decoder.samples % (uint64_t) decoder.info.channels != 0 ||
		    decoder.samples / (uint64_t) decoder.info.channels > SIZE_MAX ||
		    allocate_prefix(out,
		                    (size_t) (decoder.samples / (uint64_t) decoder.info.channels),
		                    max_seconds, decoder.info.hz, decoder.info.channels,
		                    &wanted) != 0) {
			mp3dec_ex_close(&decoder);
			return -1;
		}
		read_samples = mp3dec_ex_read(&decoder, out->pcm_data,
		                              wanted * (size_t) out->channels);
		mp3dec_ex_close(&decoder);
		out->pcm_samples = read_samples / (size_t) out->channels;
		if (out->pcm_samples < wanted) {
			pcm_decode_free(out);
			return -1;
		}
		return 0;
	}
	if (strcasecmp_ext(ext, ".ogg") == 0) {
		int error = 0;
		if (size > INT_MAX) return -1;
		return decode_ogg_prefix_decoder(
		    stb_vorbis_open_memory(data, (int) size, &error, NULL),
		    max_seconds, out);
	}
	if (strcasecmp_ext(ext, ".flac") == 0)
		return decode_flac_prefix_decoder(drflac_open_memory(data, size, NULL),
		                                  max_seconds, out);
	return -1;
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
	out->pcm_samples = total_samples;
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
	out->pcm_samples = total_samples;
	return 0;
}
