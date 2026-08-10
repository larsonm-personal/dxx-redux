/*
 * cd_preview.c -- Standalone CD audio preview player for the launcher.
 *
 * Plays BIN/CUE audio tracks using direct file I/O and OpenSL ES,
 * without requiring SDL or PHYSFS.  Reuses the same sector format,
 * PCM decoding, and linear-interpolation resampling approach from
 * rbaudio_bin.c so that bugs in the core audio pipeline surface in
 * both the in-game player and the launcher preview.
 *
 * Thread model:
 *   - Main thread: JNI calls (start/stop/pause/seek/get_state)
 *   - Render thread (pthread): reads BIN, decodes PCM, resamples,
 *     writes to lock-free ring buffer
 *   - OpenSL ES callback thread: reads ring buffer, outputs audio
 */

#include "cd_preview.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <unistd.h>
#include <android/log.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include "../extract/cd_read_contract.h"
#include "../extract/cue_parser.h"

#define TAG       "DXX-CdPreview"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

/* ── Constants (kept in sync with rbaudio_bin.c) ─────────────────────── */

#define SECTOR_SIZE       2352
#define CD_SAMPLE_RATE    44100
#define FRAMES_PER_SECTOR (SECTOR_SIZE / 4) /* 588 stereo frames */

#define NUM_BUFFERS 2
#define BUF_FRAMES  1024 /* frames per OpenSL buffer */

typedef struct {
	FILE *fp;
} preview_bin_handle_t;

/* ── Ring buffer (same lock-free pattern as rbaudio_bin.c) ───────────── */

#define RB_SHIFT   18
#define RB_SAMPLES (1 << RB_SHIFT) /* 262144 samples */
#define RB_MASK    (RB_SAMPLES - 1)

static short s_rb[RB_SAMPLES];
static volatile int s_rb_wpos = 0;
static volatile int s_rb_rpos = 0;

static void rb_reset(void)
{
	__atomic_store_n(&s_rb_wpos, 0, __ATOMIC_SEQ_CST);
	__atomic_store_n(&s_rb_rpos, 0, __ATOMIC_SEQ_CST);
}

static void rb_write(const short *data, int count)
{
	unsigned int wpos = (unsigned int) __atomic_load_n(&s_rb_wpos, __ATOMIC_RELAXED);
	unsigned int idx = wpos & RB_MASK;
	int first = RB_SAMPLES - (int) idx;
	if (first > count) first = count;
	memcpy(&s_rb[idx], data, first * sizeof(short));
	if (count > first)
		memcpy(&s_rb[0], data + first, (count - first) * sizeof(short));
	__atomic_store_n(&s_rb_wpos, (int) (wpos + (unsigned int) count), __ATOMIC_RELEASE);
}

static int rb_read(short *out, int count)
{
	unsigned int wpos = (unsigned int) __atomic_load_n(&s_rb_wpos, __ATOMIC_ACQUIRE);
	unsigned int rpos = (unsigned int) __atomic_load_n(&s_rb_rpos, __ATOMIC_RELAXED);
	unsigned int avail = wpos - rpos;
	if ((int) avail < count) count = (int) avail;
	if (count <= 0) return 0;
	unsigned int idx = rpos & RB_MASK;
	int first = RB_SAMPLES - (int) idx;
	if (first > count) first = count;
	memcpy(out, &s_rb[idx], first * sizeof(short));
	if (count > first)
		memcpy(out + first, &s_rb[0], (count - first) * sizeof(short));
	__atomic_store_n(&s_rb_rpos, (int) (rpos + (unsigned int) count), __ATOMIC_RELEASE);
	return count;
}

static unsigned int rb_available(void)
{
	return (unsigned int) __atomic_load_n(&s_rb_wpos, __ATOMIC_ACQUIRE) -
	       (unsigned int) __atomic_load_n(&s_rb_rpos, __ATOMIC_ACQUIRE);
}

/* ── Playback state ──────────────────────────────────────────────────── */

static preview_bin_handle_t s_bin_files[CUE_MAX_FILES];
static int s_num_bin_files = 0;
static volatile int s_playing = 0;
static volatile int s_paused = 0;

