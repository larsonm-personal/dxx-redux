/*
 * GOG disc image (BIN/CUE) based Redbook audio.
 *
 * Replaces rbaudio.c on Android (or any platform that has GOG disc
 * images instead of a physical CD-ROM drive).
 *
 * Reads the CUE sheet (descent_ii.inst) to locate audio tracks in the
 * raw BIN image (descent_ii.gog).  Audio sectors are 2352 bytes of
 * raw 16-bit LE stereo PCM @ 44100 Hz.  Playback streams through a
 * ring buffer + background render thread into Mix_HookMusic(), with
 * linear-interpolation resampling to the SDL output rate (typically
 * 48000 Hz).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <SDL.h>
#include <SDL_mixer.h>
#include <physfs.h>

#include "pstypes.h"
#include "dxxerror.h"
#include "args.h"
#include "rbaudio.h"
#include "console.h"
#include "timer.h"
#include "ignorecase.h"
#include "track_names.h"

#ifdef ANDROID
#include <android/log.h>
#define RBA_LOG(...) __android_log_print(ANDROID_LOG_INFO, "RBAudio", __VA_ARGS__)
#else
#define RBA_LOG(...) con_printf(CON_VERBOSE, __VA_ARGS__)
#endif

/* ── Constants ───────────────────────────────────────────────────────── */

#define SECTOR_SIZE         2352
#define CD_SAMPLE_RATE      44100
#define FRAMES_PER_SECTOR   (SECTOR_SIZE / 4)   /* 588 stereo frames */
#define MAX_TRACKS          100

/* Known Descent II disc ID — returned by RBAGetDiscID() so that
 * songs_haved2_cd() recognises the GOG image as an original D2 CD. */
#define GOG_D2_DISCID       0x7d0ff809u

/* ── Track table ─────────────────────────────────────────────────────── */

typedef struct {
	int type;           /* 0 = data, 1 = audio */
	int start_sector;
	int num_sectors;
} cue_track_t;

static cue_track_t s_tracks[MAX_TRACKS];
static int         s_num_tracks  = 0;
static PHYSFS_File *s_gog_file   = NULL;
static int         s_initialised = 0;
static int         s_output_rate = 48000;

/* ── Playback state ──────────────────────────────────────────────────── */

static volatile int s_playing = 0;
static volatile int s_paused  = 0;
static int  s_current_track   = 0;   /* 1-based */
static int  s_play_first      = 0;
static int  s_play_last       = 0;
static int  s_read_sector     = 0;   /* next sector to read */
static int  s_track_end       = 0;   /* sector past end of current track */
static float s_volume         = 1.0f;
static int  s_rb_underruns    = 0;   /* callback found buffer empty */
static int  s_rb_cb_count     = 0;   /* total callbacks */

static void (*s_finished_hook)(void) = NULL;
static volatile int s_song_finished  = 0;

/* ── PCM input buffer (raw CD audio before resampling) ───────────────── */

#define PCM_BUF_FRAMES  (FRAMES_PER_SECTOR * 16)   /* 9408 frames ≈ 213 ms */

static short s_pcm_buf[PCM_BUF_FRAMES * 2];   /* stereo interleaved */
static int   s_pcm_len = 0;   /* valid frames in buffer  */
static int   s_pcm_pos = 0;   /* current read position   */

/* Resampling accumulator (fractional input position, 0.0–1.0) */
static double s_resample_frac = 0.0;

/* ── Ring buffer (identical pattern to TSF music) ────────────────────── */

#define RB_SHIFT    18
#define RB_SAMPLES  (1 << RB_SHIFT)   /* 262144 samples ≈ 2.7 s @ 48 kHz */
#define RB_MASK     (RB_SAMPLES - 1)

static short        s_rb[RB_SAMPLES];
static volatile int s_rb_wpos = 0;
static volatile int s_rb_rpos = 0;

static SDL_Thread  *s_render_thread  = NULL;
static volatile int s_render_running = 0;

static void rb_reset(void)
{
	__atomic_store_n(&s_rb_wpos, 0, __ATOMIC_SEQ_CST);
	__atomic_store_n(&s_rb_rpos, 0, __ATOMIC_SEQ_CST);
}

static void rb_write(const short *data, int count)
{
	unsigned int wpos = (unsigned int)__atomic_load_n(&s_rb_wpos, __ATOMIC_RELAXED);
	unsigned int idx  = wpos & RB_MASK;
	int first = RB_SAMPLES - (int)idx;
	if (first > count) first = count;
	memcpy(&s_rb[idx], data, first * sizeof(short));
	if (count > first)
		memcpy(&s_rb[0], data + first, (count - first) * sizeof(short));
	__atomic_store_n(&s_rb_wpos, (int)(wpos + (unsigned int)count), __ATOMIC_RELEASE);
}

