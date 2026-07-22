/*
 * midi_preview.c -- Standalone MIDI/HMP preview player for the launcher.
 *
 * Mirrors cd_preview.c architecture: OpenSL ES output, render thread,
 * lock-free ring buffer.  Renders MIDI via TinySoundFont (TSF) +
 * TinyMidiLoader (TML).
 *
 * HMP -> MIDI conversion reuses hmp2mid_mem() from d2/misc/hmp.c
 * (and d1/misc/hmp.c), which parses HMP from a memory buffer.
 */

#include "midi_preview.h"
#include "hmp_android_shared.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <android/log.h>
#include <android/asset_manager.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include "u_mem.h"

#define TSF_NO_STDIO
#include "tsf.h"
#define TML_NO_STDIO
#include "tml.h"

#define TAG       "DXX-MidiPreview"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

/* ── Constants ───────────────────────────────────────────────────────── */

#define NUM_BUFFERS 2
#define BUF_FRAMES  1024

/* ── Ring buffer (same lock-free pattern as cd_preview.c) ────────────── */

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

static tsf *s_tsf = NULL;                /* SoundFont synth (persistent)    */
static tml_message *s_midi = NULL;       /* parsed MIDI message list        */
static tml_message *s_midi_cur = NULL;   /* current playback cursor         */
static unsigned char *s_midi_buf = NULL; /* raw MIDI bytes (owned)         */
static int s_midi_buf_len = 0;

static volatile int s_playing = 0;
static volatile int s_paused = 0;
static volatile int s_output_enabled = 0;
static double s_playback_msec = 0.0;
static int s_duration_ms = 0;
static int s_output_rate = 48000;
static float s_volume = 0.7f;
static float s_gain_db = -10.0f;
static int s_max_voices = 48;
static pthread_mutex_t s_control_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t s_playback_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t s_ring_reset_mutex = PTHREAD_MUTEX_INITIALIZER;

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

/* ── Utility ─────────────────────────────────────────────────────────── */

static unsigned int read_le32(const unsigned char *p)
{
	return (unsigned int) p[0] |
	       ((unsigned int) p[1] << 8) |
	       ((unsigned int) p[2] << 16) |
	       ((unsigned int) p[3] << 24);
}

/* ── HOG file reader ─────────────────────────────────────────────────
 *
 * HOG format: 3-byte magic "DHF", then repeated entries of
 *   13-byte filename (null-padded), 4-byte LE length, data bytes.
 *
 * Keep in sync with d2/utilities/hogextract.c format assumptions.
 */

/* Sanity limit: HMP/MIDI tracks in descent HOGs are at most a few hundred
 * KB. Cap well above that to reject obviously corrupt or non-HOG data
 * without preventing legitimate large entries. */
#define HOG_ENTRY_MAX_BYTES (64 * 1024 * 1024)

static long hog_file_size(FILE *fp)
{
	long cur = ftell(fp);
	if (cur < 0 || fseek(fp, 0, SEEK_END) != 0) return -1;
	long end = ftell(fp);
	fseek(fp, cur, SEEK_SET);
	return end;
}

int hog_read_entry(const char *hog_path, const char *entry_name,
                   unsigned char **out_data, int *out_len)
{
	if (!hog_path || !entry_name || !out_data || !out_len) return 0;
	FILE *fp = fopen(hog_path, "rb");
	if (!fp) return 0;

	long file_size = hog_file_size(fp);

	char magic[3];
	if (fread(magic, 1, 3, fp) != 3 || memcmp(magic, "DHF", 3) != 0) {
		fclose(fp);
		return 0;
	}

	while (!feof(fp)) {
		char name[14];
		unsigned char len_bytes[4];
		memset(name, 0, sizeof(name));
		if (fread(name, 1, 13, fp) != 13) break;
		if (fread(len_bytes, 1, 4, fp) != 4) break;
		unsigned int entry_len = read_le32(len_bytes);

		/* Reject unreasonable sizes (corrupt HOG or non-HOG file that
		 * happened to start with DHF). Android: a bogus 4GB malloc here
		 * causes a silent SIGSEGV with no Java crash report. */
		if (entry_len > HOG_ENTRY_MAX_BYTES ||
		    (file_size >= 0 && (long) entry_len > file_size)) {
			LOGI("hog_read_entry: rejecting entry '%s' with bogus length %u", name, entry_len);
			fclose(fp);
			return 0;
		}

		if (strncasecmp(name, entry_name, 13) == 0) {
			unsigned char *data = (unsigned char *) malloc(entry_len);
			if (!data) {
				fclose(fp);
				return 0;
			}
			if (fread(data, 1, entry_len, fp) != entry_len) {
				free(data);
				fclose(fp);
				return 0;
			}
			fclose(fp);
			*out_data = data;
			*out_len = (int) entry_len;
			return 1;
		}
		if (fseek(fp, (long) entry_len, SEEK_CUR) != 0) break;
	}
	fclose(fp);
	return 0;
}