static int s_start_sector = 0;     /* first sector of current track */
static int s_read_sector = 0;      /* next sector to read */
static int s_track_end = 0;        /* sector past end */
static int s_num_sectors = 0;      /* total sectors in track */
static int s_track_file_index = 0; /* BIN file that owns the current track */
static int s_output_rate = 48000;
static float s_volume = 0.8f;

/* PCM input buffer (same pattern as rbaudio_bin.c) */
#define PCM_BUF_FRAMES (FRAMES_PER_SECTOR * 16) /* 9408 frames */
static short s_pcm_buf[PCM_BUF_FRAMES * 2];
static int s_pcm_len = 0;
static int s_pcm_pos = 0;
static double s_resample_frac = 0.0;

/* Output frame counter for position tracking */
static volatile long long s_output_frames = 0;

/* Render thread */
static pthread_t s_render_tid;
static volatile int s_render_running = 0;
static int s_thread_created = 0;

/* OpenSL ES objects */
static SLObjectItf s_engine_obj = NULL;
static SLEngineItf s_engine = NULL;
static SLObjectItf s_outmix_obj = NULL;
static SLObjectItf s_player_obj = NULL;
static SLPlayItf s_player_play = NULL;
static SLAndroidSimpleBufferQueueItf s_player_bq = NULL;
static short s_play_bufs[NUM_BUFFERS][BUF_FRAMES * 2];
static int s_next_buf = 0;

/* ── BIN/CUE helpers ─────────────────────────────────────────────────── */

static void close_bin_files(void)
{
	int i;
	for (i = 0; i < s_num_bin_files; i++) {
		if (s_bin_files[i].fp) {
			fclose(s_bin_files[i].fp);
			s_bin_files[i].fp = NULL;
		}
	}
	s_num_bin_files = 0;
	s_track_file_index = 0;
}

static FILE *current_track_fp(void)
{
	if (s_track_file_index < 0 || s_track_file_index >= s_num_bin_files)
		return NULL;
	return s_bin_files[s_track_file_index].fp;
}

static char *read_cue_text(const char *cue_path)
{
	char *buf = NULL;

	if (!cd_read_file_exact(cue_path, CD_CUE_MAX_BYTES, &buf, NULL)) {
		LOGE("Cannot open CUE: %s", cue_path);
		return NULL;
	}
	return buf;
}

/* ── PCM decode + resample (same approach as rbaudio_bin.c) ──────────── */

static int refill_pcm(void)
{
	int frames_read = 0;

	if (s_pcm_pos > 0 && s_pcm_pos < s_pcm_len) {
		int remaining = s_pcm_len - s_pcm_pos;
		memmove(s_pcm_buf, s_pcm_buf + s_pcm_pos * 2, remaining * 2 * sizeof(short));
		s_pcm_len = remaining;
	} else if (s_pcm_pos >= s_pcm_len) {
		s_pcm_len = 0;
	}
	s_pcm_pos = 0;

	while (s_pcm_len + FRAMES_PER_SECTOR <= PCM_BUF_FRAMES) {
		unsigned char raw[SECTOR_SIZE];
		long offset;
		int i;
		FILE *bin_fp;

		if (s_read_sector >= s_track_end)
			return frames_read;

		bin_fp = current_track_fp();
		if (!bin_fp) break;
		offset = (long) s_read_sector * SECTOR_SIZE;
		if (fseek(bin_fp, offset, SEEK_SET) != 0) break;
		if (fread(raw, SECTOR_SIZE, 1, bin_fp) != 1) break;

		/* Decode 16-bit LE stereo PCM (same as rbaudio_bin.c) */
		for (i = 0; i < FRAMES_PER_SECTOR; i++) {
			int b = i * 4;
			s_pcm_buf[(s_pcm_len + i) * 2] =
			    (short) ((unsigned short) raw[b] | ((unsigned short) raw[b + 1] << 8));
			s_pcm_buf[(s_pcm_len + i) * 2 + 1] =
			    (short) ((unsigned short) raw[b + 2] | ((unsigned short) raw[b + 3] << 8));
		}
		s_pcm_len += FRAMES_PER_SECTOR;
		frames_read += FRAMES_PER_SECTOR;
		s_read_sector++;
	}
	return frames_read;
}