static int rb_read(short *out, int count)
{
	unsigned int wpos  = (unsigned int)__atomic_load_n(&s_rb_wpos, __ATOMIC_ACQUIRE);
	unsigned int rpos  = (unsigned int)__atomic_load_n(&s_rb_rpos, __ATOMIC_RELAXED);
	unsigned int avail = wpos - rpos;
	if ((int)avail < count) count = (int)avail;
	if (count <= 0) return 0;
	unsigned int idx = rpos & RB_MASK;
	int first = RB_SAMPLES - (int)idx;
	if (first > count) first = count;
	memcpy(out, &s_rb[idx], first * sizeof(short));
	if (count > first)
		memcpy(out + first, &s_rb[0], (count - first) * sizeof(short));
	__atomic_store_n(&s_rb_rpos, (int)(rpos + (unsigned int)count), __ATOMIC_RELEASE);
	return count;
}

static unsigned int rb_available(void)
{
	return (unsigned int)__atomic_load_n(&s_rb_wpos, __ATOMIC_ACQUIRE) -
	       (unsigned int)__atomic_load_n(&s_rb_rpos, __ATOMIC_ACQUIRE);
}

/* ── File helpers ────────────────────────────────────────────────────── */

/* Open a PhysFS file with case-insensitive lookup */
static PHYSFS_File *open_ci(const char *name)
{
	char buf[256];
	strncpy(buf, name, sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = '\0';
	PHYSFSEXT_locateCorrectCase(buf);
	return PHYSFS_openRead(buf);
}

/* ── CUE parser ──────────────────────────────────────────────────────── */

static int parse_msf(const char *msf)
{
	int m = 0, s = 0, f = 0;
	if (sscanf(msf, "%d:%d:%d", &m, &s, &f) < 3) return 0;
	return m * 60 * 75 + s * 75 + f;
}

static int parse_cue_file(void)
{
	PHYSFS_File *f;
	PHYSFS_sint64 file_size;
	int cur_track = -1;
	int i, total_sectors;

	f = open_ci("descent_ii.inst");
	if (!f) {
		RBA_LOG("Could not open CUE file (descent_ii.inst)");
		return 0;
	}

	s_num_tracks = 0;
	memset(s_tracks, 0, sizeof(s_tracks));

	while (!PHYSFS_eof(f)) {
		char line[512];
		int  li = 0;
		/* Read one line */
		while (li < (int)sizeof(line) - 1 && !PHYSFS_eof(f)) {
			char c;
			if (PHYSFS_read(f, &c, 1, 1) != 1) break;
			if (c == '\n') break;
			if (c != '\r') line[li++] = c;
		}
		line[li] = '\0';

		/* TRACK nn TYPE */
		{
			int tnum;
			char ttype[32];
			if (sscanf(line, " TRACK %d %31s", &tnum, ttype) == 2) {
				if (tnum >= 1 && tnum <= MAX_TRACKS) {
					cur_track = tnum;
					if (s_num_tracks < tnum)
						s_num_tracks = tnum;
					s_tracks[tnum - 1].type = (strstr(ttype, "AUDIO") != NULL) ? 1 : 0;
				}
				continue;
			}
		}
		/* TITLE "..." */
		{
			char *p = line;
			while (*p == ' ' || *p == '\t') p++;
			if (strncasecmp(p, "TITLE", 5) == 0 && cur_track >= 1) {
				char *q = strchr(p, '"');
				if (q) {
					q++;
					char *end = strchr(q, '"');
					if (end) {
						int len = (int)(end - q);
						char title[64];
						if (len > 63) len = 63;
						memcpy(title, q, len);
						title[len] = '\0';
						track_names_set_cue_title(cur_track, title);
					}
				}
				continue;
			}
		}
		/* INDEX 01 MM:SS:FF */
		{
			int idx;
			char msf[32];
			if (cur_track >= 1 &&
			    sscanf(line, " INDEX %d %31s", &idx, msf) == 2 && idx == 1)
			{
				s_tracks[cur_track - 1].start_sector = parse_msf(msf);
			}
		}
	}
	PHYSFS_close(f);

	track_names_set_cue_count(s_num_tracks);

	/* Open the BIN file */
	s_gog_file = open_ci("descent_ii.gog");
	if (!s_gog_file) {
		RBA_LOG("Could not open BIN file (descent_ii.gog)");
		s_num_tracks = 0;
		return 0;
	}

	/* Compute track lengths from successive start positions */
	file_size = PHYSFS_fileLength(s_gog_file);
	total_sectors = (int)(file_size / SECTOR_SIZE);

	for (i = 0; i < s_num_tracks; i++) {
		if (i + 1 < s_num_tracks)
			s_tracks[i].num_sectors = s_tracks[i + 1].start_sector - s_tracks[i].start_sector;
		else
			s_tracks[i].num_sectors = total_sectors - s_tracks[i].start_sector;
	}

	for (i = 0; i < s_num_tracks; i++) {
		RBA_LOG("Track %d: %s  start=%d  len=%d",
		        i + 1, s_tracks[i].type ? "audio" : "data",
		        s_tracks[i].start_sector, s_tracks[i].num_sectors);
	}

	return s_num_tracks;
}

/* ── Sector I/O ──────────────────────────────────────────────────────── */

/* Refill s_pcm_buf with raw CD audio frames.  Returns number of new
 * stereo frames appended, or 0 when the current track range is done. */
static int refill_pcm(void)
{
	int frames_read = 0;

	/* Shift unconsumed data to front */
	if (s_pcm_pos > 0 && s_pcm_pos < s_pcm_len) {
		int remaining = s_pcm_len - s_pcm_pos;
		memmove(s_pcm_buf, s_pcm_buf + s_pcm_pos * 2, remaining * 2 * sizeof(short));
		s_pcm_len = remaining;
	} else if (s_pcm_pos >= s_pcm_len) {
		s_pcm_len = 0;
	}
	s_pcm_pos = 0;

	while (s_pcm_len < PCM_BUF_FRAMES) {
		unsigned char raw[SECTOR_SIZE];
		PHYSFS_sint64 offset;
		int i;

		/* Advance track if current one is exhausted */
		while (s_read_sector >= s_track_end) {
			if (s_current_track >= s_play_last) {
				return frames_read;   /* all tracks done */
			}
			s_current_track++;
			s_read_sector = s_tracks[s_current_track - 1].start_sector;
			s_track_end   = s_read_sector + s_tracks[s_current_track - 1].num_sectors;
		}

		offset = (PHYSFS_sint64)s_read_sector * SECTOR_SIZE;
		if (!PHYSFS_seek(s_gog_file, offset)) break;
		if (PHYSFS_read(s_gog_file, raw, SECTOR_SIZE, 1) != 1) break;

		/* Decode 16-bit LE stereo PCM */
		for (i = 0; i < FRAMES_PER_SECTOR; i++) {
			int b = i * 4;
			s_pcm_buf[(s_pcm_len + i) * 2]     = (short)((unsigned short)raw[b]     | ((unsigned short)raw[b + 1] << 8));
			s_pcm_buf[(s_pcm_len + i) * 2 + 1] = (short)((unsigned short)raw[b + 2] | ((unsigned short)raw[b + 3] << 8));
		}
		s_pcm_len   += FRAMES_PER_SECTOR;
		frames_read += FRAMES_PER_SECTOR;
		s_read_sector++;
	}

	return frames_read;
}

/* ── Resampling render (44100 → output rate) ─────────────────────────── */

/* Render up to max_frames stereo output frames.  Returns actual count. */
static int render_cd_frames(short *out, int max_frames)
{
	const double ratio = (double)CD_SAMPLE_RATE / (double)s_output_rate;
	int written = 0;

	while (written < max_frames && s_playing) {
		/* Need more input? */
		if (s_pcm_pos >= s_pcm_len - 1) {
			if (refill_pcm() == 0 && s_pcm_pos >= s_pcm_len - 1) {
				/* End of all tracks */
				s_playing = 0;
				s_song_finished = 1;
				break;
			}
		}

		/* Linear interpolation */
		{
			int p0 = s_pcm_pos + (int)s_resample_frac;
			double frac = s_resample_frac - (int)s_resample_frac;

			if (p0 + 1 < s_pcm_len) {
				short l0 = s_pcm_buf[p0 * 2],     r0 = s_pcm_buf[p0 * 2 + 1];
				short l1 = s_pcm_buf[(p0+1) * 2],  r1 = s_pcm_buf[(p0+1) * 2 + 1];
				out[written * 2]     = (short)(l0 + (int)((l1 - l0) * frac));
				out[written * 2 + 1] = (short)(r0 + (int)((r1 - r0) * frac));
			} else if (p0 < s_pcm_len) {
				out[written * 2]     = s_pcm_buf[p0 * 2];
				out[written * 2 + 1] = s_pcm_buf[p0 * 2 + 1];
			} else {
				out[written * 2] = out[written * 2 + 1] = 0;
			}
		}

		written++;
		s_resample_frac += ratio;

		/* Advance integer part of input position */
		{
			int consumed = (int)s_resample_frac;
			s_pcm_pos += consumed;
			s_resample_frac -= consumed;
		}
	}
	return written;
}

/* ── Background render thread ────────────────────────────────────────── */

static int render_thread_func(void *data)
{
	enum { CHUNK = 2048 };
	short buf[CHUNK * 2];
	(void)data;

	RBA_LOG("CD render thread started");

	while (__atomic_load_n(&s_render_running, __ATOMIC_SEQ_CST)) {
		if (!s_playing || s_paused) {
			SDL_Delay(20);
			continue;
		}

		unsigned int space = RB_SAMPLES - rb_available();
		if (space < CHUNK * 2u) {
			SDL_Delay(5);
			continue;
		}

		int got = render_cd_frames(buf, CHUNK);
		if (got > 0)
			rb_write(buf, got * 2);

		if (!s_playing) break;
	}

	RBA_LOG("CD render thread exiting");
	return 0;
}

static void render_thread_start(void)
{
	if (s_render_thread) return;
	rb_reset();
	__atomic_store_n(&s_render_running, 1, __ATOMIC_SEQ_CST);
	s_render_thread = SDL_CreateThread(render_thread_func, NULL);
}

static void render_thread_stop(void)
{
	if (!s_render_thread) return;
	__atomic_store_n(&s_render_running, 0, __ATOMIC_SEQ_CST);
	SDL_WaitThread(s_render_thread, NULL);
	s_render_thread = NULL;
}

/* ── Mix_HookMusic callback ──────────────────────────────────────────── */

static void rba_music_callback(void *udata, Uint8 *stream, int len)
{
	int needed = len / (int)sizeof(short);
	short *out = (short *)stream;
	int got;
	(void)udata;

	s_rb_cb_count++;

	if (!s_playing || s_paused) {
		memset(stream, 0, len);
		return;
	}

	got = rb_read(out, needed);
	if (got < needed) {
		memset(out + got, 0, (needed - got) * (int)sizeof(short));
		if (s_playing) {
			s_rb_underruns++;
			if (s_rb_underruns <= 10 || (s_rb_underruns % 50) == 0)
				RBA_LOG("CD underrun #%d: got=%d needed=%d rb_fill=%u",
					s_rb_underruns, got, needed, rb_available());
		}
	}
	/* Volume scaling */
	if (s_volume < 0.99f) {
		int i;
		for (i = 0; i < got; i++)
			out[i] = (short)(out[i] * s_volume);
	}
}

/* ── Public RBA API ──────────────────────────────────────────────────── */

void RBAInit(void)
{
	if (s_initialised) return;

	/* Query SDL mixer output rate */
	{
		int freq = 0; Uint16 fmt; int ch;
		if (Mix_QuerySpec(&freq, &fmt, &ch) && freq > 0)
			s_output_rate = freq;
		else
			s_output_rate = 48000;
		RBA_LOG("Output rate: %d Hz", s_output_rate);
	}

	if (parse_cue_file() < 2) {
		RBA_LOG("No usable tracks found in CUE/BIN");
		s_num_tracks = 0;
		if (s_gog_file) { PHYSFS_close(s_gog_file); s_gog_file = NULL; }
		return;
	}

	s_initialised = 1;
	RBAList();
}

void RBAExit(void)
{
	RBAStop();
	render_thread_stop();
	if (s_gog_file) {
		PHYSFS_close(s_gog_file);
		s_gog_file = NULL;
	}
	s_initialised = 0;
	s_num_tracks  = 0;
}

int RBAEnabled(void)
{
	return s_initialised;
}

/* Start playing a single track (1-based) */
int RBAPlayTrack(int track)
{
	if (!s_initialised) return -1;
	if (track < 1 || track > s_num_tracks) return -1;
	if (s_tracks[track - 1].type != 1) return -1;   /* not audio */

	RBAStop();

	s_current_track = track;
	s_play_first    = track;
	s_play_last     = track;
	s_read_sector   = s_tracks[track - 1].start_sector;
	s_track_end     = s_read_sector + s_tracks[track - 1].num_sectors;
	s_pcm_len       = 0;
	s_pcm_pos       = 0;
	s_resample_frac = 0.0;
	s_song_finished = 0;
	s_paused        = 0;
	s_rb_underruns  = 0;
	s_rb_cb_count   = 0;
	s_playing       = 1;

	render_thread_start();
	Mix_HookMusic(rba_music_callback, NULL);

	RBA_LOG("Playing track %d (sectors %d–%d)", track, s_read_sector, s_track_end - 1);
	return track;
}

/* Play tracks first..last (inclusive, 1-based), call hook when done */
int RBAPlayTracks(int first, int last, void (*hook_finished)(void))
{
	if (!s_initialised) return 0;
	if (first < 1 || first > s_num_tracks) return 0;
	if (last < first) last = first;
	if (last > s_num_tracks) last = s_num_tracks;

	RBAStop();

	s_finished_hook = hook_finished;
	s_current_track = first;
	s_play_first    = first;
	s_play_last     = last;
	s_read_sector   = s_tracks[first - 1].start_sector;
	s_track_end     = s_read_sector + s_tracks[first - 1].num_sectors;
	s_pcm_len       = 0;
	s_pcm_pos       = 0;
	s_resample_frac = 0.0;
	s_song_finished = 0;
	s_paused        = 0;
	s_rb_underruns  = 0;
	s_rb_cb_count   = 0;
	s_playing       = 1;

	render_thread_start();
	Mix_HookMusic(rba_music_callback, NULL);

	RBA_LOG("Playing tracks %d–%d", first, last);
	return 1;
}

void RBAStop(void)
{
	if (!s_initialised) return;

	s_playing = 0;
	render_thread_stop();
	Mix_HookMusic(NULL, NULL);
	s_finished_hook = NULL;
	s_song_finished = 0;
	rb_reset();

	RBA_LOG("Playback stopped");
}

void RBASetVolume(int volume)
{
	/* volume: 0–8 from the game */
	s_volume = (volume > 0) ? (float)volume / 8.0f : 0.0f;
}

void RBAPause(void)
{
	s_paused = 1;
	RBA_LOG("Paused");
}

int RBAResume(void)
{
	if (!s_initialised) return -1;
	s_paused = 0;
	RBA_LOG("Resumed");
	return 1;
}

int RBAPauseResume(void)
{
	if (!s_initialised) return 0;
	if (s_paused) {
		s_paused = 0;
		RBA_LOG("Toggle → resumed");
	} else if (s_playing) {
		s_paused = 1;
		RBA_LOG("Toggle → paused");
	} else {
		return 0;
	}
	return 1;
}

int RBAGetNumberOfTracks(void)
{
	if (!s_initialised) return -1;
	RBA_LOG("Number of tracks: %d", s_num_tracks);
	return s_num_tracks;
}

void RBACheckFinishedHook(void)
{
	static fix64 last_check = 0;

	if (!s_initialised) return;

	if ((timer_query() - last_check) >= F2_0) {
		if (s_song_finished && s_finished_hook) {
			void (*hook)(void) = s_finished_hook;
			s_song_finished = 0;
			s_finished_hook = NULL;
			RBA_LOG("Song finished — calling hook");
			hook();
		}
		last_check = timer_query();
	}
}

int RBAGetTrackNum(void)
{
	if (!s_initialised || !s_playing) return 0;
	return s_current_track;
}

int RBAPeekPlayStatus(void)
{
	if (!s_initialised) return 0;
	if (s_playing && !s_paused) return 1;
	if (s_paused) return -1;
	return 0;
}

unsigned long RBAGetDiscID(void)
{
	/* Return a known D2 disc ID so songs_haved2_cd() works */
	if (!s_initialised) return 0;
	return GOG_D2_DISCID;
}

void RBAList(void)
{
	int i;
	if (!s_initialised) return;
	for (i = 0; i < s_num_tracks; i++)
		con_printf(CON_VERBOSE, "RBAudio: Track %d, type %s, sectors %d, offset %d",
		           i + 1, s_tracks[i].type ? "audio" : "data",
		           s_tracks[i].num_sectors, s_tracks[i].start_sector);
}

/* Stubs for unused functions */
void RBAEjectDisk(void) { }
void RBASetStereoAudio(RBACHANNELCTL *channels) { (void)channels; }
void RBASetQuadAudio(RBACHANNELCTL *channels)   { (void)channels; }
void RBAGetAudioInfo(RBACHANNELCTL *channels)    { (void)channels; }
void RBASetChannelVolume(int channel, int volume) { (void)channel; (void)volume; }
void RBADisable(void)  { s_initialised = 0; }
void RBAEnable(void)   { /* re-init would be needed */ }
