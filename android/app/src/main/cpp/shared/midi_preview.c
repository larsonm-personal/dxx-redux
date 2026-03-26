/*
 * midi_preview.c -- Standalone MIDI/HMP preview player for the launcher.
 *
 * Mirrors cd_preview.c architecture: OpenSL ES output, render thread,
 * lock-free ring buffer.  Renders MIDI via TinySoundFont (TSF) +
 * TinyMidiLoader (TML).
 *
 * HMP -> MIDI conversion is a standalone re-implementation of the
 * algorithm in d2/misc/hmp.c, reading from memory instead of PHYSFS.
 * Keep in sync with hmp.c if the format handling changes.
 */

#include "midi_preview.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <android/log.h>
#include <android/asset_manager.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

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
static double s_playback_msec = 0.0;
static int s_duration_ms = 0;
static int s_output_rate = 48000;
static float s_volume = 0.7f;
static float s_gain_db = -10.0f;
static int s_max_voices = 48;

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

/* ── HMP -> MIDI conversion (standalone, from memory) ────────────────
 *
 * Re-implements d2/misc/hmp.c:hmp2mid() without PHYSFS.
 * HMP format:
 *   offset 0x00: "HMIMIDIP" (8 bytes)
 *   offset 0x30: num_tracks (4 bytes LE)
 *   offset 0x38: tempo (4 bytes LE)
 *   offset 0x308: track data begins
 *   Each track: 4 bytes skip, 4 bytes track_size (includes 12-byte header),
 *               4 bytes skip, then (track_size - 12) bytes of track data.
 *
 * Keep in sync with d2/misc/hmp.c if format handling changes.
 */

#define HMP_TRACKS 32

static unsigned int read_le32(const unsigned char *p)
{
	return (unsigned int) p[0] |
	       ((unsigned int) p[1] << 8) |
	       ((unsigned int) p[2] << 16) |
	       ((unsigned int) p[3] << 24);
}

static void write_be16(unsigned char *p, unsigned short v)
{
	p[0] = (unsigned char) (v >> 8);
	p[1] = (unsigned char) v;
}

static void write_be32(unsigned char *p, unsigned int v)
{
	p[0] = (unsigned char) (v >> 24);
	p[1] = (unsigned char) (v >> 16);
	p[2] = (unsigned char) (v >> 8);
	p[3] = (unsigned char) v;
}

/* Dynamic buffer for building MIDI output */
typedef struct {
	unsigned char *data;
	unsigned int len;
	unsigned int cap;
} midbuf_t;

static void mb_init(midbuf_t *mb)
{
	mb->data = NULL;
	mb->len = 0;
	mb->cap = 0;
}

static void mb_ensure(midbuf_t *mb, unsigned int need)
{
	if (mb->len + need > mb->cap) {
		unsigned int newcap = mb->cap ? mb->cap * 2 : 4096;
		while (newcap < mb->len + need) newcap *= 2;
		mb->data = (unsigned char *) realloc(mb->data, newcap);
		mb->cap = newcap;
	}
}

static void mb_append(midbuf_t *mb, const void *src, unsigned int n)
{
	mb_ensure(mb, n);
	memcpy(mb->data + mb->len, src, n);
	mb->len += n;
}

static void mb_append_byte(midbuf_t *mb, unsigned char b)
{
	mb_ensure(mb, 1);
	mb->data[mb->len++] = b;
}

/*
 * Convert one HMP track to MIDI track data (MTrk body, no header).
 * Returns length of MIDI track body written.
 */
static unsigned int hmptrk2mid(const unsigned char *data, int size, midbuf_t *mb)
{
	const unsigned char *dptr = data;
	const unsigned char *end = data + size;
	unsigned char last_com = 0;
	unsigned int offset = mb->len;

	while (data < end) {
		/* Read variable-length delta time (HMI format) */
		if (data[0] & 0x80) {
			unsigned char b = data[0] & 0x7F;
			mb_append_byte(mb, b);
		} else {
			unsigned int d = data[0] & 0x7F;
			int n1 = 0;
			while (data + n1 < end && (data[n1] & 0x80) == 0) {
				n1++;
				if (data + n1 >= end) break;
				d += (unsigned int) (data[n1] & 0x7F) << (n1 * 7);
			}
			n1 = 1;
			while (data + n1 < end && (data[n1] & 0x80) == 0) {
				n1++;
				if (n1 == 4) return 0;
			}
			/* Write as standard MIDI variable-length (big-endian) */
			int n2;
			for (n2 = 0; n2 <= n1; n2++) {
				unsigned char b = data[n1 - n2] & 0x7F;
				if (n2 != n1) b |= 0x80;
				mb_append_byte(mb, b);
			}
			data += n1;
		}
		data++;
		if (data >= end) break;

		if (*data == 0xFF) {
			/* Meta event */
			if (data + 2 >= end) break;
			unsigned int meta_len = data[2];
			if (data + 3 + meta_len > end) break;
			mb_append(mb, data, 3 + meta_len);
			if (data[1] == 0x2F) break; /* end of track */
			data += 3 + meta_len;
		} else {
			unsigned char lc1 = data[0];
			if ((lc1 & 0x80) == 0) return 0;
			switch (lc1 & 0xF0) {
				case 0x80:
				case 0x90:
				case 0xA0:
				case 0xB0:
				case 0xE0:
					if (lc1 != last_com) mb_append_byte(mb, lc1);
					if (data + 2 >= end) break;
					mb_append(mb, data + 1, 2);
					data += 3;
					break;
				case 0xC0:
				case 0xD0:
					if (lc1 != last_com) mb_append_byte(mb, lc1);
					if (data + 1 >= end) break;
					mb_append(mb, data + 1, 1);
					data += 2;
					break;
				default:
					return 0;
			}
			last_com = lc1;
		}
	}
	return mb->len - offset;

	(void) dptr;
}

