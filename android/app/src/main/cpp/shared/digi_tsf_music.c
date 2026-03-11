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

/* ── Configurable gain (dB) ──────────────────────────────────────────────── */
static float    g_gain_db           = -10.0f; /* TSF global gain in dB      */
static int      g_max_voices        = 48;     /* voice limit (runtime-tunable) */

/* ── Audio diagnostics ──────────────────────────────────────────────────── */
#ifdef ANDROID
#include <time.h>
static int      g_clip_count        = 0;     /* samples that clipped       */
static int      g_sample_count_total= 0;     /* total samples rendered     */
static int      g_peak_sample       = 0;     /* peak absolute sample value */
static int      g_active_voices_max = 0;     /* peak active voice count    */

/* Ring-buffer diagnostics */
static int      g_rb_underruns      = 0;     /* callback found buffer empty */
static int      g_rb_cb_count       = 0;     /* total callbacks             */
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

	/* Configure output: stereo interleaved, match SDL mixer rate */
	tsf_set_output(g_tsf, TSF_STEREO_INTERLEAVED, g_output_rate, g_gain_db);
	tsf_set_max_voices(g_tsf, g_max_voices);

	TSFMUSIC_LOG("TSF configured: rate=%d, max_voices=%d, gain=%.1fdB", g_output_rate, g_max_voices, g_gain_db);

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
	double rate = (double)g_output_rate;
	int rendered = 0;

	/* Process in 256-frame blocks for adequate MIDI event timing */
	enum { BLOCK = 256 };

	while (frames > 0 && g_midi_cur) {
		int block = (frames > BLOCK) ? BLOCK : frames;

		double block_ms = block * (1000.0 / rate);
		g_playback_msec += block_ms;

		/* Dispatch MIDI events up to current time */
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

		tsf_render_short(g_tsf, out, block, 0);

#ifdef ANDROID
		/* Clipping detection (on rendered PCM before volume) */
		{
			int i, n = block * 2;
			for (i = 0; i < n; i++) {
				int abs_val = out[i] < 0 ? -out[i] : out[i];
				if (abs_val > g_peak_sample) g_peak_sample = abs_val;
				if (out[i] == 32767 || out[i] == -32768) g_clip_count++;
				g_sample_count_total++;
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
			tsf_set_output(g_tsf, TSF_STEREO_INTERLEAVED, g_output_rate, g_gain_db);
		} else {
			g_playing = 0;
			g_song_finished = 1;
			tsf_reset(g_tsf);
			tsf_set_output(g_tsf, TSF_STEREO_INTERLEAVED, g_output_rate, g_gain_db);
		}
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
#define RB_SHIFT     18
#define RB_SAMPLES   (1 << RB_SHIFT)
#define RB_MASK      (RB_SAMPLES - 1)

static short          g_rb[RB_SAMPLES];
static volatile int   g_rb_wpos;        /* monotonic write position       */
static volatile int   g_rb_rpos;        /* monotonic read position        */
static SDL_Thread    *g_render_thread = NULL;
static volatile int   g_render_running;

/* ── Ring buffer helpers ────────────────────────────────────────────── */

static void rb_reset(void)
{
	__atomic_store_n(&g_rb_wpos, 0, __ATOMIC_SEQ_CST);
	__atomic_store_n(&g_rb_rpos, 0, __ATOMIC_SEQ_CST);
}

static void rb_write(const short *data, int count)
{
	unsigned int wpos = (unsigned int)__atomic_load_n(&g_rb_wpos, __ATOMIC_RELAXED);
	unsigned int idx = wpos & RB_MASK;
	int first = RB_SAMPLES - (int)idx;
	if (first > count) first = count;
	memcpy(&g_rb[idx], data, first * sizeof(short));
	if (count > first)
		memcpy(&g_rb[0], data + first, (count - first) * sizeof(short));
	__atomic_store_n(&g_rb_wpos, (int)(wpos + (unsigned int)count), __ATOMIC_RELEASE);
}

static int rb_read(short *out, int count)
{
	unsigned int wpos = (unsigned int)__atomic_load_n(&g_rb_wpos, __ATOMIC_ACQUIRE);
	unsigned int rpos = (unsigned int)__atomic_load_n(&g_rb_rpos, __ATOMIC_RELAXED);
	unsigned int avail = wpos - rpos;
	if ((int)avail < count) count = (int)avail;
	if (count <= 0) return 0;

	unsigned int idx = rpos & RB_MASK;
	int first = RB_SAMPLES - (int)idx;
	if (first > count) first = count;
	memcpy(out, &g_rb[idx], first * sizeof(short));
	if (count > first)
		memcpy(out + first, &g_rb[0], (count - first) * sizeof(short));
	__atomic_store_n(&g_rb_rpos, (int)(rpos + (unsigned int)count), __ATOMIC_RELEASE);
	return count;
}

static unsigned int rb_available(void)
{
	return (unsigned int)__atomic_load_n(&g_rb_wpos, __ATOMIC_ACQUIRE) -
	       (unsigned int)__atomic_load_n(&g_rb_rpos, __ATOMIC_ACQUIRE);
}

/* ── Render thread ──────────────────────────────────────────────────── */

static int render_thread_func(void *data)
{
	(void)data;
	enum { CHUNK = 2048 };     /* frames per render pass */
	short buf[CHUNK * 2];

	TSFMUSIC_LOG("Render thread started");

	while (__atomic_load_n(&g_render_running, __ATOMIC_SEQ_CST)) {
		/* Pause: don't produce data */
		if (!g_playing || g_paused || g_bg_paused) {
			SDL_Delay(20);
			continue;
		}

		/* Check available space (samples, not frames) */
		unsigned int filled = rb_available();
		unsigned int space = RB_SAMPLES - filled;
		if (space < CHUNK * 2) {
			SDL_Delay(5);        /* buffer is full enough */
			continue;
		}

		int frames = CHUNK;
		int got = render_frames(buf, frames);

		/* Track peak active voices */
		if (g_tsf) {
			int active = tsf_active_voice_count(g_tsf);
			if (active > g_active_voices_max)
				g_active_voices_max = active;
		}

		if (got > 0)
			rb_write(buf, got * 2);

		/* Song ended without loop — stop producing */
		if (!g_playing)
			break;
	}

	TSFMUSIC_LOG("Render thread exiting");
	return 0;
}

static void render_thread_start(void)
{
	if (g_render_thread) return;  /* already running */
	rb_reset();
	__atomic_store_n(&g_render_running, 1, __ATOMIC_SEQ_CST);
	g_render_thread = SDL_CreateThread(render_thread_func, NULL);
}

static void render_thread_stop(void)
{
	if (!g_render_thread) return;
	__atomic_store_n(&g_render_running, 0, __ATOMIC_SEQ_CST);
	SDL_WaitThread(g_render_thread, NULL);
	g_render_thread = NULL;
}

/* ── Audio callback (Android): just drain ring buffer ───────────────── */

static void tsf_music_callback(void *udata, Uint8 *stream, int len)
{
	(void)udata;
	int needed = len / (int)sizeof(short);   /* total samples (stereo) */
	short *out = (short *)stream;

	g_rb_cb_count++;

	if (!g_playing || g_paused || g_bg_paused) {
		memset(stream, 0, len);
		return;
	}

	int got = rb_read(out, needed);

	/* Zero-fill if ring buffer had less data than needed (underrun) */
	if (got < needed) {
		memset(out + got, 0, (needed - got) * (int)sizeof(short));
		if (g_playing) {
			g_rb_underruns++;
			if (g_rb_underruns <= 10 || (g_rb_underruns % 50) == 0)
				TSFMUSIC_LOG("MIDI underrun #%d: got=%d needed=%d rb_fill=%u",
					g_rb_underruns, got, needed, rb_available());
		}
	}

	/* Apply volume scaling (cheap — just multiply) */
	if (g_volume < 0.99f) {
		int i;
		for (i = 0; i < got; i++)
			out[i] = (short)(out[i] * g_volume);
	}
}

#else  /* !ANDROID — desktop: render directly in callback */

static void tsf_music_callback(void *udata, Uint8 *stream, int len)
{
	(void)udata;

	if (!g_tsf || !g_midi_cur || !g_playing || g_paused || g_bg_paused) {
		memset(stream, 0, len);
		return;
	}

	int frames = len / (2 * (int)sizeof(short));
	short *out = (short *)stream;

	int got = render_frames(out, frames);

	/* Zero-fill remainder */
	if (got < frames)
		memset(out + got * 2, 0, (frames - got) * 2 * sizeof(short));

	/* Apply volume scaling */
	if (g_volume < 0.99f) {
		int i, n = got * 2;
		for (i = 0; i < n; i++)
			out[i] = (short)(out[i] * g_volume);
	}
}

#endif /* ANDROID */

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
	tsf_set_output(g_tsf, TSF_STEREO_INTERLEAVED, g_output_rate, g_gain_db);

#ifdef ANDROID
	/* Reset diagnostics for the new song */
	g_clip_count = 0;
	g_sample_count_total = 0;
	g_peak_sample = 0;
	g_active_voices_max = 0;
	g_rb_underruns = 0;
	g_rb_cb_count = 0;
#endif

	/* Start playback */
	g_midi_cur = g_midi;
	g_playback_msec = 0.0;
	g_loop = loop;
	g_paused = 0;
	g_playing = 1;
	g_finished_hook = hook_finished_track ? hook_finished_track : mix_free_music;

#ifdef ANDROID
	/* Start background render thread — fills ring buffer ahead */
	render_thread_start();
#endif

	Mix_HookMusic(tsf_music_callback, NULL);

	return 1;
}

void mix_free_music(void)
{
#ifdef ANDROID
	render_thread_stop();
#endif
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
		tsf_set_output(g_tsf, TSF_STEREO_INTERLEAVED, g_output_rate, g_gain_db);
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
int tsf_music_get_cb_count(void) { return g_rb_cb_count; }
long tsf_music_get_cb_max_ns(void) { return 0; }  /* no longer measured */
long tsf_music_get_cb_total_ns(void) { return 0; }
int tsf_music_get_cb_overrun_count(void) { return g_rb_underruns; }
int tsf_music_get_clip_count(void) { return g_clip_count; }
int tsf_music_get_sample_count(void) { return g_sample_count_total; }
int tsf_music_get_peak_sample(void) { return g_peak_sample; }
int tsf_music_get_active_voices_max(void) { return g_active_voices_max; }
int tsf_music_get_max_voices(void) { return g_max_voices; }
int tsf_music_get_rb_fill(void) { return (int)rb_available(); }
int tsf_music_get_rb_capacity(void) { return RB_SAMPLES; }
float tsf_music_get_gain_db(void) { return g_gain_db; }
void tsf_music_set_gain_db(float db) {
	g_gain_db = db;
	if (g_tsf) {
		tsf_set_output(g_tsf, TSF_STEREO_INTERLEAVED, g_output_rate, g_gain_db);
	}
	g_clip_count = 0;
	g_sample_count_total = 0;
	g_peak_sample = 0;
	g_active_voices_max = 0;
}
void tsf_music_set_max_voices(int n) {
	if (n < 8) n = 8;
	if (n > 256) n = 256;
	g_max_voices = n;
	if (g_tsf) {
		tsf_set_max_voices(g_tsf, g_max_voices);
	}
	g_active_voices_max = 0;
}
#endif
