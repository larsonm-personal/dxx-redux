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
#include <physfs.h>

#ifdef ANDROID
#include <android/log.h>
#include <android/asset_manager.h>
#define TSFMUSIC_LOG(...) __android_log_print(ANDROID_LOG_INFO, "TSF-Music", __VA_ARGS__)
#else
#define TSFMUSIC_LOG(...) ((void)0)
#endif

/* TinySoundFont — implementation in this translation unit */
#define TSF_IMPLEMENTATION
#define TSF_NO_STDIO
#include "tsf.h"

/* TinyMidiLoader — implementation in this translation unit */
#define TML_IMPLEMENTATION
#define TML_NO_STDIO
#include "tml.h"

#include "args.h"
#include "hmp.h"
#include "digi_mixer_music.h"
#include "u_mem.h"
#include "console.h"

/* ── Globals ─────────────────────────────────────────────────────────── */

static tsf *g_tsf              = NULL;   /* SoundFont synth instance       */
static tml_message *g_midi     = NULL;   /* parsed MIDI message list       */
static tml_message *g_midi_cur = NULL;   /* current playback cursor        */

static unsigned char *g_midi_buf = NULL; /* raw MIDI bytes from hmp2mid    */

static double g_playback_msec  = 0.0;   /* current playback time (ms)     */
static int    g_playing        = 0;      /* 1 = music is actively playing  */
static int    g_paused         = 0;      /* 1 = playback is paused         */
static int    g_loop           = 0;      /* 1 = loop when reaching the end */
static int    g_song_finished  = 0;      /* set by audio thread at end     */
static int    g_bg_paused      = 0;      /* 1 = paused due to app background */
static float  g_volume         = 1.0f;   /* 0.0 – 1.0 volume scale        */
static int    g_output_rate    = 48000;  /* actual SDL output sample rate  */

static void (*g_finished_hook)(void) = NULL;  /* callback when song ends  */

/* ── Audio callback diagnostics ─────────────────────────────────────────── */
#ifdef ANDROID
#include <time.h>
static int      g_cb_count          = 0;     /* total callbacks            */
static long     g_cb_max_ns         = 0;     /* worst-case render time     */
static long     g_cb_total_ns       = 0;     /* accumulated render time    */
static int      g_cb_overrun_count  = 0;     /* callbacks exceeding budget */

static long timespec_diff_ns(struct timespec *a, struct timespec *b) {
	return (b->tv_sec - a->tv_sec) * 1000000000L + (b->tv_nsec - a->tv_nsec);
}
#endif

/* ── AAssetManager (Android) ─────────────────────────────────────────── */

#ifdef ANDROID
extern AAssetManager *g_asset_manager;   /* set in jni_main.c              */
#endif

/* ── Soundfont loading ───────────────────────────────────────────────── */