static int render_cd_frames(short *out, int max_frames)
{
	const double ratio = (double) CD_SAMPLE_RATE / (double) s_output_rate;
	int written = 0;

	while (written < max_frames && s_playing) {
		if (s_pcm_pos >= s_pcm_len - 1) {
			if (refill_pcm() == 0 && s_pcm_pos >= s_pcm_len - 1) {
				s_playing = 0;
				break;
			}
		}

		/* Linear interpolation (same as rbaudio_bin.c) */
		{
			int p0 = s_pcm_pos + (int) s_resample_frac;
			double frac = s_resample_frac - (int) s_resample_frac;

			if (p0 + 1 < s_pcm_len) {
				short l0 = s_pcm_buf[p0 * 2], r0 = s_pcm_buf[p0 * 2 + 1];
				short l1 = s_pcm_buf[(p0 + 1) * 2], r1 = s_pcm_buf[(p0 + 1) * 2 + 1];
				out[written * 2] = (short) (l0 + (int) ((l1 - l0) * frac));
				out[written * 2 + 1] = (short) (r0 + (int) ((r1 - r0) * frac));
			} else if (p0 < s_pcm_len) {
				out[written * 2] = s_pcm_buf[p0 * 2];
				out[written * 2 + 1] = s_pcm_buf[p0 * 2 + 1];
			} else {
				out[written * 2] = out[written * 2 + 1] = 0;
			}
		}

		written++;
		s_resample_frac += ratio;
		{
			int consumed = (int) s_resample_frac;
			s_pcm_pos += consumed;
			s_resample_frac -= consumed;
		}
	}
	return written;
}

/* ── Render thread (pthread, same pattern as rbaudio_bin.c) ──────────── */

static void *render_thread_func(void *data)
{
	enum { CHUNK = 2048 };
	short buf[CHUNK * 2];
	(void) data;

	LOGI("Preview render thread started");

	while (__atomic_load_n(&s_render_running, __ATOMIC_SEQ_CST)) {
		if (!s_playing || s_paused) {
			usleep(20000);
			continue;
		}

		unsigned int space = RB_SAMPLES - rb_available();
		if (space < CHUNK * 2u) {
			usleep(5000);
			continue;
		}

		int got = render_cd_frames(buf, CHUNK);
		if (got > 0)
			rb_write(buf, got * 2);

		if (!s_playing) break;
	}

	LOGI("Preview render thread exiting");
	return NULL;
}

static void render_thread_start(void)
{
	if (s_thread_created) return;
	rb_reset();
	__atomic_store_n(&s_render_running, 1, __ATOMIC_SEQ_CST);
	if (pthread_create(&s_render_tid, NULL, render_thread_func, NULL) == 0)
		s_thread_created = 1;
	else
		LOGE("Failed to create render thread");
}

static void render_thread_stop(void)
{
	if (!s_thread_created) return;
	__atomic_store_n(&s_render_running, 0, __ATOMIC_SEQ_CST);
	pthread_join(s_render_tid, NULL);
	s_thread_created = 0;
}

/* ── OpenSL ES output ────────────────────────────────────────────────── */

static void osl_callback(SLAndroidSimpleBufferQueueItf bq, void *ctx)
{
	(void) ctx;
	short *buf = s_play_bufs[s_next_buf];
	int needed = BUF_FRAMES * 2; /* stereo samples */

	memset(buf, 0, needed * sizeof(short));

	if (s_playing && !s_paused) {
		int got = rb_read(buf, needed);
		/* Volume scaling */
		if (s_volume < 0.99f && got > 0) {
			int i;
			for (i = 0; i < got; i++)
				buf[i] = (short) (buf[i] * s_volume);
		}
		if (got > 0)
			__atomic_fetch_add(&s_output_frames, got / 2, __ATOMIC_RELAXED);
	}

	(*bq)->Enqueue(bq, buf, needed * sizeof(short));
	s_next_buf = (s_next_buf + 1) % NUM_BUFFERS;
}