/*
 * Convert HMP data (in memory) to standard MIDI.
 * On success, *out_midi and *out_len are set (caller must free *out_midi).
 * Returns 1 on success, 0 on failure.
 */
int hmp2mid_mem(const unsigned char *hmp, int hmp_len,
                unsigned char **out_midi, int *out_len)
{
	if (hmp_len < 0x308 + 12) return 0;
	if (memcmp(hmp, "HMIMIDIP", 8) != 0) return 0;

	int num_tracks = (int) read_le32(hmp + 0x30);
	if (num_tracks < 1 || num_tracks > HMP_TRACKS) return 0;

	unsigned int tempo = read_le32(hmp + 0x38);
	unsigned short time_div = (unsigned short) (tempo * 1.6);

	midbuf_t mb;
	mb_init(&mb);

	/* MIDI header: MThd, length=6, format=1, ntrks, time_div */
	unsigned char hdr[14];
	memcpy(hdr, "MThd", 4);
	write_be32(hdr + 4, 6);
	write_be16(hdr + 8, 1); /* format */
	write_be16(hdr + 10, (unsigned short) num_tracks);
	write_be16(hdr + 12, time_div);
	mb_append(&mb, hdr, 14);

	/* Tempo track (track 0 in HMP is skipped, starts from track 1) */
	static const unsigned char tempo_trk[] = {
		'M', 'T', 'r', 'k', 0, 0, 0, 11,
		0, 0xFF, 0x51, 0x03, 0x18, 0x80, 0x00,
		0, 0xFF, 0x2F, 0x00
	};
	mb_append(&mb, tempo_trk, sizeof(tempo_trk));

	/* Parse track data starting at offset 0x308 */
	int offset = 0x308;
	int i;
	for (i = 0; i < num_tracks; i++) {
		if (offset + 12 > hmp_len) break;
		/* Skip 4 bytes, read track size (4 bytes LE), skip 4 bytes */
		unsigned int trk_size = read_le32(hmp + offset + 4);
		if (trk_size < 12) break;
		unsigned int data_size = trk_size - 12;
		offset += 12;
		if (offset + (int) data_size > hmp_len) break;

		if (i == 0) {
			/* Track 0 is tempo, already emitted above -- skip */
			offset += (int) data_size;
			continue;
		}

		/* Write MTrk header, then convert track data */
		unsigned int mtrk_pos = mb.len;
		unsigned char mtrk_hdr[8];
		memcpy(mtrk_hdr, "MTrk", 4);
		write_be32(mtrk_hdr + 4, 0); /* placeholder */
		mb_append(&mb, mtrk_hdr, 8);

		unsigned int trk_body_len = hmptrk2mid(hmp + offset, (int) data_size, &mb);
		if (trk_body_len == 0) {
			LOGW("Track %d conversion failed", i);
		}
		/* Patch MTrk length */
		write_be32(mb.data + mtrk_pos + 4, trk_body_len);

		offset += (int) data_size;
	}

	*out_midi = mb.data;
	*out_len = (int) mb.len;
	return 1;
}

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
	FILE *fp = fopen(hog_path, "rb");
	if (!fp) return 0;

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
		fseek(fp, (long) entry_len, SEEK_CUR);
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

	int count = 0;
	int ext_len = ext ? (int) strlen(ext) : 0;
	while (!feof(fp) && count < max_entries) {
		char name[14];
		unsigned char len_bytes[4];
		memset(name, 0, sizeof(name));
		if (fread(name, 1, 13, fp) != 13) break;
		if (fread(len_bytes, 1, 4, fp) != 4) break;
		unsigned int entry_len = read_le32(len_bytes);

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
		fseek(fp, (long) entry_len, SEEK_CUR);
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
		if (!s_playing || s_paused) {
			usleep(20000);
			continue;
		}

		unsigned int space = RB_SAMPLES - rb_available();
		if (space < CHUNK * 2u) {
			usleep(5000);
			continue;
		}

		int got = render_midi_frames(buf, CHUNK);
		if (got > 0)
			rb_write(buf, got * 2);

		if (!s_playing) break;
	}

	LOGI("MIDI preview render thread exiting");
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

