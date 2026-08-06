/*
 * MIDI music playback using TinySoundFont + TinyMidiLoader.
 *
 * Replaces digi_mixer_music.c on Android (or any platform without a
 * working SDL_mixer MIDI backend).  HMP files are converted to standard
 * MIDI in memory (via hmp2mid), parsed with TinyMidiLoader, then rendered
 * to PCM through TinySoundFont feeding Mix_HookMusic().
 *
 * Requires:
 *   - tsf.h  (TinySoundFont — SF2 synth, single-header C library)
 *   - tml.h  (TinyMidiLoader — MIDI parser, single-header C library)
 *   - A General MIDI .sf2 soundfont available at runtime
 */

#include <SDL.h>
#include <SDL_mixer.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <physfs.h>

#ifdef ANDROID
#include <android/log.h>
#include <android/asset_manager.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <unistd.h>
#include "android_crash_handler.h"
#include "android_lifecycle_diagnostics.h"
#define TSFMUSIC_LOG(...) __android_log_print(ANDROID_LOG_INFO, "TSF-Music", __VA_ARGS__)
#else
#define TSFMUSIC_LOG(...) ((void) 0)
#endif

/* TinySoundFont — implementation in libtsf.so */
#define TSF_NO_STDIO
#include "tsf.h"

/* TinyMidiLoader — implementation in libtsf.so */
#define TML_NO_STDIO
#include "tml.h"

#include "args.h"
#include "hmp.h"
#include "digi_mixer_music.h"
#include "u_mem.h"
#include "console.h"

/* ── Globals ─────────────────────────────────────────────────────────── */

static tsf *g_tsf = NULL;              /* SoundFont synth instance       */
static tml_message *g_midi = NULL;     /* parsed MIDI message list       */
static tml_message *g_midi_cur = NULL; /* current playback cursor        */

static unsigned char *g_midi_buf = NULL; /* raw MIDI bytes from hmp2mid    */

static double g_playback_msec = 0.0; /* current playback time (ms)     */
static int g_playing = 0;            /* 1 = music is actively playing  */
static int g_paused = 0;             /* 1 = playback is paused         */
static int g_requested_paused = 0;   /* latest requested pause state   */
static int g_loop = 0;               /* 1 = loop when reaching the end */
static int g_song_finished = 0;      /* set by audio thread at end     */
static int g_bg_paused = 0;          /* 1 = paused due to app background */
#ifdef ANDROID
static pthread_mutex_t g_background_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_background_cond = PTHREAD_COND_INITIALIZER;
static int g_render_thread_alive;
static int g_background_waiting;
#endif
static float g_volume = 1.0f;     /* 0.0 – 1.0 volume scale        */
static int g_output_rate = 48000; /* actual SDL output sample rate  */

static void (*g_finished_hook)(void) = NULL; /* callback when song ends  */

/* ── PCM playback state (OGG/MP3/FLAC via pcm_decoders) ─────────────── */
#include "pcm_decoders.h"
static int g_is_pcm;       /* 1 = PCM playback, 0 = MIDI         */
static int16_t *g_pcm_buf; /* decoded interleaved PCM samples     */
static size_t g_pcm_total; /* total frames (per-channel samples)  */
static double g_pcm_pos;   /* fractional playback position        */
static int g_pcm_channels; /* 1 or 2                              */
static int g_pcm_rate;     /* source sample rate (e.g. 44100)     */

/* ── Configurable gain (dB) ──────────────────────────────────────────────── */
static float g_gain_db = -10.0f; /* TSF global gain in dB      */
static int g_max_voices = 48;    /* voice limit (runtime-tunable) */

#ifdef ANDROID
static int tsf_atomic_load_int(const int *value)
{
	return __atomic_load_n(value, __ATOMIC_ACQUIRE);
}

static void tsf_atomic_store_int(int *value, int new_value)
{
	__atomic_store_n(value, new_value, __ATOMIC_RELEASE);
}

static float tsf_atomic_load_float(const float *value)
{
	float result;
	__atomic_load(value, &result, __ATOMIC_ACQUIRE);
	return result;
}

static void tsf_atomic_store_float(float *value, float new_value)
{
	__atomic_store(value, &new_value, __ATOMIC_RELEASE);
}
#else
#define tsf_atomic_load_int(value)               (*(value))
#define tsf_atomic_store_int(value, new_value)   (*(value) = (new_value))
#define tsf_atomic_load_float(value)             (*(value))
#define tsf_atomic_store_float(value, new_value) (*(value) = (new_value))
#endif

/* ── Audio diagnostics ──────────────────────────────────────────────────── */
#ifdef ANDROID
#include <time.h>
static int g_clip_count = 0;         /* samples that clipped       */
static int g_sample_count_total = 0; /* total samples rendered     */
static int g_peak_sample = 0;        /* peak absolute sample value */
static int g_active_voices_max = 0;  /* peak active voice count    */

/* Ring-buffer diagnostics */
static int g_rb_underruns = 0; /* callback found buffer empty */
static int g_rb_cb_count = 0;  /* total callbacks             */
static unsigned int g_render_pass_count = 0;
static unsigned int g_callback_trace_count = 0;

static long tsf_music_gettid(void)
{
	return (long) syscall(__NR_gettid);
}

static int tsf_music_should_trace(unsigned int count)
{
	return count <= 8u || (count % 64u) == 0u;
}
#endif

/* ── AAssetManager (Android) ─────────────────────────────────────────── */

#ifdef ANDROID
extern AAssetManager *g_asset_manager; /* set in jni_main.c              */
#endif