/* List all entries in a HOG file matching a given extension.
 * Returns count of matching entries.  names[i] is a 14-byte buffer.
 * Caller provides names array and max_entries.
 */
int hog_list_entries(const char *hog_path, const char *ext,
                     char (*names)[14], int *sizes, int max_entries)
{
	FILE *fp = fopen(hog_path, "rb");
	if (!fp) return 0;

	char magic[3];
	if (fread(magic, 1, 3, fp) != 3 || memcmp(magic, "DHF", 3) != 0) {
		fclose(fp);
		return 0;
	}

	long file_size = hog_file_size(fp);
	int count = 0;
	int ext_len = ext ? (int) strlen(ext) : 0;
	while (!feof(fp) && count < max_entries) {
		char name[14];
		unsigned char len_bytes[4];
		memset(name, 0, sizeof(name));
		if (fread(name, 1, 13, fp) != 13) break;
		if (fread(len_bytes, 1, 4, fp) != 4) break;
		unsigned int entry_len = read_le32(len_bytes);

		/* Same sanity guard as hog_read_entry. */
		if (entry_len > HOG_ENTRY_MAX_BYTES ||
		    (file_size >= 0 && (long) entry_len > file_size)) {
			LOGI("hog_list_entries: rejecting entry '%s' with bogus length %u", name, entry_len);
			break;
		}

		if (ext && ext_len > 0) {
			int nlen = (int) strlen(name);
			if (nlen > ext_len) {
				const char *dot = name + nlen - ext_len;
				if (strncasecmp(dot, ext, ext_len) == 0) {
					memcpy(names[count], name, 14);
					if (sizes) sizes[count] = (int) entry_len;
					count++;
				}
			}
		} else {
			memcpy(names[count], name, 14);
			if (sizes) sizes[count] = (int) entry_len;
			count++;
		}
		if (fseek(fp, (long) entry_len, SEEK_CUR) != 0) break;
	}
	fclose(fp);
	return count;
}

/* ── MIDI duration calculation ───────────────────────────────────────── */

static int compute_midi_duration_ms(const unsigned char *midi_data, int midi_len)
{
	tml_message *msg = tml_load_memory(midi_data, midi_len);
	if (!msg) return 0;

	/* Walk to the last message to find total duration */
	tml_message *m = msg;
	unsigned int last_time = 0;
	while (m) {
		if (m->time > last_time) last_time = m->time;
		m = m->next;
	}
	tml_free(msg);
	return (int) last_time;
}

/* ── Render function ─────────────────────────────────────────────────── */