static int osl_init(int sample_rate)
{
	SLresult r;
	int i;

	r = slCreateEngine(&s_engine_obj, 0, NULL, 0, NULL, NULL);
	if (r != SL_RESULT_SUCCESS) {
		LOGE("slCreateEngine: %d", (int) r);
		return 0;
	}
	(*s_engine_obj)->Realize(s_engine_obj, SL_BOOLEAN_FALSE);
	(*s_engine_obj)->GetInterface(s_engine_obj, SL_IID_ENGINE, &s_engine);

	r = (*s_engine)->CreateOutputMix(s_engine, &s_outmix_obj, 0, NULL, NULL);
	if (r != SL_RESULT_SUCCESS) {
		LOGE("CreateOutputMix: %d", (int) r);
		return 0;
	}
	(*s_outmix_obj)->Realize(s_outmix_obj, SL_BOOLEAN_FALSE);

	SLDataLocator_AndroidSimpleBufferQueue loc_bq = {
		SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, NUM_BUFFERS
	};
	SLDataFormat_PCM fmt = {
		SL_DATAFORMAT_PCM, 2,
		(SLuint32) (sample_rate * 1000),
		SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16,
		SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,
		SL_BYTEORDER_LITTLEENDIAN
	};
	SLDataSource src = { &loc_bq, &fmt };
	SLDataLocator_OutputMix loc_out = { SL_DATALOCATOR_OUTPUTMIX, s_outmix_obj };
	SLDataSink snk = { &loc_out, NULL };

	const SLInterfaceID ids[1] = { SL_IID_BUFFERQUEUE };
	const SLboolean req[1] = { SL_BOOLEAN_TRUE };

	r = (*s_engine)->CreateAudioPlayer(s_engine, &s_player_obj,
	                                   &src, &snk, 1, ids, req);
	if (r != SL_RESULT_SUCCESS) {
		LOGE("CreateAudioPlayer: %d", (int) r);
		return 0;
	}
	(*s_player_obj)->Realize(s_player_obj, SL_BOOLEAN_FALSE);
	(*s_player_obj)->GetInterface(s_player_obj, SL_IID_PLAY, &s_player_play);
	(*s_player_obj)->GetInterface(s_player_obj, SL_IID_BUFFERQUEUE, &s_player_bq);

	(*s_player_bq)->RegisterCallback(s_player_bq, osl_callback, NULL);
	(*s_player_play)->SetPlayState(s_player_play, SL_PLAYSTATE_PLAYING);

	/* Pre-enqueue silence to start the callback chain */
	for (i = 0; i < NUM_BUFFERS; i++) {
		memset(s_play_bufs[i], 0, sizeof(s_play_bufs[i]));
		(*s_player_bq)->Enqueue(s_player_bq, s_play_bufs[i], BUF_FRAMES * 2 * sizeof(short));
	}
	s_next_buf = 0;

	LOGI("OpenSL ES ready: %d Hz stereo, %d frames/buf", sample_rate, BUF_FRAMES);
	return 1;
}

static void osl_shutdown(void)
{
	if (s_player_obj) {
		(*s_player_play)->SetPlayState(s_player_play, SL_PLAYSTATE_STOPPED);
		(*s_player_obj)->Destroy(s_player_obj);
		s_player_obj = NULL;
		s_player_play = NULL;
		s_player_bq = NULL;
	}
	if (s_outmix_obj) {
		(*s_outmix_obj)->Destroy(s_outmix_obj);
		s_outmix_obj = NULL;
	}
	if (s_engine_obj) {
		(*s_engine_obj)->Destroy(s_engine_obj);
		s_engine_obj = NULL;
		s_engine = NULL;
	}
}

/* ── Public API ──────────────────────────────────────────────────────── */