/* ── Soundfont loading ───────────────────────────────────────────────── */

static int tsf_music_load_soundfont(void)
{
	if (g_tsf) return 1; /* already loaded */

	/* Query the actual output sample rate from SDL_mixer */
	{
		int freq = 0;
		Uint16 fmt;
		int ch;
		if (Mix_QuerySpec(&freq, &fmt, &ch) && freq > 0) {
			g_output_rate = freq;
		} else {
			g_output_rate = 48000; /* fallback */
		}
		TSFMUSIC_LOG("SDL mixer output: %d Hz, fmt=0x%04X, ch=%d", g_output_rate, fmt, ch);
	}

#ifdef ANDROID
	if (!g_asset_manager) {
		TSFMUSIC_LOG("No AAssetManager — cannot load soundfont");
		return 0;
	}

	AAsset *asset = AAssetManager_open(g_asset_manager, "gm.sf2",
	                                   AASSET_MODE_BUFFER);
	if (!asset) {
		TSFMUSIC_LOG("gm.sf2 not found in APK assets");
		return 0;
	}

	const void *data = AAsset_getBuffer(asset);
	off_t size = AAsset_getLength(asset);
	crash_breadcrumb_v("tsf_sf2 ptr=%p a4=%lu a8=%lu size=%ld", data,
	                   ((unsigned long) data) & 3ul,
	                   ((unsigned long) data) & 7ul, (long) size);

	g_tsf = tsf_load_memory(data, (int) size);
	AAsset_close(asset);

	if (!g_tsf) {
		TSFMUSIC_LOG("tsf_load_memory failed");
		return 0;
	}

	TSFMUSIC_LOG("Soundfont loaded (%d presets)", tsf_get_presetcount(g_tsf));
#endif

	/* Configure output: stereo interleaved, match SDL mixer rate */
	tsf_set_output(g_tsf, TSF_STEREO_INTERLEAVED, g_output_rate,
	               tsf_atomic_load_float(&g_gain_db));
	tsf_set_max_voices(g_tsf, tsf_atomic_load_int(&g_max_voices));

	TSFMUSIC_LOG("TSF configured: rate=%d, max_voices=%d, gain=%.1fdB",
	             g_output_rate, tsf_atomic_load_int(&g_max_voices),
	             tsf_atomic_load_float(&g_gain_db));

	return 1;
}

/* ── Shared render function ───────────────────────────────────────────
 *
 * Advances the MIDI timeline and renders TSF into a signed-16-bit
 * stereo interleaved buffer.  Used by the render thread (Android) or
 * called directly from the audio callback (desktop).
 *
 * Returns the number of stereo frames actually rendered (may be less
 * than requested if the song ends without looping).
 */
static int render_frames(short *out, int frames)
{
	double rate = (double) g_output_rate;
	int rendered = 0;

	/* Process in 256-frame blocks for adequate MIDI event timing */
	enum { BLOCK = 256 };

	while (frames > 0 && g_midi_cur) {
		int block = (frames > BLOCK) ? BLOCK : frames;
#ifdef ANDROID
		unsigned int pass = __atomic_add_fetch(&g_render_pass_count, 1u,
		                                       __ATOMIC_RELAXED);
		int trace = tsf_music_should_trace(pass);
		if (trace)
			crash_breadcrumb_v("tsf_render #%u enter tid=%ld cur=%p ms=%ld",
			                   pass, tsf_music_gettid(), (void *) g_midi_cur,
			                   (long) g_playback_msec);
#endif

		double block_ms = block * (1000.0 / rate);
		g_playback_msec += block_ms;

		/* Dispatch MIDI events up to current time */
#ifdef ANDROID
		if (trace)
			crash_breadcrumb_v("tsf_render #%u events", pass);
#endif
		while (g_midi_cur && g_midi_cur->time <= (unsigned int) g_playback_msec) {
			tml_message *m = g_midi_cur;
			switch (m->type) {
				case TML_NOTE_ON:
					tsf_channel_note_on(g_tsf, m->channel, m->key,
					                    m->velocity / 127.0f);
					break;
				case TML_NOTE_OFF:
					tsf_channel_note_off(g_tsf, m->channel, m->key);
					break;
				case TML_PROGRAM_CHANGE:
					tsf_channel_set_presetnumber(g_tsf, m->channel,
					                             m->program,
					                             (m->channel == 9));
					break;
				case TML_CONTROL_CHANGE:
					tsf_channel_midi_control(g_tsf, m->channel,
					                         m->control, m->control_value);
					break;
				case TML_PITCH_BEND:
					tsf_channel_set_pitchwheel(g_tsf, m->channel,
					                           m->pitch_bend);
					break;
				default:
					break;
			}
			g_midi_cur = m->next;
		}

#ifdef ANDROID
		if (trace)
			crash_breadcrumb_v("tsf_render #%u synth", pass);
#endif
		tsf_render_short(g_tsf, out, block, 0);
#ifdef ANDROID
		if (trace)
			crash_breadcrumb_v("tsf_render #%u synth_done", pass);
#endif

#ifdef ANDROID
		/* Clipping detection (on rendered PCM before volume) */
		{
			int i, n = block * 2;
			for (i = 0; i < n; i++) {
				int abs_val = out[i] < 0 ? -out[i] : out[i];
				if (abs_val > tsf_atomic_load_int(&g_peak_sample))
					tsf_atomic_store_int(&g_peak_sample, abs_val);
				if (out[i] == 32767 || out[i] == -32768)
					__atomic_add_fetch(&g_clip_count, 1, __ATOMIC_RELAXED);
				__atomic_add_fetch(&g_sample_count_total, 1, __ATOMIC_RELAXED);
			}
		}
#endif

		out += block * 2;
		frames -= block;
		rendered += block;
	}

	/* End of MIDI — loop or stop */
	if (!g_midi_cur) {
		if (g_loop) {
			g_midi_cur = g_midi;
			g_playback_msec = 0.0;
			tsf_reset(g_tsf);
			tsf_set_output(g_tsf, TSF_STEREO_INTERLEAVED, g_output_rate,
			               tsf_atomic_load_float(&g_gain_db));
		} else {
			tsf_atomic_store_int(&g_playing, 0);
			tsf_atomic_store_int(&g_song_finished, 1);
			tsf_reset(g_tsf);
			tsf_set_output(g_tsf, TSF_STEREO_INTERLEAVED, g_output_rate,
			               tsf_atomic_load_float(&g_gain_db));
		}
	}

	return rendered;
}