static int render_midi_frames(short *out, int frames)
{
	double rate = (double) s_output_rate;
	int rendered = 0;
	enum { BLOCK = 256 };

	while (frames > 0 && s_midi_cur) {
		int block = (frames > BLOCK) ? BLOCK : frames;
		double block_ms = block * (1000.0 / rate);
		s_playback_msec += block_ms;

		/* Dispatch MIDI events up to current time */
		while (s_midi_cur && s_midi_cur->time <= (unsigned int) s_playback_msec) {
			tml_message *m = s_midi_cur;
			switch (m->type) {
				case TML_NOTE_ON:
					tsf_channel_note_on(s_tsf, m->channel, m->key,
					                    m->velocity / 127.0f);
					break;
				case TML_NOTE_OFF:
					tsf_channel_note_off(s_tsf, m->channel, m->key);
					break;
				case TML_PROGRAM_CHANGE:
					tsf_channel_set_presetnumber(s_tsf, m->channel,
					                             m->program,
					                             (m->channel == 9));
					break;
				case TML_CONTROL_CHANGE:
					tsf_channel_midi_control(s_tsf, m->channel,
					                         m->control, m->control_value);
					break;
				case TML_PITCH_BEND:
					tsf_channel_set_pitchwheel(s_tsf, m->channel,
					                           m->pitch_bend);
					break;
				default:
					break;
			}
			s_midi_cur = m->next;
		}

		tsf_render_short(s_tsf, out, block, 0);

		/* Volume scaling */
		if (s_volume < 0.99f) {
			int i, n = block * 2;
			for (i = 0; i < n; i++)
				out[i] = (short) (out[i] * s_volume);
		}

		out += block * 2;
		frames -= block;
		rendered += block;
	}

	/* Song ended */
	if (!s_midi_cur && rendered < frames + rendered) {
		s_playing = 0;
		__atomic_store_n(&s_output_enabled, 0, __ATOMIC_RELEASE);
	}
	return rendered;
}

/* ── Render thread ───────────────────────────────────────────────────── */

static void *render_thread_func(void *data)
{
	enum { CHUNK = 2048 };
	short buf[CHUNK * 2];
	(void) data;

	LOGI("MIDI preview render thread started");

	while (__atomic_load_n(&s_render_running, __ATOMIC_SEQ_CST)) {
		int sleep_usec = 0;
		int stop = 0;
		pthread_mutex_lock(&s_playback_mutex);
		if (!s_playing || s_paused) {
			sleep_usec = 20000;
		} else {
			unsigned int space = RB_SAMPLES - rb_available();
			if (space < CHUNK * 2u) {
				sleep_usec = 5000;
			} else {
				int got = render_midi_frames(buf, CHUNK);
				if (got > 0)
					rb_write(buf, got * 2);
				stop = !s_playing;
			}
		}
		pthread_mutex_unlock(&s_playback_mutex);

		if (stop) break;
		if (sleep_usec) usleep((useconds_t) sleep_usec);
	}

	LOGI("MIDI preview render thread exiting");
	return NULL;
}