/* Common start logic after all BIN handles are already open */
static int cd_preview_start_common(const char *cue_path,
                                   int audio_track_1based, int sample_rate)
{
	cue_disc_t disc;
	char *cue_text = NULL;
	long long bin_sizes[CUE_MAX_FILES];
	int count, audio_idx, i;

	memset(&disc, 0, sizeof(disc));
	memset(bin_sizes, 0, sizeof(bin_sizes));

	cue_text = read_cue_text(cue_path);
	if (!cue_text) {
		close_bin_files();
		return 0;
	}

	count = cue_parse(cue_text, NULL, 0, &disc);
	if (count <= 0 || disc.num_tracks <= 0) {
		LOGE("No tracks in CUE");
		free(cue_text);
		close_bin_files();
		return 0;
	}
	if (disc.num_files <= 0 || disc.num_files > s_num_bin_files) {
		LOGE("CUE expects %d BIN files, preview has %d", disc.num_files, s_num_bin_files);
		free(cue_text);
		close_bin_files();
		return 0;
	}

	for (i = 0; i < disc.num_files; i++) {
		long size;
		if (!s_bin_files[i].fp) {
			free(cue_text);
			close_bin_files();
			return 0;
		}
		if (fseek(s_bin_files[i].fp, 0, SEEK_END) != 0) {
			free(cue_text);
			close_bin_files();
			return 0;
		}
		size = ftell(s_bin_files[i].fp);
		if (size <= 0 || fseek(s_bin_files[i].fp, 0, SEEK_SET) != 0) {
			LOGE("Invalid BIN size for file %d", i);
			free(cue_text);
			close_bin_files();
			return 0;
		}
		bin_sizes[i] = size;
	}

	memset(&disc, 0, sizeof(disc));
	count = cue_parse(cue_text, bin_sizes, s_num_bin_files, &disc);
	free(cue_text);
	if (count <= 0 || disc.num_tracks <= 0) {
		LOGE("Failed to compute track lengths from CUE");
		close_bin_files();
		return 0;
	}

	/* Find the Nth audio track */
	audio_idx = 0;
	for (i = 0; i < disc.num_tracks; i++) {
		if (disc.tracks[i].type == CUE_TRACK_AUDIO) {
			audio_idx++;
			if (audio_idx == audio_track_1based) break;
		}
	}
	if (i >= disc.num_tracks || disc.tracks[i].type != CUE_TRACK_AUDIO) {
		LOGE("Audio track %d not found (%d total CUE tracks)", audio_track_1based, disc.num_tracks);
		close_bin_files();
		return 0;
	}

	s_track_file_index = disc.tracks[i].file_index;
	s_start_sector = disc.tracks[i].start_sector;
	s_read_sector = s_start_sector;
	s_num_sectors = disc.tracks[i].num_sectors;
	s_track_end = s_start_sector + s_num_sectors;
	s_output_rate = sample_rate;
	s_pcm_len = 0;
	s_pcm_pos = 0;
	s_resample_frac = 0.0;
	s_output_frames = 0;
	s_paused = 0;
	s_playing = 1;

	LOGI("Starting track %d (audio #%d): file=%d sectors %d-%d (%d), rate=%d",
	     i + 1, audio_track_1based, s_track_file_index, s_start_sector, s_track_end - 1,
	     s_num_sectors, sample_rate);

	/* Init OpenSL ES output */
	if (!osl_init(sample_rate)) {
		LOGE("OpenSL ES init failed");
		close_bin_files();
		s_playing = 0;
		return 0;
	}

	/* Start render thread */
	render_thread_start();

	return 1;
}

static int open_bin_path_for_preview(const char *bin_path)
{
	FILE *fp;
	if (!bin_path) return 0;
	fp = fopen(bin_path, "rb");
	if (!fp) {
		LOGE("Cannot open BIN: %s", bin_path);
		return 0;
	}
	s_bin_files[s_num_bin_files++].fp = fp;
	return 1;
}

static int open_bin_fd_for_preview(int fd)
{
	int duped;
	FILE *fp;
	if (fd < 0) return 0;
	duped = dup(fd);
	if (duped < 0) {
		LOGE("dup(fd=%d) failed", fd);
		return 0;
	}
	fp = fdopen(duped, "rb");
	if (!fp) {
		LOGE("fdopen(fd=%d) failed", duped);
		close(duped);
		return 0;
	}
	s_bin_files[s_num_bin_files++].fp = fp;
	return 1;
}

int cd_preview_start(const char *bin_path, const char *cue_path,
                     int audio_track_1based, int sample_rate)
{
	const char *bins[1] = { bin_path };
	return cd_preview_start_multi(bins, 1, cue_path, audio_track_1based, sample_rate);
}

