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
#include "hog_midi_catalog.h"
#include "midi_seek_timeline.h"

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
static unsigned char *s_midi_buf = NULL; /* raw MIDI bytes (owned)         */
static int s_midi_buf_len = 0;
static struct midi_seek_timeline s_timeline;

static volatile int s_playing = 0;
static volatile int s_paused = 0;
static volatile int s_output_enabled = 0;
static volatile int s_seek_target_ms = -1;
static volatile int s_position_snapshot_ms = 0;
static volatile int s_duration_snapshot_ms = 0;
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

/* ── HOG file reader ─────────────────────────────────────────────────
 *
 * HOG format: 3-byte magic "DHF", then repeated entries of
 *   13-byte filename (null-padded), 4-byte LE length, data bytes.
 *
 * Keep in sync with d2/utilities/hogextract.c format assumptions.
 */

int hog_read_entry(const char *hog_path, const char *entry_name,
                   unsigned char **out_data, int *out_len)
{
	struct hog_midi_catalog catalog;
	size_t index;
	int result = 0;

	if (!hog_path || !entry_name || !out_data || !out_len)
		return 0;
	if (hog_midi_catalog_load(hog_path, &catalog) != HOG_MIDI_CATALOG_OK)
		return 0;
	for (index = 0; index < catalog.count; index++) {
		if (!hog_catalog_strcasecmp(catalog.entries[index].name, entry_name)) {
			result = hog_midi_catalog_read(hog_path, &catalog, index, out_data, out_len);
			break;
		}
	}
	hog_midi_catalog_free(&catalog);
	return result;
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

static unsigned int midi_event_time_ms(const void *event)
{
	return ((const tml_message *) event)->time;
}

static const void *midi_event_next(const void *event)
{
	return ((const tml_message *) event)->next;
}

static void dispatch_midi_event(void *context, const void *event)
{
	tsf *synth = (tsf *) context;
	const tml_message *m = (const tml_message *) event;

	switch (m->type) {
		case TML_NOTE_ON:
			tsf_channel_note_on(synth, m->channel, m->key, m->velocity / 127.0f);
			break;
		case TML_NOTE_OFF:
			tsf_channel_note_off(synth, m->channel, m->key);
			break;
		case TML_PROGRAM_CHANGE:
			tsf_channel_set_presetnumber(synth, m->channel, m->program, (m->channel == 9));
			break;
		case TML_CONTROL_CHANGE:
			tsf_channel_midi_control(synth, m->channel, m->control, m->control_value);
			break;
		case TML_PITCH_BEND:
			tsf_channel_set_pitchwheel(synth, m->channel, m->pitch_bend);
			break;
		default:
			break;
	}
}

static void render_tsf_frames(void *context, short *output, int frames)
{
	tsf_render_short((tsf *) context, output, frames, 0);
}

static void reset_midi_timeline(void)
{
	static const struct midi_seek_timeline_ops ops = {
		midi_event_time_ms,
		midi_event_next,
		dispatch_midi_event,
		render_tsf_frames
	};
	midi_seek_timeline_init(&s_timeline, s_midi, s_output_rate, 2, s_tsf, &ops);
}

static int render_midi_frames(short *out, int frames)
{
	int i;
	int rendered = midi_seek_timeline_render(&s_timeline, out, frames);

	s_playback_msec = midi_seek_timeline_position_ms(&s_timeline);
	if (s_volume < 0.99f)
		for (i = 0; i < rendered * 2; i++)
			out[i] = (short) (out[i] * s_volume);

	/* Song ended */
	if (!s_timeline.event && rendered < frames) {
		__atomic_store_n(&s_playing, 0, __ATOMIC_RELEASE);
		__atomic_store_n(&s_output_enabled, 0, __ATOMIC_RELEASE);
	}
	return rendered;
}

struct approximate_seek_channel {
	unsigned char key_down[128];
	unsigned char sounding[128];
	unsigned char velocity[128];
	unsigned char sustain;
};

static void publish_playback_state(void)
{
	int position_ms = (int) s_playback_msec;

	if (position_ms > s_duration_ms) position_ms = s_duration_ms;
	__atomic_store_n(&s_position_snapshot_ms, position_ms, __ATOMIC_RELEASE);
	__atomic_store_n(&s_duration_snapshot_ms, s_duration_ms, __ATOMIC_RELEASE);
}

static void approximate_note_off(struct approximate_seek_channel *channel, int key)
{
	channel->key_down[key] = 0;
	if (!channel->sustain)
		channel->sounding[key] = 0;
}

static void approximate_control_change(struct approximate_seek_channel *channel,
                                       int control, int value)
{
	int key;

	if (control == 64) {
		channel->sustain = value >= 64;
		if (!channel->sustain)
			for (key = 0; key < 128; key++)
				if (!channel->key_down[key])
					channel->sounding[key] = 0;
	} else if (control == 120) {
		memset(channel->key_down, 0, sizeof(channel->key_down));
		memset(channel->sounding, 0, sizeof(channel->sounding));
	} else if (control == 121) {
		channel->sustain = 0;
		for (key = 0; key < 128; key++)
			if (!channel->key_down[key])
				channel->sounding[key] = 0;
	} else if (control == 123) {
		memset(channel->key_down, 0, sizeof(channel->key_down));
		if (!channel->sustain)
			memset(channel->sounding, 0, sizeof(channel->sounding));
	}
}

/*
 * Rebuild logical MIDI state without rendering the intervening PCM. Notes
 * sounding across the target restart their envelopes, which is acceptable for
 * a launcher preview and keeps seek work proportional to MIDI events instead
 * of elapsed audio frames.
 */
static void approximate_seek(int target_ms)
{
	struct approximate_seek_channel channels[16] = { 0 };
	tml_message *message = s_midi;
	int channel;
	int events = 0;
	int key;
	int notes = 0;

	tsf_reset(s_tsf);
	tsf_set_output(s_tsf, TSF_STEREO_INTERLEAVED, s_output_rate, s_gain_db);
	tsf_set_max_voices(s_tsf, s_max_voices);
	while (message && message->time <= (unsigned int) target_ms) {
		channel = (unsigned char) message->channel;
		if (channel < 16) {
			struct approximate_seek_channel *seek_channel = &channels[channel];
			key = (unsigned char) message->key;
			switch (message->type) {
				case TML_NOTE_ON:
					if ((unsigned char) message->velocity) {
						seek_channel->key_down[key] = 1;
						seek_channel->sounding[key] = 1;
						seek_channel->velocity[key] = (unsigned char) message->velocity;
					} else {
						approximate_note_off(seek_channel, key);
					}
					break;
				case TML_NOTE_OFF:
					approximate_note_off(seek_channel, key);
					break;
				case TML_PROGRAM_CHANGE:
					tsf_channel_set_presetnumber(s_tsf, channel,
					                             (unsigned char) message->program,
					                             channel == 9);
					break;
				case TML_CONTROL_CHANGE: {
					int control = (unsigned char) message->control;
					int value = (unsigned char) message->control_value;
					approximate_control_change(seek_channel, control, value);
					tsf_channel_midi_control(s_tsf, channel, control, value);
					break;
				}
				case TML_PITCH_BEND:
					tsf_channel_set_pitchwheel(s_tsf, channel, message->pitch_bend);
					break;
				default:
					break;
			}
		}
		message = message->next;
		events++;
	}
	for (channel = 0; channel < 16; channel++)
		for (key = 0; key < 128; key++)
			if (channels[channel].sounding[key]) {
				tsf_channel_note_on(s_tsf, channel, key,
				                    channels[channel].velocity[key] / 127.0f);
				notes++;
			}

	reset_midi_timeline();
	s_timeline.event = message;
	s_timeline.frame = midi_seek_timeline_frame_for_ms(target_ms, s_output_rate);
	s_playback_msec = target_ms;
	pthread_mutex_lock(&s_ring_reset_mutex);
	rb_reset();
	pthread_mutex_unlock(&s_ring_reset_mutex);
	LOGI("Approximate seek complete: target=%dms events=%d notes=%d",
	     target_ms, events, notes);
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
		{
			int target_ms = __atomic_exchange_n(&s_seek_target_ms, -1, __ATOMIC_ACQ_REL);
			if (target_ms >= 0 && s_midi && s_tsf) {
				approximate_seek(target_ms);
				publish_playback_state();
			}
		}
		if (!__atomic_load_n(&s_playing, __ATOMIC_ACQUIRE) ||
		    __atomic_load_n(&s_paused, __ATOMIC_ACQUIRE)) {
			sleep_usec = 20000;
		} else {
			unsigned int space = RB_SAMPLES - rb_available();
			if (space < CHUNK * 2u) {
				sleep_usec = 5000;
			} else {
				int got = render_midi_frames(buf, CHUNK);
				if (got > 0)
					rb_write(buf, got * 2);
				stop = !__atomic_load_n(&s_playing, __ATOMIC_ACQUIRE);
			}
		}
		publish_playback_state();
		pthread_mutex_unlock(&s_playback_mutex);

		if (stop) break;
		if (sleep_usec) usleep((useconds_t) sleep_usec);
	}

	__atomic_store_n(&s_render_running, 0, __ATOMIC_SEQ_CST);
	LOGI("MIDI preview render thread exiting");
	return NULL;
}