/* ── OpenSL ES output (same pattern as cd_preview.c) ─────────────────── */

static void osl_callback(SLAndroidSimpleBufferQueueItf bq, void *ctx)
{
	(void) ctx;
	short *buf = s_play_bufs[s_next_buf];
	int needed = BUF_FRAMES * 2;

	memset(buf, 0, needed * sizeof(short));

	if (s_playing && !s_paused) {
		rb_read(buf, needed);
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

int midi_preview_init(AAssetManager *mgr)
{
	if (s_tsf) return 1; /* already initialized */

	if (!mgr) {
		LOGE("No AAssetManager");
		return 0;
	}

	AAsset *asset = AAssetManager_open(mgr, "gm.sf2", AASSET_MODE_BUFFER);
	if (!asset) {
		LOGE("gm.sf2 not found in APK assets");
		return 0;
	}

	const void *data = AAsset_getBuffer(asset);
	off_t size = AAsset_getLength(asset);

	s_tsf = tsf_load_memory(data, (int) size);
	AAsset_close(asset);

	if (!s_tsf) {
		LOGE("tsf_load_memory failed for gm.sf2");
		return 0;
	}

	LOGI("SoundFont loaded (%d presets)", tsf_get_presetcount(s_tsf));
	return 1;
}

int midi_preview_start(const unsigned char *data, int len,
                       int is_hmp, int sample_rate)
{
	unsigned char *midi_data = NULL;
	int midi_len = 0;

	/* Stop any existing preview */
	midi_preview_stop();

	if (!data || len <= 0 || sample_rate <= 0) {
		LOGE("Invalid args");
		return 0;
	}
	if (!s_tsf) {
		LOGE("Not initialized -- call midi_preview_init first");
		return 0;
	}

	/* Convert HMP to MIDI if needed */
	if (is_hmp) {
		if (!hmp2mid_mem(data, len, &midi_data, &midi_len)) {
			LOGE("HMP -> MIDI conversion failed");
			return 0;
		}
	} else {
		/* Standard MIDI: copy the data */
		midi_data = (unsigned char *) malloc(len);
		if (!midi_data) return 0;
		memcpy(midi_data, data, len);
		midi_len = len;
	}

	/* Parse with TML */
	s_midi = tml_load_memory(midi_data, midi_len);
	if (!s_midi) {
		LOGE("tml_load_memory failed");
		free(midi_data);
		return 0;
	}

	/* Compute duration */
	s_duration_ms = compute_midi_duration_ms(midi_data, midi_len);

	s_midi_buf = midi_data;
	s_midi_buf_len = midi_len;
	s_midi_cur = s_midi;
	s_playback_msec = 0.0;
	s_output_rate = sample_rate;
	s_paused = 0;
	s_playing = 1;

	/* Configure TSF */
	tsf_reset(s_tsf);
	tsf_set_output(s_tsf, TSF_STEREO_INTERLEAVED, s_output_rate, s_gain_db);
	tsf_set_max_voices(s_tsf, s_max_voices);

	LOGI("Starting MIDI playback (%d bytes, duration=%dms, rate=%d)",
	     midi_len, s_duration_ms, sample_rate);

	/* Init OpenSL ES */
	if (!osl_init(sample_rate)) {
		LOGE("OpenSL ES init failed");
		tml_free(s_midi);
		s_midi = NULL;
		s_midi_cur = NULL;
		free(s_midi_buf);
		s_midi_buf = NULL;
		s_playing = 0;
		return 0;
	}

	render_thread_start();
	return 1;
}

void midi_preview_stop(void)
{
	s_playing = 0;
	s_paused = 0;
	render_thread_stop();
	osl_shutdown();

	if (s_midi) {
		tml_free(s_midi);
		s_midi = NULL;
		s_midi_cur = NULL;
	}
	if (s_midi_buf) {
		free(s_midi_buf);
		s_midi_buf = NULL;
		s_midi_buf_len = 0;
	}
	rb_reset();
	s_duration_ms = 0;
	s_playback_msec = 0.0;
}

void midi_preview_pause(void)
{
	s_paused = 1;
}

void midi_preview_resume(void)
{
	s_paused = 0;
}

int midi_preview_seek(float fraction)
{
	if (!s_playing && !s_paused) return 0;
	if (!s_midi || !s_midi_buf) return 0;
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
	rb_reset();
	return 1;
}

int midi_preview_get_state(int *out_position_ms, int *out_duration_ms)
{
	if (out_position_ms) {
		int pos = (int) s_playback_msec;
		if (pos > s_duration_ms) pos = s_duration_ms;
		*out_position_ms = pos;
	}
	if (out_duration_ms) *out_duration_ms = s_duration_ms;

	if (s_playing && !s_paused) return MDP_PLAYING;
	if (s_paused) return MDP_PAUSED;
	return MDP_STOPPED;
}