int cd_preview_start_multi(const char *const *bin_paths, int num_bins,
                           const char *cue_path,
                           int audio_track_1based, int sample_rate)
{
	int i;

	/* Stop any existing preview */
	cd_preview_stop();

	if (!bin_paths || num_bins < 1 || num_bins > CUE_MAX_FILES || !cue_path ||
	    audio_track_1based < 1 || sample_rate <= 0) {
		LOGE("Invalid args: bins=%d cue=%s track=%d rate=%d",
		     num_bins, cue_path ? cue_path : "NULL",
		     audio_track_1based, sample_rate);
		return 0;
	}

	for (i = 0; i < num_bins; i++) {
		if (!open_bin_path_for_preview(bin_paths[i])) {
			close_bin_files();
			return 0;
		}
	}

	return cd_preview_start_common(cue_path, audio_track_1based, sample_rate);
}

int cd_preview_start_fd(int fd, const char *cue_path,
                        int audio_track_1based, int sample_rate)
{
	const int fds[1] = { fd };
	return cd_preview_start_multi_fd(fds, 1, cue_path, audio_track_1based, sample_rate);
}

int cd_preview_start_multi_fd(const int *fds, int num_fds,
                              const char *cue_path,
                              int audio_track_1based, int sample_rate)
{
	int i;

	/* Stop any existing preview */
	cd_preview_stop();

	if (!fds || num_fds < 1 || num_fds > CUE_MAX_FILES || !cue_path ||
	    audio_track_1based < 1 || sample_rate <= 0) {
		LOGE("Invalid args: fds=%d cue=%s track=%d rate=%d",
		     num_fds, cue_path ? cue_path : "NULL",
		     audio_track_1based, sample_rate);
		return 0;
	}

	for (i = 0; i < num_fds; i++) {
		if (!open_bin_fd_for_preview(fds[i])) {
			close_bin_files();
			return 0;
		}
	}

	return cd_preview_start_common(cue_path, audio_track_1based, sample_rate);
}

void cd_preview_stop(void)
{
	s_playing = 0;
	s_paused = 0;
	render_thread_stop();
	osl_shutdown();
	close_bin_files();
	rb_reset();
}

void cd_preview_pause(void)
{
	s_paused = 1;
}

void cd_preview_resume(void)
{
	s_paused = 0;
}

int cd_preview_seek(float fraction)
{
	int target;

	if (!s_playing && !s_paused) return 0;
	if (fraction < 0.0f) fraction = 0.0f;
	if (fraction > 1.0f) fraction = 1.0f;

	target = s_start_sector + (int) (fraction * s_num_sectors);
	if (target >= s_track_end) target = s_track_end - 1;
	if (target < s_start_sector) target = s_start_sector;

	/* Reset PCM state and ring buffer */
	s_read_sector = target;
	s_pcm_len = 0;
	s_pcm_pos = 0;
	s_resample_frac = 0.0;
	rb_reset();

	/* Update output frame counter to reflect seek position */
	{
		long long sectors_in = target - s_start_sector;
		long long input_frames = sectors_in * FRAMES_PER_SECTOR;
		long long output_frames = (input_frames * s_output_rate) / CD_SAMPLE_RATE;
		__atomic_store_n(&s_output_frames, output_frames, __ATOMIC_RELAXED);
	}

	return 1;
}

int cd_preview_get_state(int *out_position_ms, int *out_duration_ms)
{
	/* Duration from sector count */
	int duration_ms = (int) ((long long) s_num_sectors * FRAMES_PER_SECTOR * 1000 /
	                         CD_SAMPLE_RATE);

	/* Position from output frame counter */
	long long frames = __atomic_load_n(&s_output_frames, __ATOMIC_RELAXED);
	int position_ms = (s_output_rate > 0)
	                      ? (int) (frames * 1000 / s_output_rate)
	                      : 0;
	if (position_ms > duration_ms) position_ms = duration_ms;

	if (out_position_ms) *out_position_ms = position_ms;
	if (out_duration_ms) *out_duration_ms = duration_ms;

	if (s_playing && !s_paused) return CDP_PLAYING;
	if (s_paused) return CDP_PAUSED;
	return CDP_STOPPED;
}