/* ── PCM render: resample decoded audio into stereo output buffer ───── */

static int pcm_render_frames(short *out, int frames)
{
	if (!g_pcm_buf || g_pcm_total == 0) return 0;

	double ratio = (double) g_pcm_rate / (double) g_output_rate;
	int rendered = 0;

	while (frames > 0) {
		size_t idx = (size_t) g_pcm_pos;
		if (idx >= g_pcm_total - 1) {
			if (g_loop) {
				g_pcm_pos = 0.0;
				idx = 0;
			} else {
				tsf_atomic_store_int(&g_playing, 0);
				tsf_atomic_store_int(&g_song_finished, 1);
				break;
			}
		}

		double frac = g_pcm_pos - (double) idx;
		size_t next = idx + 1;
		if (next >= g_pcm_total) next = 0;

		int16_t s0l, s1l, s0r, s1r;
		if (g_pcm_channels >= 2) {
			s0l = g_pcm_buf[idx * 2];
			s1l = g_pcm_buf[next * 2];
			s0r = g_pcm_buf[idx * 2 + 1];
			s1r = g_pcm_buf[next * 2 + 1];
		} else {
			s0l = s0r = g_pcm_buf[idx];
			s1l = s1r = g_pcm_buf[next];
		}

		out[0] = (short) (s0l + (int) ((s1l - s0l) * frac));
		out[1] = (short) (s0r + (int) ((s1r - s0r) * frac));
		out += 2;
		frames--;
		rendered++;
		g_pcm_pos += ratio;
	}

	return rendered;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Android: lock-free SPSC ring buffer + background render thread
 *
 *  The render thread calls render_frames() into a ~2.7-second ring
 *  buffer.  The Mix_HookMusic callback just copies pre-rendered PCM
 *  out of the buffer — essentially zero CPU, no overrun risk.
 * ═══════════════════════════════════════════════════════════════════════ */
#ifdef ANDROID

/* Power-of-2 ring buffer:  2^18 = 262144 samples = 131072 frames
 *                          = 2.73 seconds at 48 kHz stereo               */
#define RB_SHIFT   18
#define RB_SAMPLES (1 << RB_SHIFT)
#define RB_MASK    (RB_SAMPLES - 1)

static short g_rb[RB_SAMPLES];
static volatile int g_rb_wpos; /* monotonic write position       */
static volatile int g_rb_rpos; /* monotonic read position        */
static SDL_Thread *g_render_thread = NULL;
static volatile int g_render_running;

enum tsf_tuning_command_type {
	TSF_TUNING_GAIN,
	TSF_TUNING_VOICES,
	TSF_TUNING_VOLUME,
	TSF_TUNING_PAUSED
};

struct tsf_tuning_command {
	enum tsf_tuning_command_type type;
	union {
		float real_value;
		int int_value;
	} value;
};

#define TSF_TUNING_QUEUE_CAPACITY 64
static struct tsf_tuning_command g_tuning_queue[TSF_TUNING_QUEUE_CAPACITY];
static unsigned int g_tuning_head;
static unsigned int g_tuning_count;
static pthread_mutex_t g_tuning_mutex = PTHREAD_MUTEX_INITIALIZER;
static int g_render_accepting_commands;

static void tsf_reset_render_diagnostics(void)
{
	tsf_atomic_store_int(&g_clip_count, 0);
	tsf_atomic_store_int(&g_sample_count_total, 0);
	tsf_atomic_store_int(&g_peak_sample, 0);
	tsf_atomic_store_int(&g_active_voices_max, 0);
}

static void tsf_apply_tuning_command(const struct tsf_tuning_command *command,
                                     int mutate_synth)
{
	switch (command->type) {
		case TSF_TUNING_GAIN:
			tsf_atomic_store_float(&g_gain_db, command->value.real_value);
			if (mutate_synth && g_tsf)
				tsf_set_output(g_tsf, TSF_STEREO_INTERLEAVED, g_output_rate,
				               command->value.real_value);
			tsf_reset_render_diagnostics();
			break;
		case TSF_TUNING_VOICES:
			tsf_atomic_store_int(&g_max_voices, command->value.int_value);
			if (mutate_synth && g_tsf)
				tsf_set_max_voices(g_tsf, command->value.int_value);
			tsf_atomic_store_int(&g_active_voices_max, 0);
			break;
		case TSF_TUNING_VOLUME:
			tsf_atomic_store_float(&g_volume, command->value.real_value);
			break;
		case TSF_TUNING_PAUSED:
			tsf_atomic_store_int(&g_paused, command->value.int_value);
			break;
	}
}

static void tsf_drain_pending_tuning(int mutate_synth)
{
	struct tsf_tuning_command commands[TSF_TUNING_QUEUE_CAPACITY];
	unsigned int count;
	unsigned int i;

	pthread_mutex_lock(&g_tuning_mutex);
	count = g_tuning_count;
	for (i = 0; i < count; ++i)
		commands[i] = g_tuning_queue[(g_tuning_head + i) % TSF_TUNING_QUEUE_CAPACITY];
	g_tuning_head = (g_tuning_head + count) % TSF_TUNING_QUEUE_CAPACITY;
	g_tuning_count = 0;
	pthread_mutex_unlock(&g_tuning_mutex);

	for (i = 0; i < count; ++i)
		tsf_apply_tuning_command(&commands[i], mutate_synth);
}

static void tsf_apply_pending_tuning(void)
{
	tsf_drain_pending_tuning(1);
}

static void tsf_submit_tuning_command(struct tsf_tuning_command command)
{
	for (;;) {
		unsigned int tail;

		pthread_mutex_lock(&g_tuning_mutex);
		if (!g_render_accepting_commands) {
			if (command.type == TSF_TUNING_PAUSED)
				tsf_atomic_store_int(&g_requested_paused,
				                     command.value.int_value);
			tsf_apply_tuning_command(&command, 0);
			pthread_mutex_unlock(&g_tuning_mutex);
			return;
		}
		if (g_tuning_count < TSF_TUNING_QUEUE_CAPACITY) {
			if (command.type == TSF_TUNING_PAUSED)
				tsf_atomic_store_int(&g_requested_paused,
				                     command.value.int_value);
			tail = (g_tuning_head + g_tuning_count) % TSF_TUNING_QUEUE_CAPACITY;
			g_tuning_queue[tail] = command;
			++g_tuning_count;
			pthread_mutex_unlock(&g_tuning_mutex);
			return;
		}
		pthread_mutex_unlock(&g_tuning_mutex);
		SDL_Delay(1);
	}
}

static void tsf_finish_tuning_ownership(void)
{
	unsigned int i;

	pthread_mutex_lock(&g_tuning_mutex);
	for (i = 0; i < g_tuning_count; ++i) {
		unsigned int index =
		    (g_tuning_head + i) % TSF_TUNING_QUEUE_CAPACITY;
		tsf_apply_tuning_command(&g_tuning_queue[index], 1);
	}
	g_tuning_head =
	    (g_tuning_head + g_tuning_count) % TSF_TUNING_QUEUE_CAPACITY;
	g_tuning_count = 0;
	g_render_accepting_commands = 0;
	pthread_mutex_unlock(&g_tuning_mutex);
}

/* ── Ring buffer helpers ────────────────────────────────────────────── */

static void rb_reset(void)
{
	__atomic_store_n(&g_rb_wpos, 0, __ATOMIC_SEQ_CST);
	__atomic_store_n(&g_rb_rpos, 0, __ATOMIC_SEQ_CST);
}

static void rb_write(const short *data, int count)
{
	unsigned int wpos = (unsigned int) __atomic_load_n(&g_rb_wpos, __ATOMIC_RELAXED);
	unsigned int idx = wpos & RB_MASK;
	int first = RB_SAMPLES - (int) idx;
	if (first > count) first = count;
	memcpy(&g_rb[idx], data, first * sizeof(short));
	if (count > first)
		memcpy(&g_rb[0], data + first, (count - first) * sizeof(short));
	__atomic_store_n(&g_rb_wpos, (int) (wpos + (unsigned int) count), __ATOMIC_RELEASE);
}

static int rb_read(short *out, int count)
{
	unsigned int wpos = (unsigned int) __atomic_load_n(&g_rb_wpos, __ATOMIC_ACQUIRE);
	unsigned int rpos = (unsigned int) __atomic_load_n(&g_rb_rpos, __ATOMIC_RELAXED);
	unsigned int avail = wpos - rpos;
	if ((int) avail < count) count = (int) avail;
	if (count <= 0) return 0;

	unsigned int idx = rpos & RB_MASK;
	int first = RB_SAMPLES - (int) idx;
	if (first > count) first = count;
	memcpy(out, &g_rb[idx], first * sizeof(short));
	if (count > first)
		memcpy(out + first, &g_rb[0], (count - first) * sizeof(short));
	__atomic_store_n(&g_rb_rpos, (int) (rpos + (unsigned int) count), __ATOMIC_RELEASE);
	return count;
}

static unsigned int rb_available(void)
{
	return (unsigned int) __atomic_load_n(&g_rb_wpos, __ATOMIC_ACQUIRE) -
	       (unsigned int) __atomic_load_n(&g_rb_rpos, __ATOMIC_ACQUIRE);
}

/* ── Render thread ──────────────────────────────────────────────────── */

static int render_thread_func(void *data)
{
	(void) data;
	enum { CHUNK = 2048 }; /* frames per render pass */
	short buf[CHUNK * 2];

	TSFMUSIC_LOG("Render thread started");
	crash_breadcrumb_v("tsf_thread start tid=%ld", tsf_music_gettid());

	if (g_tsf) {
		tsf_set_output(g_tsf, TSF_STEREO_INTERLEAVED, g_output_rate,
		               tsf_atomic_load_float(&g_gain_db));
		tsf_set_max_voices(g_tsf, tsf_atomic_load_int(&g_max_voices));
	}

	for (;;) {
		android_lifecycle_diagnostics_count(ANDROID_LIFECYCLE_COUNTER_MUSIC_PRODUCER_WAKE);
		tsf_apply_pending_tuning();
		if (!__atomic_load_n(&g_render_running, __ATOMIC_SEQ_CST))
			break;
		if (tsf_atomic_load_int(&g_bg_paused)) {
			pthread_mutex_lock(&g_background_mutex);
			g_background_waiting = 1;
			pthread_cond_broadcast(&g_background_cond);
			while (tsf_atomic_load_int(&g_bg_paused) &&
			       __atomic_load_n(&g_render_running, __ATOMIC_SEQ_CST))
				pthread_cond_wait(&g_background_cond, &g_background_mutex);
			g_background_waiting = 0;
			pthread_mutex_unlock(&g_background_mutex);
			continue;
		}

		/* Pause: don't produce data */
		if (!tsf_atomic_load_int(&g_playing) ||
		    tsf_atomic_load_int(&g_paused)) {
			SDL_Delay(20);
			continue;
		}

		/* Check available space (samples, not frames) */
		unsigned int filled = rb_available();
		unsigned int space = RB_SAMPLES - filled;
		if (space < CHUNK * 2) {
			SDL_Delay(5); /* buffer is full enough */
			continue;
		}

		int frames = CHUNK;
		int got = g_is_pcm ? pcm_render_frames(buf, frames)
		                   : render_frames(buf, frames);

		/* Track peak active voices (MIDI only) */
		if (!g_is_pcm && g_tsf) {
			int active = tsf_active_voice_count(g_tsf);
			if (active > tsf_atomic_load_int(&g_active_voices_max))
				tsf_atomic_store_int(&g_active_voices_max, active);
		}

		if (got > 0)
			rb_write(buf, got * 2);
	}

	tsf_finish_tuning_ownership();
	pthread_mutex_lock(&g_background_mutex);
	g_background_waiting = 0;
	g_render_thread_alive = 0;
	pthread_cond_broadcast(&g_background_cond);
	pthread_mutex_unlock(&g_background_mutex);
	TSFMUSIC_LOG("Render thread exiting");
	crash_breadcrumb_v("tsf_thread exit tid=%ld", tsf_music_gettid());
	return 0;
}

static void render_thread_start(void)
{
	if (g_render_thread) return; /* already running */
	rb_reset();
	pthread_mutex_lock(&g_tuning_mutex);
	g_render_accepting_commands = 1;
	__atomic_store_n(&g_render_running, 1, __ATOMIC_SEQ_CST);
	pthread_mutex_unlock(&g_tuning_mutex);
	pthread_mutex_lock(&g_background_mutex);
	g_render_thread_alive = 1;
	pthread_mutex_unlock(&g_background_mutex);
	g_render_thread = SDL_CreateThread(render_thread_func, NULL);
	if (!g_render_thread) {
		unsigned int i;
		pthread_mutex_lock(&g_tuning_mutex);
		for (i = 0; i < g_tuning_count; ++i) {
			unsigned int index =
			    (g_tuning_head + i) % TSF_TUNING_QUEUE_CAPACITY;
			tsf_apply_tuning_command(&g_tuning_queue[index], 0);
		}
		g_tuning_head =
		    (g_tuning_head + g_tuning_count) % TSF_TUNING_QUEUE_CAPACITY;
		g_tuning_count = 0;
		g_render_accepting_commands = 0;
		__atomic_store_n(&g_render_running, 0, __ATOMIC_SEQ_CST);
		pthread_mutex_unlock(&g_tuning_mutex);
		pthread_mutex_lock(&g_background_mutex);
		g_render_thread_alive = 0;
		pthread_cond_broadcast(&g_background_cond);
		pthread_mutex_unlock(&g_background_mutex);
	}
}

static void render_thread_stop(void)
{
	if (!g_render_thread) return;
	__atomic_store_n(&g_render_running, 0, __ATOMIC_SEQ_CST);
	pthread_mutex_lock(&g_background_mutex);
	pthread_cond_broadcast(&g_background_cond);
	pthread_mutex_unlock(&g_background_mutex);
	SDL_WaitThread(g_render_thread, NULL);
	g_render_thread = NULL;
}

/* ── Audio callback (Android): just drain ring buffer ───────────────── */

static void tsf_music_callback(void *udata, Uint8 *stream, int len)
{
	(void) udata;
	int needed = len / (int) sizeof(short); /* total samples (stereo) */
	short *out = (short *) stream;
	unsigned int cb = __atomic_add_fetch(&g_callback_trace_count, 1u,
	                                     __ATOMIC_RELAXED);
	int trace = tsf_music_should_trace(cb);
	float volume;

	__atomic_add_fetch(&g_rb_cb_count, 1, __ATOMIC_RELAXED);
	if (trace)
		crash_breadcrumb_v("tsf_cb #%u enter tid=%ld need=%d fill=%u", cb,
		                   tsf_music_gettid(), needed, rb_available());

	if (!tsf_atomic_load_int(&g_playing) ||
	    tsf_atomic_load_int(&g_paused) ||
	    tsf_atomic_load_int(&g_bg_paused)) {
		memset(stream, 0, len);
		return;
	}

	int got = rb_read(out, needed);
	if (trace)
		crash_breadcrumb_v("tsf_cb #%u got=%d", cb, got);

	/* Zero-fill if ring buffer had less data than needed (underrun) */
	if (got < needed) {
		memset(out + got, 0, (needed - got) * (int) sizeof(short));
		if (tsf_atomic_load_int(&g_playing)) {
			int underruns = __atomic_add_fetch(&g_rb_underruns, 1,
			                                   __ATOMIC_RELAXED);
			if (underruns <= 10 || (underruns % 50) == 0)
				TSFMUSIC_LOG("MIDI underrun #%d: got=%d needed=%d rb_fill=%u",
				             underruns, got, needed, rb_available());
		}
	}

	/* Apply volume scaling (cheap — just multiply) */
	volume = tsf_atomic_load_float(&g_volume);
	if (volume < 0.99f) {
		int i;
		for (i = 0; i < got; i++)
			out[i] = (short) (out[i] * volume);
	}
}

#else /* !ANDROID — desktop: render directly in callback */

static void tsf_music_callback(void *udata, Uint8 *stream, int len)
{
	(void) udata;
	float volume;

	if (!tsf_atomic_load_int(&g_playing) ||
	    tsf_atomic_load_int(&g_paused) ||
	    tsf_atomic_load_int(&g_bg_paused)) {
		memset(stream, 0, len);
		return;
	}

	if (!g_is_pcm && (!g_tsf || !g_midi_cur)) {
		memset(stream, 0, len);
		return;
	}

	int frames = len / (2 * (int) sizeof(short));
	short *out = (short *) stream;

	int got = g_is_pcm ? pcm_render_frames(out, frames)
	                   : render_frames(out, frames);

	/* Zero-fill remainder */
	if (got < frames)
		memset(out + got * 2, 0, (frames - got) * 2 * sizeof(short));

	/* Apply volume scaling */
	volume = tsf_atomic_load_float(&g_volume);
	if (volume < 0.99f) {
		int i, n = got * 2;
		for (i = 0; i < n; i++)
			out[i] = (short) (out[i] * volume);
	}
}

#endif /* ANDROID */

/* ── Public API (implements digi_mixer_music.h) ──────────────────────── */
/* Dispatch the finished-song callback on the game thread (safe context). */
static void tsf_dispatch_finished(void)
{
	if (tsf_atomic_load_int(&g_song_finished)) {
		tsf_atomic_store_int(&g_song_finished, 0);
		if (g_finished_hook) {
			void (*hook)(void) = g_finished_hook;
			g_finished_hook = NULL;
			hook();
		}
	}
}
int mix_play_file(char *filename, int loop, void (*hook_finished_track)())
{
	unsigned int bufsize = 0;
	char *fptr;

	crash_breadcrumb_v("mix_play_file enter tid=%ld file=%s loop=%d",
	                   tsf_music_gettid(), filename, loop);
	tsf_dispatch_finished();
	crash_breadcrumb("mix_play_file: dispatch_finished_done");
	mix_free_music();
	crash_breadcrumb("mix_play_file: preflight_free_done");

	fptr = strrchr(filename, '.');
	if (!fptr) return 0;

	/* ── PCM path: OGG / MP3 / FLAC ─────────────────────────────────── */
	if (!d_stricmp(fptr, ".ogg") || !d_stricmp(fptr, ".mp3") ||
	    !d_stricmp(fptr, ".flac")) {
		/* Load file into memory.  Try PhysFS first (game archives),
		 * fall back to fopen for absolute paths (M3U jukebox entries). */
		unsigned char *fbuf = NULL;
		unsigned int fsize = 0;
		PHYSFS_file *fh = PHYSFS_openRead(filename);
		if (fh) {
			fsize = (unsigned int) PHYSFS_fileLength(fh);
			fbuf = (unsigned char *) malloc(fsize);
			if (!fbuf) {
				PHYSFS_close(fh);
				return 0;
			}
			PHYSFS_read(fh, fbuf, 1, fsize);
			PHYSFS_close(fh);
		} else {
			FILE *fp = fopen(filename, "rb");
			if (!fp) {
				con_printf(CON_CRITICAL, "PCM: cannot open %s\n", filename);
				return 0;
			}
			fseek(fp, 0, SEEK_END);
			fsize = (unsigned int) ftell(fp);
			fseek(fp, 0, SEEK_SET);
			fbuf = (unsigned char *) malloc(fsize);
			if (!fbuf) {
				fclose(fp);
				return 0;
			}
			if (fread(fbuf, 1, fsize, fp) != fsize) {
				free(fbuf);
				fclose(fp);
				return 0;
			}
			fclose(fp);
		}

		/* Decode to raw PCM */
		pcm_decode_result_t pcm;
		int decode_status = pcm_decode_memory(fbuf, fsize, fptr, &pcm);
		if (decode_status != PCM_DECODE_OK) {
			con_printf(CON_CRITICAL,
			           decode_status == PCM_DECODE_UNSUPPORTED_CHANNELS
			               ? "PCM: unsupported channel layout for %s; mono or stereo required\n"
			               : "PCM: decode failed for %s\n",
			           filename);
			free(fbuf);
			return 0;
		}
		free(fbuf);
		if (pcm_decode_result_status(&pcm) != PCM_DECODE_OK ||
		    pcm.pcm_samples != pcm.total_samples) {
			con_printf(CON_CRITICAL, "PCM: incomplete or invalid decoded audio for %s\n",
			           filename);
			pcm_decode_free(&pcm);
			return 0;
		}

		/* Query SDL output rate if not yet known */
		if (g_output_rate <= 0) {
			int freq = 0;
			Uint16 fmt;
			int ch;
			if (Mix_QuerySpec(&freq, &fmt, &ch) && freq > 0)
				g_output_rate = freq;
			else
				g_output_rate = 48000;
		}

		g_pcm_buf = pcm.pcm_data;
		g_pcm_total = pcm.total_samples;
		g_pcm_channels = pcm.channels;
		g_pcm_rate = pcm.sample_rate;
		g_pcm_pos = 0.0;
		g_is_pcm = 1;

		TSFMUSIC_LOG("Playing PCM %s (%zu frames, %dHz %dch, loop=%d)",
		             filename, g_pcm_total, g_pcm_rate, g_pcm_channels, loop);

		g_loop = loop;
		tsf_atomic_store_int(&g_requested_paused, 0);
		tsf_atomic_store_int(&g_paused, 0);
		tsf_atomic_store_int(&g_playing, 1);
		g_finished_hook = hook_finished_track ? hook_finished_track : mix_free_music;

#ifdef ANDROID
		tsf_atomic_store_int(&g_rb_underruns, 0);
		tsf_atomic_store_int(&g_rb_cb_count, 0);
		__atomic_store_n(&g_render_pass_count, 0u, __ATOMIC_RELEASE);
		__atomic_store_n(&g_callback_trace_count, 0u, __ATOMIC_RELEASE);
		render_thread_start();
#endif
		Mix_HookMusic(tsf_music_callback, NULL);
		return 1;
	}

	/* ── MIDI path: HMP / MID ────────────────────────────────────────── */
	g_is_pcm = 0;

	/* Load the soundfont on first call */
	if (!tsf_music_load_soundfont()) {
		con_printf(CON_CRITICAL, "TSF: Cannot load soundfont -- no music\n");
		return 0;
	}

	/* Convert HMP -> MIDI in memory */
	if (!d_stricmp(fptr, ".hmp")) {
		hmp2mid(filename, &g_midi_buf, &bufsize);
		if (!g_midi_buf || !bufsize) {
			con_printf(CON_CRITICAL, "TSF: hmp2mid failed for %s\n", filename);
			return 0;
		}
	} else {
		/* For .mid files, read via PhysFS */
		PHYSFS_file *fh = PHYSFS_openRead(filename);
		if (!fh) {
			con_printf(CON_CRITICAL, "TSF: cannot open %s\n", filename);
			return 0;
		}
		bufsize = (unsigned int) PHYSFS_fileLength(fh);
		g_midi_buf = (unsigned char *) d_malloc(bufsize);
		if (!g_midi_buf) {
			PHYSFS_close(fh);
			return 0;
		}
		PHYSFS_read(fh, g_midi_buf, 1, bufsize);
		PHYSFS_close(fh);
	}
	crash_breadcrumb_v("tsf_music midi=%s bytes=%u ptr=%p a4=%lu a8=%lu",
	                   fptr, bufsize, (void *) g_midi_buf,
	                   ((unsigned long) g_midi_buf) & 3ul,
	                   ((unsigned long) g_midi_buf) & 7ul);

	/* Parse MIDI with TinyMidiLoader */
	g_midi = tml_load_memory(g_midi_buf, (int) bufsize);
	if (!g_midi) {
		con_printf(CON_CRITICAL, "TSF: tml_load_memory failed for %s\n",
		           filename);
		d_free(g_midi_buf);
		return 0;
	}

	TSFMUSIC_LOG("Playing %s (%u bytes MIDI, loop=%d)", filename, bufsize, loop);
	crash_breadcrumb_v("tsf_music parsed cur=%p first=%u", (void *) g_midi,
	                   g_midi ? g_midi->time : 0u);

	/* Reset synth state for new song */
	tsf_reset(g_tsf);
	tsf_set_output(g_tsf, TSF_STEREO_INTERLEAVED, g_output_rate,
	               tsf_atomic_load_float(&g_gain_db));

#ifdef ANDROID
	/* Reset diagnostics for the new song */
	tsf_reset_render_diagnostics();
	tsf_atomic_store_int(&g_rb_underruns, 0);
	tsf_atomic_store_int(&g_rb_cb_count, 0);
	__atomic_store_n(&g_render_pass_count, 0u, __ATOMIC_RELEASE);
	__atomic_store_n(&g_callback_trace_count, 0u, __ATOMIC_RELEASE);
#endif

	/* Start playback */
	g_midi_cur = g_midi;
	g_playback_msec = 0.0;
	g_loop = loop;
	tsf_atomic_store_int(&g_requested_paused, 0);
	tsf_atomic_store_int(&g_paused, 0);
	tsf_atomic_store_int(&g_playing, 1);
	g_finished_hook = hook_finished_track ? hook_finished_track : mix_free_music;

#ifdef ANDROID
	/* Start background render thread -- fills ring buffer ahead */
	render_thread_start();
#endif

	Mix_HookMusic(tsf_music_callback, NULL);

	return 1;
}

void mix_free_music(void)
{
#ifdef ANDROID
	crash_breadcrumb_v("mix_free enter tid=%ld playing=%d render=%p midi=%p cur=%p buf=%p",
	                   tsf_music_gettid(), tsf_atomic_load_int(&g_playing),
	                   (void *) g_render_thread,
	                   (void *) g_midi, (void *) g_midi_cur,
	                   (void *) g_midi_buf);

#ifdef ANDROID
	render_thread_stop();
	crash_breadcrumb("mix_free: render_thread_stop_done");
#endif
#endif
	Mix_HookMusic(NULL, NULL);
	crash_breadcrumb("mix_free: hook_cleared");
	tsf_atomic_store_int(&g_playing, 0);
	tsf_atomic_store_int(&g_requested_paused, 0);
	tsf_atomic_store_int(&g_paused, 0);
	tsf_atomic_store_int(&g_song_finished, 0);

	/* Clean up PCM state */
	if (g_pcm_buf) {
		free(g_pcm_buf);
		g_pcm_buf = NULL;
	}
	g_pcm_total = 0;
	g_pcm_pos = 0.0;
	g_is_pcm = 0;

	/* Clean up MIDI state */
	g_midi_cur = NULL;

	if (g_midi) {
		crash_breadcrumb_v("mix_free: tml_free %p", (void *) g_midi);
		tml_free(g_midi);
		crash_breadcrumb("mix_free: tml_free_done");
		g_midi = NULL;
	}

	if (g_midi_buf) {
		crash_breadcrumb_v("mix_free: midi_buf_free %p", (void *) g_midi_buf);
		d_free(g_midi_buf);
		crash_breadcrumb("mix_free: midi_buf_free_done");
	}

	/* Stop all voices but keep the synth loaded */
	if (g_tsf) {
		tsf_reset(g_tsf);
		tsf_set_output(g_tsf, TSF_STEREO_INTERLEAVED, g_output_rate,
		               tsf_atomic_load_float(&g_gain_db));
	}
}

void mix_set_music_volume(int vol)
{
	/* vol is 0..8 from the game.  Map to 0.0 – 1.0. */
#ifdef ANDROID
	struct tsf_tuning_command command = { TSF_TUNING_VOLUME };
	command.value.real_value = (vol > 0) ? (vol / 8.0f) : 0.0f;
	tsf_submit_tuning_command(command);
#else
	tsf_atomic_store_float(&g_volume, (vol > 0) ? (vol / 8.0f) : 0.0f);
#endif
}

void mix_stop_music(void)
{
	mix_free_music();
}

void mix_pause_music(void)
{
#ifdef ANDROID
	struct tsf_tuning_command command = { TSF_TUNING_PAUSED };
	command.value.int_value = 1;
	tsf_submit_tuning_command(command);
#else
	tsf_atomic_store_int(&g_paused, 1);
#endif
}

void mix_resume_music(void)
{
#ifdef ANDROID
	struct tsf_tuning_command command = { TSF_TUNING_PAUSED };
	command.value.int_value = 0;
	tsf_submit_tuning_command(command);
#else
	tsf_atomic_store_int(&g_paused, 0);
#endif
}

void mix_pause_resume_music(void)
{
#ifdef ANDROID
	if (tsf_atomic_load_int(&g_requested_paused))
#else
	if (tsf_atomic_load_int(&g_paused))
#endif
		mix_resume_music();
	else
		mix_pause_music();
}

/* ── Background pause (called from JNI on lifecycle transitions) ────── */

void mix_background_pause(void)
{
	tsf_atomic_store_int(&g_bg_paused, 1);
	pthread_mutex_lock(&g_background_mutex);
	while (g_render_thread_alive &&
	       __atomic_load_n(&g_render_running, __ATOMIC_SEQ_CST) &&
	       !g_background_waiting)
		pthread_cond_wait(&g_background_cond, &g_background_mutex);
	pthread_mutex_unlock(&g_background_mutex);
}

void mix_background_resume(void)
{
	tsf_atomic_store_int(&g_bg_paused, 0);
	pthread_mutex_lock(&g_background_mutex);
	pthread_cond_broadcast(&g_background_cond);
	pthread_mutex_unlock(&g_background_mutex);
}

/* ── Diagnostic accessors (called from game_introspect.cpp) ──────────── */

int tsf_music_get_output_rate(void)
{
	return g_output_rate;
}
int tsf_music_get_playing(void)
{
	return tsf_atomic_load_int(&g_playing);
}
int tsf_music_get_paused(void)
{
	return tsf_atomic_load_int(&g_paused);
}
#ifdef ANDROID
int tsf_music_get_cb_count(void)
{
	return tsf_atomic_load_int(&g_rb_cb_count);
}
long tsf_music_get_cb_max_ns(void)
{
	return 0;
} /* no longer measured */
long tsf_music_get_cb_total_ns(void)
{
	return 0;
}
int tsf_music_get_cb_overrun_count(void)
{
	return tsf_atomic_load_int(&g_rb_underruns);
}
int tsf_music_get_clip_count(void)
{
	return tsf_atomic_load_int(&g_clip_count);
}
int tsf_music_get_sample_count(void)
{
	return tsf_atomic_load_int(&g_sample_count_total);
}
int tsf_music_get_peak_sample(void)
{
	return tsf_atomic_load_int(&g_peak_sample);
}
int tsf_music_get_active_voices_max(void)
{
	return tsf_atomic_load_int(&g_active_voices_max);
}
int tsf_music_get_max_voices(void)
{
	return tsf_atomic_load_int(&g_max_voices);
}
int tsf_music_get_rb_fill(void)
{
	return (int) rb_available();
}
int tsf_music_get_rb_capacity(void)
{
	return RB_SAMPLES;
}
float tsf_music_get_gain_db(void)
{
	return tsf_atomic_load_float(&g_gain_db);
}
void tsf_music_set_gain_db(float db)
{
	struct tsf_tuning_command command = { TSF_TUNING_GAIN };
	if (!isfinite(db))
		return;
	command.value.real_value = db;
	tsf_submit_tuning_command(command);
}
void tsf_music_set_max_voices(int n)
{
	struct tsf_tuning_command command = { TSF_TUNING_VOICES };
	if (n < 8) n = 8;
	if (n > 256) n = 256;
	command.value.int_value = n;
	tsf_submit_tuning_command(command);
}
#endif