static int render_thread_start(void)
{
	if (s_thread_created) return 1;
	pthread_mutex_lock(&s_ring_reset_mutex);
	rb_reset();
	pthread_mutex_unlock(&s_ring_reset_mutex);
	__atomic_store_n(&s_render_running, 1, __ATOMIC_SEQ_CST);
	if (pthread_create(&s_render_tid, NULL, render_thread_func, NULL) == 0) {
		s_thread_created = 1;
		return 1;
	}
	__atomic_store_n(&s_render_running, 0, __ATOMIC_SEQ_CST);
	LOGE("Failed to create render thread");
	return 0;
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

	SLresult r = (*bq)->Enqueue(bq, buf, needed * sizeof(short));
	if (r != SL_RESULT_SUCCESS)
		LOGE("Re-enqueue audio buffer: %d", (int) r);
	s_next_buf = (s_next_buf + 1) % NUM_BUFFERS;
}

static int osl_init(int sample_rate)
{
	SLresult r;
	int i;
	SLObjectItf engine_obj = NULL;
	SLEngineItf engine = NULL;
	SLObjectItf outmix_obj = NULL;
	SLObjectItf player_obj = NULL;
	SLPlayItf player_play = NULL;
	SLAndroidSimpleBufferQueueItf player_bq = NULL;

	r = slCreateEngine(&engine_obj, 0, NULL, 0, NULL, NULL);
	if (r != SL_RESULT_SUCCESS || !engine_obj) {
		LOGE("slCreateEngine: %d", (int) r);
		goto fail;
	}
	r = (*engine_obj)->Realize(engine_obj, SL_BOOLEAN_FALSE);
	if (r != SL_RESULT_SUCCESS) {
		LOGE("Realize engine: %d", (int) r);
		goto fail;
	}
	r = (*engine_obj)->GetInterface(engine_obj, SL_IID_ENGINE, &engine);
	if (r != SL_RESULT_SUCCESS || !engine) {
		LOGE("Get engine interface: %d", (int) r);
		goto fail;
	}

	r = (*engine)->CreateOutputMix(engine, &outmix_obj, 0, NULL, NULL);
	if (r != SL_RESULT_SUCCESS || !outmix_obj) {
		LOGE("CreateOutputMix: %d", (int) r);
		goto fail;
	}
	r = (*outmix_obj)->Realize(outmix_obj, SL_BOOLEAN_FALSE);
	if (r != SL_RESULT_SUCCESS) {
		LOGE("Realize output mix: %d", (int) r);
		goto fail;
	}

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
	SLDataLocator_OutputMix loc_out = { SL_DATALOCATOR_OUTPUTMIX, outmix_obj };
	SLDataSink snk = { &loc_out, NULL };

	const SLInterfaceID ids[1] = { SL_IID_BUFFERQUEUE };
	const SLboolean req[1] = { SL_BOOLEAN_TRUE };

	r = (*engine)->CreateAudioPlayer(engine, &player_obj,
	                                 &src, &snk, 1, ids, req);
	if (r != SL_RESULT_SUCCESS || !player_obj) {
		LOGE("CreateAudioPlayer: %d", (int) r);
		goto fail;
	}
	r = (*player_obj)->Realize(player_obj, SL_BOOLEAN_FALSE);
	if (r != SL_RESULT_SUCCESS) {
		LOGE("Realize audio player: %d", (int) r);
		goto fail;
	}
	r = (*player_obj)->GetInterface(player_obj, SL_IID_PLAY, &player_play);
	if (r != SL_RESULT_SUCCESS || !player_play) {
		LOGE("Get play interface: %d", (int) r);
		goto fail;
	}
	r = (*player_obj)->GetInterface(player_obj, SL_IID_BUFFERQUEUE, &player_bq);
	if (r != SL_RESULT_SUCCESS || !player_bq) {
		LOGE("Get buffer queue interface: %d", (int) r);
		goto fail;
	}
	r = (*player_bq)->RegisterCallback(player_bq, osl_callback, NULL);
	if (r != SL_RESULT_SUCCESS) {
		LOGE("Register buffer queue callback: %d", (int) r);
		goto fail;
	}

	for (i = 0; i < NUM_BUFFERS; i++) {
		memset(s_play_bufs[i], 0, sizeof(s_play_bufs[i]));
		r = (*player_bq)->Enqueue(player_bq, s_play_bufs[i], BUF_FRAMES * 2 * sizeof(short));
		if (r != SL_RESULT_SUCCESS) {
			LOGE("Enqueue initial buffer %d: %d", i, (int) r);
			goto fail;
		}
	}
	s_next_buf = 0;
	r = (*player_play)->SetPlayState(player_play, SL_PLAYSTATE_PLAYING);
	if (r != SL_RESULT_SUCCESS) {
		LOGE("Start audio player: %d", (int) r);
		goto fail;
	}

	s_engine_obj = engine_obj;
	s_engine = engine;
	s_outmix_obj = outmix_obj;
	s_player_obj = player_obj;
	s_player_play = player_play;
	s_player_bq = player_bq;

	LOGI("OpenSL ES ready: %d Hz stereo", sample_rate);
	return 1;

fail:
	if (player_obj)
		(*player_obj)->Destroy(player_obj);
	if (outmix_obj)
		(*outmix_obj)->Destroy(outmix_obj);
	if (engine_obj)
		(*engine_obj)->Destroy(engine_obj);
	return 0;
}