static void render_thread_start(void)
{
	if (s_thread_created) return;
	pthread_mutex_lock(&s_ring_reset_mutex);
	rb_reset();
	pthread_mutex_unlock(&s_ring_reset_mutex);
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

/* ── OpenSL ES output (same pattern as cd_preview.c) ─────────────────── */

static void osl_callback(SLAndroidSimpleBufferQueueItf bq, void *ctx)
{
	(void) ctx;
	short *buf = s_play_bufs[s_next_buf];
	int needed = BUF_FRAMES * 2;

	memset(buf, 0, needed * sizeof(short));

	if (__atomic_load_n(&s_output_enabled, __ATOMIC_ACQUIRE) &&
	    pthread_mutex_trylock(&s_ring_reset_mutex) == 0) {
		if (__atomic_load_n(&s_output_enabled, __ATOMIC_RELAXED))
			rb_read(buf, needed);
		pthread_mutex_unlock(&s_ring_reset_mutex);
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

	for (i = 0; i < NUM_BUFFERS; i++) {
		memset(s_play_bufs[i], 0, sizeof(s_play_bufs[i]));
		(*s_player_bq)->Enqueue(s_player_bq, s_play_bufs[i], BUF_FRAMES * 2 * sizeof(short));
	}
	s_next_buf = 0;

	LOGI("OpenSL ES ready: %d Hz stereo", sample_rate);
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

static void midi_preview_stop_internal(void);

int midi_preview_init(AAssetManager *mgr)
{
	pthread_mutex_lock(&s_control_mutex);
	if (s_tsf) {
		pthread_mutex_unlock(&s_control_mutex);
		return 1; /* already initialized */
	}

	if (!mgr) {
		LOGE("No AAssetManager");
		pthread_mutex_unlock(&s_control_mutex);
		return 0;
	}

	AAsset *asset = AAssetManager_open(mgr, "gm.sf2", AASSET_MODE_BUFFER);
	if (!asset) {
		LOGE("gm.sf2 not found in APK assets");
		pthread_mutex_unlock(&s_control_mutex);
		return 0;
	}

	const void *data = AAsset_getBuffer(asset);
	off_t size = AAsset_getLength(asset);

	s_tsf = tsf_load_memory(data, (int) size);
	AAsset_close(asset);

	if (!s_tsf) {
		LOGE("tsf_load_memory failed for gm.sf2");
		pthread_mutex_unlock(&s_control_mutex);
		return 0;
	}

	LOGI("SoundFont loaded (%d presets)", tsf_get_presetcount(s_tsf));
	pthread_mutex_unlock(&s_control_mutex);
	return 1;
}

int midi_preview_start(const unsigned char *data, int len,
                       int is_hmp, int sample_rate)
{
	unsigned char *midi_data = NULL;
	int midi_len = 0;
	pthread_mutex_lock(&s_control_mutex);

	/* Stop any existing preview */
	midi_preview_stop_internal();

	if (!data || len <= 0 || sample_rate <= 0) {
		LOGE("Invalid args");
		pthread_mutex_unlock(&s_control_mutex);
		return 0;
	}
	if (!s_tsf) {
		LOGE("Not initialized -- call midi_preview_init first");
		pthread_mutex_unlock(&s_control_mutex);
		return 0;
	}

	/* Convert HMP to MIDI if needed */
	if (is_hmp) {
		if (!hmp2mid_mem(data, len, &midi_data, &midi_len)) {
			LOGE("HMP -> MIDI conversion failed");
			pthread_mutex_unlock(&s_control_mutex);
			return 0;
		}
	} else {
		/* Standard MIDI: copy the data */
		midi_data = (unsigned char *) d_malloc(len);
		if (!midi_data) {
			pthread_mutex_unlock(&s_control_mutex);
			return 0;
		}
		memcpy(midi_data, data, len);
		midi_len = len;
	}

	/* Parse with TML */
	s_midi = tml_load_memory(midi_data, midi_len);
	if (!s_midi) {
		LOGE("tml_load_memory failed");
		d_free(midi_data);
		pthread_mutex_unlock(&s_control_mutex);
		return 0;
	}

	/* Compute duration */
	s_duration_ms = compute_midi_duration_ms(midi_data, midi_len);

	s_midi_buf = midi_data;
	s_midi_buf_len = midi_len;
	pthread_mutex_lock(&s_playback_mutex);
	s_midi_cur = s_midi;
	s_playback_msec = 0.0;
	s_output_rate = sample_rate;
	s_paused = 0;
	s_playing = 1;
	__atomic_store_n(&s_output_enabled, 1, __ATOMIC_RELEASE);

	/* Configure TSF */
	tsf_reset(s_tsf);
	tsf_set_output(s_tsf, TSF_STEREO_INTERLEAVED, s_output_rate, s_gain_db);
	tsf_set_max_voices(s_tsf, s_max_voices);
	pthread_mutex_unlock(&s_playback_mutex);

	LOGI("Starting MIDI playback (%d bytes, duration=%dms, rate=%d)",
	     midi_len, s_duration_ms, sample_rate);

	/* Init OpenSL ES */
	if (!osl_init(sample_rate)) {
		LOGE("OpenSL ES init failed");
		pthread_mutex_lock(&s_playback_mutex);
		tml_free(s_midi);
		s_midi = NULL;
		s_midi_cur = NULL;
		d_free(s_midi_buf);
		s_playing = 0;
		__atomic_store_n(&s_output_enabled, 0, __ATOMIC_RELEASE);
		pthread_mutex_unlock(&s_playback_mutex);
		pthread_mutex_unlock(&s_control_mutex);
		return 0;
	}

	render_thread_start();
	pthread_mutex_unlock(&s_control_mutex);
	return 1;
}

static void midi_preview_stop_internal(void)
{
	pthread_mutex_lock(&s_playback_mutex);
	s_playing = 0;
	s_paused = 0;
	__atomic_store_n(&s_output_enabled, 0, __ATOMIC_RELEASE);
	pthread_mutex_unlock(&s_playback_mutex);
	render_thread_stop();
	osl_shutdown();

	pthread_mutex_lock(&s_playback_mutex);
	if (s_midi) {
		tml_free(s_midi);
		s_midi = NULL;
		s_midi_cur = NULL;
	}
	if (s_midi_buf) {
		d_free(s_midi_buf);
		s_midi_buf_len = 0;
	}
	rb_reset();
	s_duration_ms = 0;
	s_playback_msec = 0.0;
	pthread_mutex_unlock(&s_playback_mutex);
}

void midi_preview_stop(void)
{
	pthread_mutex_lock(&s_control_mutex);
	midi_preview_stop_internal();
	pthread_mutex_unlock(&s_control_mutex);
}

void midi_preview_pause(void)
{
	pthread_mutex_lock(&s_control_mutex);
	pthread_mutex_lock(&s_playback_mutex);
	s_paused = 1;
	__atomic_store_n(&s_output_enabled, 0, __ATOMIC_RELEASE);
	pthread_mutex_unlock(&s_playback_mutex);
	pthread_mutex_unlock(&s_control_mutex);
}

void midi_preview_resume(void)
{
	pthread_mutex_lock(&s_control_mutex);
	pthread_mutex_lock(&s_playback_mutex);
	s_paused = 0;
	__atomic_store_n(&s_output_enabled, s_playing ? 1 : 0, __ATOMIC_RELEASE);
	pthread_mutex_unlock(&s_playback_mutex);
	pthread_mutex_unlock(&s_control_mutex);
}

int midi_preview_seek(float fraction)
{
	int result = 0;
	pthread_mutex_lock(&s_control_mutex);
	pthread_mutex_lock(&s_playback_mutex);
	if ((!s_playing && !s_paused) || !s_midi || !s_midi_buf)
		goto done;
	if (fraction < 0.0f) fraction = 0.0f;
	if (fraction > 1.0f) fraction = 1.0f;

	double target_ms = fraction * s_duration_ms;

	/* Reset synth and replay from start to target position */
	tsf_reset(s_tsf);
	tsf_set_output(s_tsf, TSF_STEREO_INTERLEAVED, s_output_rate, s_gain_db);
	tsf_set_max_voices(s_tsf, s_max_voices);

	s_midi_cur = s_midi;
	s_playback_msec = 0.0;

	/* Fast-forward: process events without rendering audio */
	while (s_midi_cur && s_midi_cur->time <= (unsigned int) target_ms) {
		tml_message *m = s_midi_cur;
		switch (m->type) {
			case TML_NOTE_ON:
				tsf_channel_note_on(s_tsf, m->channel, m->key, m->velocity / 127.0f);
				break;
			case TML_NOTE_OFF:
				tsf_channel_note_off(s_tsf, m->channel, m->key);
				break;
			case TML_PROGRAM_CHANGE:
				tsf_channel_set_presetnumber(s_tsf, m->channel, m->program, (m->channel == 9));
				break;
			case TML_CONTROL_CHANGE:
				tsf_channel_midi_control(s_tsf, m->channel, m->control, m->control_value);
				break;
			case TML_PITCH_BEND:
				tsf_channel_set_pitchwheel(s_tsf, m->channel, m->pitch_bend);
				break;
			default:
				break;
		}
		s_midi_cur = m->next;
	}

	s_playback_msec = target_ms;
	pthread_mutex_lock(&s_ring_reset_mutex);
	rb_reset();
	pthread_mutex_unlock(&s_ring_reset_mutex);
	result = 1;

done:
	pthread_mutex_unlock(&s_playback_mutex);
	pthread_mutex_unlock(&s_control_mutex);
	return result;
}

int midi_preview_get_state(int *out_position_ms, int *out_duration_ms)
{
	int state;
	pthread_mutex_lock(&s_control_mutex);
	pthread_mutex_lock(&s_playback_mutex);
	if (out_position_ms) {
		int pos = (int) s_playback_msec;
		if (pos > s_duration_ms) pos = s_duration_ms;
		*out_position_ms = pos;
	}
	if (out_duration_ms) *out_duration_ms = s_duration_ms;

	if (s_playing && !s_paused)
		state = MDP_PLAYING;
	else if (s_paused)
		state = MDP_PAUSED;
	else
		state = MDP_STOPPED;
	pthread_mutex_unlock(&s_playback_mutex);
	pthread_mutex_unlock(&s_control_mutex);
	return state;
}