static int tsf_music_load_soundfont(void)
{
	if (g_tsf) return 1;  /* already loaded */

	/* Query the actual output sample rate from SDL_mixer */
	{
		int freq = 0; Uint16 fmt; int ch;
		if (Mix_QuerySpec(&freq, &fmt, &ch) && freq > 0) {
			g_output_rate = freq;
		} else {
			g_output_rate = 48000;  /* fallback */
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

	g_tsf = tsf_load_memory(data, (int)size);
	AAsset_close(asset);

	if (!g_tsf) {
		TSFMUSIC_LOG("tsf_load_memory failed");
		return 0;
	}

	TSFMUSIC_LOG("Soundfont loaded (%d presets)", tsf_get_presetcount(g_tsf));
#endif

	/* Configure output: stereo interleaved, match SDL mixer rate,
	 * -10 dB gain to avoid clipping when many voices play simultaneously. */
	tsf_set_output(g_tsf, TSF_STEREO_INTERLEAVED, g_output_rate, -10.0f);
	tsf_set_max_voices(g_tsf, 64);

	TSFMUSIC_LOG("TSF configured: rate=%d, max_voices=64, gain=-10dB", g_output_rate);

	return 1;
}

/* ── Mix_HookMusic callback ──────────────────────────────────────────── */

/*
 * Called by SDL_mixer on the audio thread to fill the music portion of
 * the output buffer.  We advance the MIDI timeline and render via TSF.
 *
 * stream: signed-16-bit stereo interleaved buffer
 * len:    size in bytes (= samples * 2 channels * 2 bytes)
 */
static void tsf_music_callback(void *udata, Uint8 *stream, int len)
{
	(void)udata;

	if (!g_tsf || !g_midi_cur || !g_playing || g_paused || g_bg_paused) {
		memset(stream, 0, len);
		return;
	}

#ifdef ANDROID
	struct timespec t_start, t_end;
	clock_gettime(CLOCK_MONOTONIC, &t_start);
#endif

	int sample_count = len / (2 * sizeof(short));   /* stereo frames   */
	short *out = (short *)stream;
	double rate = (double)g_output_rate;

	/* Process in small blocks for accurate MIDI event timing */
	enum { BLOCK = 64 };

	while (sample_count > 0) {
		int block = (sample_count > BLOCK) ? BLOCK : sample_count;

		/* Advance playback clock */
		double block_ms = block * (1000.0 / rate);
		g_playback_msec += block_ms;

		/* Dispatch all MIDI events up to the current time */
		while (g_midi_cur && g_midi_cur->time <= (unsigned int)g_playback_msec) {
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

		/* Render this block of audio */
		tsf_render_short(g_tsf, out, block, 0);

		/* Apply volume scaling */
		if (g_volume < 0.99f) {
			int i, n = block * 2;  /* stereo samples */
			for (i = 0; i < n; i++) {
				out[i] = (short)(out[i] * g_volume);
			}
		}

		out += block * 2;
		sample_count -= block;
	}

	/* End of MIDI — loop or stop */
	if (!g_midi_cur) {
		if (g_loop) {
			g_midi_cur = g_midi;
			g_playback_msec = 0.0;
			tsf_reset(g_tsf);
			/* Re-configure after reset */
			tsf_set_output(g_tsf, TSF_STEREO_INTERLEAVED, g_output_rate, -10.0f);
		} else {
			g_playing = 0;
			g_song_finished = 1;
			tsf_reset(g_tsf);
			tsf_set_output(g_tsf, TSF_STEREO_INTERLEAVED, g_output_rate, -10.0f);
			/* Don't call g_finished_hook here — we're on the audio thread
			 * with SDL's mixer_lock held.  The hook is dispatched from
			 * the game thread in mix_play_file / mix_free_music. */
		}
	}

#ifdef ANDROID
	clock_gettime(CLOCK_MONOTONIC, &t_end);
	long elapsed_ns = timespec_diff_ns(&t_start, &t_end);
	long budget_ns = (long)((double)len / (2 * sizeof(short) * 2) / rate * 1e9);

	g_cb_count++;
	g_cb_total_ns += elapsed_ns;
	if (elapsed_ns > g_cb_max_ns) g_cb_max_ns = elapsed_ns;
	if (elapsed_ns > budget_ns) g_cb_overrun_count++;

	/* Log stats every ~2 seconds (rate / buffer_frames ≈ callbacks/sec) */
	int frames_per_cb = len / (2 * sizeof(short));
	int log_interval = (g_output_rate / frames_per_cb) * 2;
	if (log_interval < 1) log_interval = 1;
	if (g_cb_count % log_interval == 0) {
		long avg_us = (g_cb_total_ns / g_cb_count) / 1000;
		long max_us = g_cb_max_ns / 1000;
		long budget_us = budget_ns / 1000;
		TSFMUSIC_LOG("cb stats: avg=%ldus max=%ldus budget=%ldus overruns=%d/%d frames=%d rate=%d",
			avg_us, max_us, budget_us, g_cb_overrun_count, g_cb_count,
			frames_per_cb, g_output_rate);
	}
#endif
}

/* ── Public API (implements digi_mixer_music.h) ──────────────────────── */
/* Dispatch the finished-song callback on the game thread (safe context). */
static void tsf_dispatch_finished(void)
{
	if (g_song_finished) {
		g_song_finished = 0;
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

	tsf_dispatch_finished();
	mix_free_music();

	fptr = strrchr(filename, '.');
	if (!fptr) return 0;

	/* Load the soundfont on first call */
	if (!tsf_music_load_soundfont()) {
		con_printf(CON_CRITICAL, "TSF: Cannot load soundfont — no music\n");
		return 0;
	}

	/* Convert HMP → MIDI in memory */
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
		bufsize = (unsigned int)PHYSFS_fileLength(fh);
		g_midi_buf = (unsigned char *)malloc(bufsize);
		if (!g_midi_buf) {
			PHYSFS_close(fh);
			return 0;
		}
		PHYSFS_read(fh, g_midi_buf, 1, bufsize);
		PHYSFS_close(fh);
	}

	/* Parse MIDI with TinyMidiLoader */
	g_midi = tml_load_memory(g_midi_buf, (int)bufsize);
	if (!g_midi) {
		con_printf(CON_CRITICAL, "TSF: tml_load_memory failed for %s\n",
		           filename);
		free(g_midi_buf);
		g_midi_buf = NULL;
		return 0;
	}

	TSFMUSIC_LOG("Playing %s (%u bytes MIDI, loop=%d)", filename, bufsize, loop);

	/* Reset synth state for new song */
	tsf_reset(g_tsf);
	tsf_set_output(g_tsf, TSF_STEREO_INTERLEAVED, g_output_rate, -10.0f);

#ifdef ANDROID
	/* Reset callback diagnostics for the new song */
	g_cb_count = 0;
	g_cb_max_ns = 0;
	g_cb_total_ns = 0;
	g_cb_overrun_count = 0;
#endif

	/* Start playback */
	g_midi_cur = g_midi;
	g_playback_msec = 0.0;
	g_loop = loop;
	g_paused = 0;
	g_playing = 1;
	g_finished_hook = hook_finished_track ? hook_finished_track : mix_free_music;

	Mix_HookMusic(tsf_music_callback, NULL);

	return 1;
}

void mix_free_music(void)
{
	Mix_HookMusic(NULL, NULL);
	g_playing = 0;
	g_paused = 0;
	g_song_finished = 0;
	g_midi_cur = NULL;

	if (g_midi) {
		tml_free(g_midi);
		g_midi = NULL;
	}

	if (g_midi_buf) {
		free(g_midi_buf);
		g_midi_buf = NULL;
	}

	/* Stop all voices but keep the synth loaded */
	if (g_tsf) {
		tsf_reset(g_tsf);
		tsf_set_output(g_tsf, TSF_STEREO_INTERLEAVED, g_output_rate, -10.0f);
	}
}

void mix_set_music_volume(int vol)
{
	/* vol is 0..8 from the game.  Map to 0.0 – 1.0. */
	g_volume = (vol > 0) ? (vol / 8.0f) : 0.0f;
}

void mix_stop_music(void)
{
	mix_free_music();
}

void mix_pause_music(void)
{
	g_paused = 1;
}

void mix_resume_music(void)
{
	g_paused = 0;
}

void mix_pause_resume_music(void)
{
	g_paused = !g_paused;
}

/* ── Background pause (called from JNI on lifecycle transitions) ────── */

void mix_background_pause(void)
{
	g_bg_paused = 1;
}

void mix_background_resume(void)
{
	g_bg_paused = 0;
}

/* ── Diagnostic accessors (called from game_introspect.cpp) ──────────── */

int tsf_music_get_output_rate(void) { return g_output_rate; }
int tsf_music_get_playing(void) { return g_playing; }
int tsf_music_get_paused(void) { return g_paused; }
#ifdef ANDROID
int tsf_music_get_cb_count(void) { return g_cb_count; }
long tsf_music_get_cb_max_ns(void) { return g_cb_max_ns; }
long tsf_music_get_cb_total_ns(void) { return g_cb_total_ns; }
int tsf_music_get_cb_overrun_count(void) { return g_cb_overrun_count; }
#endif