static void osl_shutdown(void)
{
	if (s_player_obj) {
		SLresult r = (*s_player_play)->SetPlayState(s_player_play, SL_PLAYSTATE_STOPPED);
		if (r != SL_RESULT_SUCCESS)
			LOGE("Stop audio player: %d", (int) r);
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
	s_playback_msec = 0.0;
	s_output_rate = sample_rate;
	__atomic_store_n(&s_paused, 0, __ATOMIC_RELEASE);
	__atomic_store_n(&s_playing, 1, __ATOMIC_RELEASE);
	__atomic_store_n(&s_output_enabled, 1, __ATOMIC_RELEASE);

	/* Configure TSF */
	tsf_reset(s_tsf);
	tsf_set_output(s_tsf, TSF_STEREO_INTERLEAVED, s_output_rate, s_gain_db);
	tsf_set_max_voices(s_tsf, s_max_voices);
	reset_midi_timeline();
	__atomic_store_n(&s_seek_target_ms, -1, __ATOMIC_RELEASE);
	publish_playback_state();
	pthread_mutex_unlock(&s_playback_mutex);

	LOGI("Starting MIDI playback (%d bytes, duration=%dms, rate=%d)",
	     midi_len, s_duration_ms, sample_rate);

	/* Init OpenSL ES */
	if (!osl_init(sample_rate)) {
		LOGE("OpenSL ES init failed");
		midi_preview_stop_internal();
		pthread_mutex_unlock(&s_control_mutex);
		return 0;
	}

	if (!render_thread_start()) {
		midi_preview_stop_internal();
		pthread_mutex_unlock(&s_control_mutex);
		return 0;
	}
	pthread_mutex_unlock(&s_control_mutex);
	return 1;
}

static void midi_preview_stop_internal(void)
{
	pthread_mutex_lock(&s_playback_mutex);
	__atomic_store_n(&s_playing, 0, __ATOMIC_RELEASE);
	__atomic_store_n(&s_paused, 0, __ATOMIC_RELEASE);
	__atomic_store_n(&s_seek_target_ms, -1, __ATOMIC_RELEASE);
	__atomic_store_n(&s_output_enabled, 0, __ATOMIC_RELEASE);
	publish_playback_state();
	pthread_mutex_unlock(&s_playback_mutex);
	render_thread_stop();
	osl_shutdown();

	pthread_mutex_lock(&s_playback_mutex);
	if (s_midi) {
		tml_free(s_midi);
		s_midi = NULL;
		s_timeline.event = NULL;
	}
	if (s_midi_buf) {
		d_free(s_midi_buf);
		s_midi_buf = NULL;
		s_midi_buf_len = 0;
	}
	rb_reset();
	s_duration_ms = 0;
	s_playback_msec = 0.0;
	publish_playback_state();
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
	__atomic_store_n(&s_paused, 1, __ATOMIC_RELEASE);
	__atomic_store_n(&s_output_enabled, 0, __ATOMIC_RELEASE);
	pthread_mutex_unlock(&s_control_mutex);
}

void midi_preview_resume(void)
{
	int playing;
	pthread_mutex_lock(&s_control_mutex);
	playing = __atomic_load_n(&s_playing, __ATOMIC_ACQUIRE);
	__atomic_store_n(&s_paused, 0, __ATOMIC_RELEASE);
	__atomic_store_n(&s_output_enabled, playing, __ATOMIC_RELEASE);
	pthread_mutex_unlock(&s_control_mutex);
}

int midi_preview_seek(float fraction)
{
	int target_ms;
	pthread_mutex_lock(&s_control_mutex);
	if (!__atomic_load_n(&s_render_running, __ATOMIC_ACQUIRE) ||
	    !s_midi || !s_midi_buf) {
		pthread_mutex_unlock(&s_control_mutex);
		return 0;
	}
	if (fraction < 0.0f) fraction = 0.0f;
	if (fraction > 1.0f) fraction = 1.0f;
	target_ms = (int) (fraction * s_duration_ms);
	__atomic_store_n(&s_position_snapshot_ms, target_ms, __ATOMIC_RELEASE);
	__atomic_store_n(&s_seek_target_ms, target_ms, __ATOMIC_RELEASE);
	pthread_mutex_unlock(&s_control_mutex);
	return 1;
}

int midi_preview_get_state(int *out_position_ms, int *out_duration_ms)
{
	int playing = __atomic_load_n(&s_playing, __ATOMIC_ACQUIRE);
	int paused = __atomic_load_n(&s_paused, __ATOMIC_ACQUIRE);

	if (out_position_ms)
		*out_position_ms = __atomic_load_n(&s_position_snapshot_ms, __ATOMIC_ACQUIRE);
	if (out_duration_ms)
		*out_duration_ms = __atomic_load_n(&s_duration_snapshot_ms, __ATOMIC_ACQUIRE);
	if (playing && !paused) return MDP_PLAYING;
	if (paused) return MDP_PAUSED;
	return MDP_STOPPED;
}
