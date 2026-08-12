/*
 * GOG disc image (BIN/CUE) based Redbook audio.
 *
 * Replaces rbaudio.c on Android (or any platform that has GOG disc
 * images instead of a physical CD-ROM drive).
 *
 * Supports multiple audio sources (BIN/CUE pairs) that are combined
 * into a unified track sequence.  Reads audio_playlist.json at init
 * to configure sources, falling back to the legacy descent_ii.inst/gog
 * single-source path.
 *
 * Audio sectors are 2352 bytes of raw 16-bit LE stereo PCM @ 44100 Hz.
 * Playback streams through a ring buffer + background render thread
 * into Mix_HookMusic(), with linear-interpolation resampling to the
 * SDL output rate (typically 48000 Hz).
 */

#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

#ifdef ANDROID
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#endif

#include <SDL.h>
#include <SDL_mixer.h>
#include <physfs.h>

#include "pstypes.h"
#include "dxxerror.h"
#include "args.h"
#include "config.h"
#include "rbaudio.h"
#include "console.h"
#include "timer.h"
#include "ignorecase.h"
#include "track_names.h"
#include "../extract/cd_read_contract.h"
#include "../extract/cue_parser.h"

#ifdef ANDROID
#include <android/log.h>
#include "android_lifecycle_diagnostics.h"
#include "android_log.h"
#define RBA_LOG(...)  __android_log_print(ANDROID_LOG_INFO, "RBAudio", __VA_ARGS__)
#define RBA_DIAG(...) debug_log(DLOG_GAME, "[RBA] " __VA_ARGS__)
#else
#define RBA_LOG(...)  con_printf(CON_VERBOSE, __VA_ARGS__)
#define RBA_DIAG(...) ((void) 0)
#endif

/* ── Constants ───────────────────────────────────────────────────────── */

#define SECTOR_SIZE       2352
#define CD_SAMPLE_RATE    44100
#define FRAMES_PER_SECTOR (SECTOR_SIZE / 4) /* 588 stereo frames */
#define MAX_TRACKS        100
#define MAX_SOURCES       8
#define MAX_SOURCE_BINS   CUE_MAX_FILES

/* Known Descent II disc ID — returned by RBAGetDiscID() so that
 * songs_haved2_cd() recognises the GOG image as an original D2 CD. */
#define GOG_D2_DISCID 0x7d0ff809u

/* ── BIN file handle (PHYSFS or stdio) ───────────────────────────────── */

/* Wraps either a PHYSFS_File or a stdio FILE so that SAF paths
 * (/proc/self/fd/N) work alongside PHYSFS-relative paths.             */
typedef struct {
	PHYSFS_File *pf; /* set when using PHYSFS */
	FILE *sf;        /* set when using stdio  */
} bin_handle_t;

#ifdef ANDROID
static int path_get_proc_self_fd(const char *path)
{
	int fd = -1;
	if (!path) return -1;
	if (sscanf(path, "/proc/self/fd/%d", &fd) == 1 && fd >= 0)
		return fd;
	return -1;
}
#endif

static int path_is_local(const char *path)
{
	return path && (path[0] == '/' ||
	                (isalpha((unsigned char) path[0]) && path[1] == ':' &&
	                 (path[2] == '\\' || path[2] == '/')));
}

static int bh_valid(const bin_handle_t *h)
{
	return h->pf || h->sf;
}

static void bh_open(bin_handle_t *h, const char *path)
{
	h->pf = NULL;
	h->sf = NULL;
#ifdef ANDROID
	{
		int proc_fd = path_get_proc_self_fd(path);
		if (proc_fd >= 0) {
			int dup_fd = dup(proc_fd);
			if (dup_fd >= 0) {
				h->sf = fdopen(dup_fd, "rb");
				if (!h->sf)
					close(dup_fd);
			}
			if (h->sf)
				return;
		}
	}
#endif
	if (path_is_local(path)) {
		h->sf = fopen(path, "rb");
	} else {
		char buf[256];
		strncpy(buf, path, sizeof(buf) - 1);
		buf[sizeof(buf) - 1] = '\0';
		PHYSFSEXT_locateCorrectCase(buf);
		h->pf = PHYSFS_openRead(buf);
	}
}

static PHYSFS_sint64 bh_length(const bin_handle_t *h)
{
	if (h->pf) return PHYSFS_fileLength(h->pf);
	if (h->sf) {
#ifdef ANDROID
		int fd = fileno(h->sf);
		if (fd >= 0) {
			struct stat st;
			if (fstat(fd, &st) == 0 && st.st_size > 0)
				return (PHYSFS_sint64) st.st_size;
		}
#endif
		long cur = ftell(h->sf);
		fseek(h->sf, 0, SEEK_END);
		long len = ftell(h->sf);
		fseek(h->sf, cur, SEEK_SET);
		return (PHYSFS_sint64) len;
	}
	return -1;
}

static int bh_seek(const bin_handle_t *h, PHYSFS_sint64 offset)
{
	if (h->pf) return PHYSFS_seek(h->pf, offset);
	if (h->sf) return fseek(h->sf, (long) offset, SEEK_SET) == 0;
	return 0;
}

static int bh_read(const bin_handle_t *h, void *buf, int size)
{
	if (h->pf) return (int) PHYSFS_read(h->pf, buf, (PHYSFS_uint32) size, 1);
	if (h->sf) return (int) fread(buf, (size_t) size, 1, h->sf);
	return 0;
}

static void bh_close(bin_handle_t *h)
{
	if (h->pf) {
		PHYSFS_close(h->pf);
		h->pf = NULL;
	}
	if (h->sf) {
		fclose(h->sf);
		h->sf = NULL;
	}
}

/* ── Audio source ────────────────────────────────────────────────────── */

/* One BIN/CUE audio source (disc image) */
typedef struct {
	bin_handle_t bin_files[MAX_SOURCE_BINS];
	int num_bins;
	int first_combined; /* first 1-based combined track number */
	int audio_count;    /* number of audio tracks in this source */
	char disc_label[64];
	unsigned long legacy_disc_id; /* for songs_haved2_cd() compat */
} audio_source_t;

/* ── Track table ─────────────────────────────────────────────────────── */

/* Combined track table — tracks from all sources merged sequentially */
typedef struct {
	int type; /* 0 = data, 1 = audio */
	int start_sector;
	int num_sectors;
	int source_index; /* index into s_sources[] */
	int file_index;   /* index into source bin_files[] */
	char name[64];    /* track name from CUE/database */
} combined_track_t;

static combined_track_t s_tracks[MAX_TRACKS];
static int s_num_tracks = 0;
static audio_source_t s_sources[MAX_SOURCES];
static int s_num_sources = 0;
static char s_init_status[160] = "not initialized";
static void close_source_bins(audio_source_t *src);

static void clear_playlist_state(void)
{
	int i;
	for (i = 0; i < s_num_sources; i++)
		close_source_bins(&s_sources[i]);
	s_num_tracks = 0;
	s_num_sources = 0;
	memset(s_tracks, 0, sizeof(s_tracks));
	memset(s_sources, 0, sizeof(s_sources));
	track_names_set_cue_count(0);
}

/* Legacy single-source file handle (used when no audio_playlist.json) */
static bin_handle_t s_gog_handle = { NULL, NULL };
static int s_initialised = 0;
static int s_output_rate = 48000;

static void rba_set_status(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vsnprintf(s_init_status, sizeof(s_init_status), fmt, args);
	va_end(args);
}

/* ── Playback state ──────────────────────────────────────────────────── */

static volatile int s_playing = 0;
static volatile int s_paused = 0;
static volatile int s_bg_paused = 0;
static int s_current_track = 0; /* 1-based */
static int s_play_first = 0;
static int s_play_last = 0;
static int s_read_sector = 0; /* next sector to read */
static int s_track_end = 0;   /* sector past end of current track */
static float s_volume = 1.0f;
static int s_rb_underruns = 0; /* callback found buffer empty */
static int s_rb_cb_count = 0;  /* total callbacks */

/* Monotonic playback diagnostics.  The generation baselines let automation
 * distinguish work performed for the current request from stale activity. */
static unsigned int s_playback_generation = 0;
static unsigned long long s_source_sectors_read_total = 0;
static unsigned long long s_source_io_errors_total = 0;
static unsigned long long s_mixer_frames_delivered_total = 0;
static unsigned long long s_generation_source_read_start = 0;
static unsigned long long s_generation_source_error_start = 0;
static unsigned long long s_generation_mixer_frame_start = 0;
static int s_generation_first_track = 0;
static int s_generation_source_index = -1;
static unsigned int s_playback_diagnostics_sequence = 0;
static unsigned int s_proven_playback_generation = 0;
static unsigned long long s_proven_source_sectors_read = 0;
static unsigned long long s_proven_mixer_frames_delivered = 0;
static int s_proven_first_track = 0;
static int s_proven_source_index = -1;
static unsigned int s_playback_proof_sequence = 0;

enum {
	RBA_TERMINAL_NONE = 0,
	RBA_TERMINAL_COMPLETE = 1,
	RBA_TERMINAL_IO_ERROR = 2,
	RBA_TERMINAL_STOPPED = 3
};
enum {
	RBA_IO_NONE = 0,
	RBA_IO_HANDLE = 1,
	RBA_IO_SEEK = 2,
	RBA_IO_READ = 3
};
static int s_terminal_state = RBA_TERMINAL_STOPPED;
static int s_error_source_index = -1;
static int s_error_track = 0;
static int s_error_sector = 0;
static int s_error_operation = RBA_IO_NONE;
static int s_error_platform_code = 0;
#ifdef INTROSPECT_ON
static int s_test_source_failure_operation = RBA_IO_NONE;
#endif

static void (*s_finished_hook)(void) = NULL;
static volatile int s_song_finished = 0;

/* ── PCM input buffer (raw CD audio before resampling) ───────────────── */

#define PCM_BUF_FRAMES (FRAMES_PER_SECTOR * 16) /* 9408 frames ≈ 213 ms */

static short s_pcm_buf[PCM_BUF_FRAMES * 2]; /* stereo interleaved */
static int s_pcm_len = 0;                   /* valid frames in buffer  */
static int s_pcm_pos = 0;                   /* current read position   */

/* Resampling accumulator (fractional input position, 0.0–1.0) */
static double s_resample_frac = 0.0;

/* ── Ring buffer (identical pattern to TSF music) ────────────────────── */

#define RB_SHIFT   18
#define RB_SAMPLES (1 << RB_SHIFT) /* 262144 samples ≈ 2.7 s @ 48 kHz */
#define RB_MASK    (RB_SAMPLES - 1)

static short s_rb[RB_SAMPLES];
static volatile int s_rb_wpos = 0;
static volatile int s_rb_rpos = 0;

static SDL_Thread *s_render_thread = NULL;
static volatile int s_render_running = 0;
#ifdef ANDROID
static pthread_mutex_t s_background_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t s_background_cond = PTHREAD_COND_INITIALIZER;
static int s_render_thread_alive;
static int s_background_waiting;
#endif

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

/* ── CUE parser (uses standalone cue_parser module) ──────────────────── */

static void close_source_bins(audio_source_t *src)
{
	int i;
	for (i = 0; i < src->num_bins; i++)
		bh_close(&src->bin_files[i]);
	src->num_bins = 0;
}

/* Get the bin handle for a given combined track */
static bin_handle_t *get_track_handle(int combined_track_1based)
{
	int src_idx;
	int file_idx;
	if (combined_track_1based < 1 || combined_track_1based > s_num_tracks)
		return NULL;
	src_idx = s_tracks[combined_track_1based - 1].source_index;
	file_idx = s_tracks[combined_track_1based - 1].file_index;
	if (src_idx >= 0 && src_idx < s_num_sources) {
		audio_source_t *src = &s_sources[src_idx];
		if (file_idx >= 0 && file_idx < src->num_bins)
			return &src->bin_files[file_idx];
		if (src->num_bins > 0)
			return &src->bin_files[0];
	}
	/* Legacy single-source fallback */
	return &s_gog_handle;
}

static char *read_text_file_any(const char *path)
{
	PHYSFS_File *pf = NULL;
	PHYSFS_sint64 size;
	char *buf;

	if (path_is_local(path))
		return cd_read_file_exact(path, CD_CUE_MAX_BYTES, &buf, NULL) ? buf : NULL;

	pf = open_ci(path);
	if (!pf) return NULL;
	size = PHYSFS_fileLength(pf);
	if (size <= 0 || size > CD_CUE_MAX_BYTES ||
	    (PHYSFS_uint64) size > (PHYSFS_uint64) SIZE_MAX - 1 ||
	    (PHYSFS_uint64) size > (PHYSFS_uint64) UINT32_MAX) {
		PHYSFS_close(pf);
		return NULL;
	}

	buf = (char *) malloc((size_t) size + 1);
	if (!buf) {
		PHYSFS_close(pf);
		return NULL;
	}

	if (PHYSFS_read(pf, buf, 1, (PHYSFS_uint32) size) != size) {
		free(buf);
		PHYSFS_close(pf);
		return NULL;
	}
	PHYSFS_close(pf);

	buf[size] = '\0';
	return buf;
}

/* ── Minimal JSON helpers for audio_playlist.json ────────────────────── */

static const char *pj_skip_ws(const char *p)
{
	while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
	return p;
}

/* Extract a JSON string starting at '"'. Writes into buf[bufsz].
 * Advances *pp past the closing '"'. Returns 1 on success. */
static int pj_string(const char **pp, char *buf, int bufsz)
{
	const char *p = *pp;
	int i = 0;
	if (*p != '"') return 0;
	p++;
	while (*p && *p != '"') {
		if (*p == '\\') {
			p++;
			if (!*p) return 0;
		}
		if (i < bufsz - 1) buf[i++] = *p;
		p++;
	}
	if (*p != '"') return 0;
	buf[i] = '\0';
	*pp = ++p;
	return 1;
}

/* Parse a JSON integer. Advances *pp. */
static long pj_long(const char **pp)
{
	const char *p = *pp;
	long val = 0;
	int neg = 0;
	if (*p == '-') {
		neg = 1;
		p++;
	}
	while (*p >= '0' && *p <= '9') {
		val = val * 10 + (*p - '0');
		p++;
	}
	*pp = p;
	return neg ? -val : val;
}

/* Skip a JSON value (string, number, object, array, bool, null). */
static void pj_skip_value(const char **pp)
{
	const char *p = *pp;
	p = pj_skip_ws(p);
	if (*p == '"') {
		p++;
		while (*p && *p != '"') {
			if (*p == '\\') p++;
			p++;
		}
		if (*p == '"') p++;
	} else if (*p == '{') {
		int depth = 1;
		p++;
		while (*p && depth > 0) {
			if (*p == '{') depth++;
			else if (*p == '}') depth--;
			else if (*p == '"') {
				p++;
				while (*p && *p != '"') {
					if (*p == '\\') p++;
					p++;
				}
			}
			p++;
		}
	} else if (*p == '[') {
		int depth = 1;
		p++;
		while (*p && depth > 0) {
			if (*p == '[') depth++;
			else if (*p == ']') depth--;
			else if (*p == '"') {
				p++;
				while (*p && *p != '"') {
					if (*p == '\\') p++;
					p++;
				}
			}
			p++;
		}
	} else {
		/* number, true, false, null */
		while (*p && *p != ',' && *p != '}' && *p != ']') p++;
	}
	*pp = p;
}

/*
 * Parse a single CUE file for a given source and append tracks to the
 * combined table. cue_name may be an absolute filesystem path or a
 * PhysFS-relative filename. source_idx is which s_sources[] entry to
 * associate tracks with. Returns the number of tracks parsed, or 0 on
 * failure.
 */
static int parse_source_cue(const char *cue_name, const char *const *bin_names,
                            int num_bins, int source_idx)
{
	char *cue_text;
	cue_disc_t disc;
	long long bin_sizes[CUE_MAX_FILES];
	int base = s_num_tracks; /* where this source's tracks start */
	int i, count;

	memset(&disc, 0, sizeof(disc));
	memset(bin_sizes, 0, sizeof(bin_sizes));
	close_source_bins(&s_sources[source_idx]);

	RBA_DIAG("parse_source_cue src=%d cue=%s bins=%d mode=%s",
	         source_idx, cue_name, num_bins,
	         path_is_local(cue_name) ? "stdio" : "physfs");

	cue_text = read_text_file_any(cue_name);
	if (!cue_text) {
		RBA_LOG("parse_source_cue: cannot open %s", cue_name);
		rba_set_status("cue open failed: %s", cue_name);
		return 0;
	}

	count = cue_parse(cue_text, NULL, 0, &disc);
	if (count <= 0 || disc.num_tracks <= 0) {
		RBA_LOG("parse_source_cue: failed to parse %s", cue_name);
		rba_set_status("cue parse failed: %s", cue_name);
		free(cue_text);
		return 0;
	}
	if (disc.num_files <= 0 || disc.num_files > MAX_SOURCE_BINS || disc.num_files > num_bins) {
		RBA_LOG("parse_source_cue: cue expects %d BIN files, playlist has %d",
		        disc.num_files, num_bins);
		rba_set_status("cue/bin mismatch: cue=%d playlist=%d", disc.num_files, num_bins);
		free(cue_text);
		return 0;
	}
	if (base + disc.num_tracks > MAX_TRACKS) {
		RBA_LOG("parse_source_cue: too many tracks in %s", cue_name);
		rba_set_status("too many cue tracks: %s", cue_name);
		free(cue_text);
		return 0;
	}

	for (i = 0; i < disc.num_files; i++) {
		long long file_size;
		bh_open(&s_sources[source_idx].bin_files[i], bin_names[i]);
		if (!bh_valid(&s_sources[source_idx].bin_files[i])) {
			RBA_LOG("parse_source_cue: cannot open BIN %s", bin_names[i]);
			rba_set_status("bin open failed: %s", bin_names[i]);
			close_source_bins(&s_sources[source_idx]);
			free(cue_text);
			return 0;
		}
		file_size = (long long) bh_length(&s_sources[source_idx].bin_files[i]);
		if (file_size <= 0) {
			RBA_LOG("parse_source_cue: BIN file %s has invalid size %lld",
			        bin_names[i], file_size);
			rba_set_status("bin size invalid: %s", bin_names[i]);
			close_source_bins(&s_sources[source_idx]);
			free(cue_text);
			return 0;
		}
		bin_sizes[i] = file_size;
		s_sources[source_idx].num_bins = i + 1;
	}

	memset(&disc, 0, sizeof(disc));
	count = cue_parse(cue_text, bin_sizes, s_sources[source_idx].num_bins, &disc);
	free(cue_text);
	if (count <= 0 || disc.num_tracks <= 0) {
		RBA_LOG("parse_source_cue: failed to compute track lengths for %s", cue_name);
		rba_set_status("cue track lengths failed: %s", cue_name);
		close_source_bins(&s_sources[source_idx]);
		return 0;
	}

	for (i = 0; i < disc.num_tracks; i++) {
		const cue_track_info_t *track = &disc.tracks[i];
		combined_track_t *out = &s_tracks[s_num_tracks++];
		out->type = (track->type == CUE_TRACK_AUDIO) ? 1 : 0;
		out->start_sector = track->start_sector;
		out->num_sectors = track->num_sectors;
		out->source_index = source_idx;
		out->file_index = track->file_index;
		out->name[0] = '\0';
		if (track->title[0]) {
			strncpy(out->name, track->title, sizeof(out->name) - 1);
			out->name[sizeof(out->name) - 1] = '\0';
		}
	}

	/* Set source first_combined (1-based) and audio count */
	s_sources[source_idx].first_combined = base + 1;
	{
		int ac = 0;
		for (i = base; i < s_num_tracks; i++)
			if (s_tracks[i].type == 1) ac++;
		s_sources[source_idx].audio_count = ac;
	}

	RBA_LOG("Source %d (%s): %d tracks (%d audio) from %d BIN files",
	        source_idx, s_sources[source_idx].disc_label,
	        disc.num_tracks, s_sources[source_idx].audio_count,
	        s_sources[source_idx].num_bins);
	rba_set_status("playlist source ok: %d tracks, %d audio", disc.num_tracks,
	               s_sources[source_idx].audio_count);
	for (i = base; i < s_num_tracks; i++) {
		RBA_DIAG("track %d type=%s start=%d len=%d src=%d file=%d name=%s",
		         i + 1,
		         s_tracks[i].type ? "audio" : "data",
		         s_tracks[i].start_sector,
		         s_tracks[i].num_sectors,
		         s_tracks[i].source_index,
		         s_tracks[i].file_index,
		         s_tracks[i].name[0] ? s_tracks[i].name : "(none)");
	}

	return disc.num_tracks;
}

/*
 * Try to load audio_playlist.json written by AudioSourceManager.
 * Returns total number of tracks across all sources, or 0 if the
 * file doesn't exist or parsing fails.
 */
static int parse_audio_playlist(void)
{
	PHYSFS_File *f;
	PHYSFS_sint64 fsize;
	char *json;
	const char *p;
	char root_key[64];
	int total, audio_total = 0;

	f = PHYSFS_openRead("audio_playlist.json");
	if (!f) {
		rba_set_status("audio_playlist.json missing");
		return 0;
	}

	fsize = PHYSFS_fileLength(f);
	if (fsize <= 0 || fsize > 64 * 1024) {
		rba_set_status("audio_playlist.json invalid size: %lld", (long long) fsize);
		PHYSFS_close(f);
		return -1;
	}

	json = (char *) malloc((size_t) fsize + 1);
	if (!json) {
		rba_set_status("audio_playlist.json alloc failed");
		PHYSFS_close(f);
		return -1;
	}
	if (PHYSFS_read(f, json, 1, (PHYSFS_uint32) fsize) != fsize) {
		rba_set_status("audio_playlist.json read failed");
		free(json);
		PHYSFS_close(f);
		return -1;
	}
	json[fsize] = '\0';
	PHYSFS_close(f);

	clear_playlist_state();

	/* AudioSourceManager writes one root object containing the sources array. */
	p = pj_skip_ws(json);
	if (*p++ != '{') goto malformed;
	p = pj_skip_ws(p);
	if (!pj_string(&p, root_key, sizeof(root_key)) || strcmp(root_key, "sources") != 0)
		goto malformed;
	p = pj_skip_ws(p);
	if (*p++ != ':') goto malformed;
	p = pj_skip_ws(p);
	if (*p != '[') goto malformed;
	p++; /* skip '[' */

	for (;;) {
		char cue_name[256] = { 0 };
		char bin_names[CUE_MAX_FILES][256];
		const char *bin_ptrs[CUE_MAX_FILES];
		int num_bins = 0;
		char label[64] = { 0 };
		long legacy_id = 0;
		char key[64];
		/* Per-track names from fingerprint matching (keyed by 1-based CUE track num) */
		char tn_names[MAX_TRACKS][64];
		int tn_has_names = 0;
		int member_count = 0;

		memset(tn_names, 0, sizeof(tn_names));
		memset(bin_names, 0, sizeof(bin_names));
		memset(bin_ptrs, 0, sizeof(bin_ptrs));

		p = pj_skip_ws(p);
		if (*p == ']') {
			p++;
			break;
		}
		if (s_num_sources >= MAX_SOURCES) {
			rba_set_status("audio playlist exceeds %d sources", MAX_SOURCES);
			goto fail;
		}
		if (s_num_sources > 0) {
			if (*p++ != ',') goto malformed;
			p = pj_skip_ws(p);
		}
		if (*p != '{') goto malformed;
		p++; /* skip '{' */

		/* Parse object keys */
		for (;;) {
			p = pj_skip_ws(p);
			if (*p == '}') {
				p++;
				break;
			}
			if (member_count++ > 0) {
				if (*p++ != ',') goto malformed;
				p = pj_skip_ws(p);
			}

			if (!pj_string(&p, key, sizeof(key))) goto malformed;
			p = pj_skip_ws(p);
			if (*p++ != ':') goto malformed;
			p = pj_skip_ws(p);

			if (strcmp(key, "cue") == 0) {
				if (!pj_string(&p, cue_name, sizeof(cue_name))) goto malformed;
			} else if (strcmp(key, "bins") == 0) {
				/* Array of bin filenames -- preserve all entries for multi-BIN CUEs */
				int bin_index = 0;
				if (*p++ != '[') goto malformed;
				for (;;) {
					char bin_entry[256] = { 0 };
					p = pj_skip_ws(p);
					if (*p == ']') {
						p++;
						break;
					}
					if (bin_index++ > 0) {
						if (*p++ != ',') goto malformed;
						p = pj_skip_ws(p);
					}
					if (num_bins >= CUE_MAX_FILES) {
						rba_set_status("audio source exceeds %d BIN files", CUE_MAX_FILES);
						goto fail;
					}
					if (!pj_string(&p, bin_entry, sizeof(bin_entry))) goto malformed;
					strncpy(bin_names[num_bins], bin_entry, sizeof(bin_names[num_bins]) - 1);
					bin_names[num_bins][sizeof(bin_names[num_bins]) - 1] = '\0';
					bin_ptrs[num_bins] = bin_names[num_bins];
					num_bins++;
				}
			} else if (strcmp(key, "label") == 0) {
				if (!pj_string(&p, label, sizeof(label))) goto malformed;
			} else if (strcmp(key, "legacy_disc_id") == 0) {
				const char *number_start = p;
				legacy_id = pj_long(&p);
				if (p == number_start) goto malformed;
			} else if (strcmp(key, "track_names") == 0) {
				/* {"2":"Title","3":"Base Return",...} */
				int name_index = 0;
				if (*p++ != '{') goto malformed;
				for (;;) {
					char tk[16] = { 0 };
					char name[64] = { 0 };
					int tnum;
					p = pj_skip_ws(p);
					if (*p == '}') {
						p++;
						break;
					}
					if (name_index++ > 0) {
						if (*p++ != ',') goto malformed;
						p = pj_skip_ws(p);
					}
					if (!pj_string(&p, tk, sizeof(tk))) goto malformed;
					p = pj_skip_ws(p);
					if (*p++ != ':') goto malformed;
					p = pj_skip_ws(p);
					if (!pj_string(&p, name, sizeof(name))) goto malformed;
					tnum = atoi(tk);
					if (tnum >= 1 && tnum <= MAX_TRACKS) {
						strncpy(tn_names[tnum - 1], name, 63);
						tn_names[tnum - 1][63] = '\0';
						tn_has_names = 1;
					}
				}
			} else {
				pj_skip_value(&p);
			}
		}

		if (!cue_name[0] || num_bins <= 0) goto malformed;
		{
			int src = s_num_sources;
			int base = s_num_tracks;
			int parsed;
			RBA_DIAG("playlist source=%d label=%s cue=%s bins=%d first_bin=%s legacy_disc_id=%ld",
			         src, label[0] ? label : "Unknown", cue_name, num_bins,
			         bin_names[0][0] ? bin_names[0] : "(none)", legacy_id);
			memset(&s_sources[src], 0, sizeof(s_sources[src]));
			strncpy(s_sources[src].disc_label, label[0] ? label : "Unknown",
			        sizeof(s_sources[src].disc_label) - 1);
			s_sources[src].legacy_disc_id = (unsigned long) legacy_id;
			s_num_sources++;

			parsed = parse_source_cue(cue_name, bin_ptrs, num_bins, src);
			if (parsed <= 0) {
				goto fail;
			}

			/* Apply fingerprint-matched track names (override CUE titles) */
			if (tn_has_names) {
				int count = s_num_tracks - base;
				int i;
				for (i = 0; i < count && i < MAX_TRACKS; i++) {
					if (tn_names[i][0]) {
						strncpy(s_tracks[base + i].name, tn_names[i], 63);
						s_tracks[base + i].name[63] = '\0';
					}
				}
			}
		}
	}

	p = pj_skip_ws(p);
	if (*p++ != '}') goto malformed;
	p = pj_skip_ws(p);
	if (*p != '\0') goto malformed;

	free(json);
	json = NULL;
	for (total = 0; total < s_num_tracks; total++)
		if (s_tracks[total].type == 1) audio_total++;
	if (audio_total <= 0) {
		rba_set_status("audio playlist has no playable tracks");
		goto fail;
	}

	/* Set track names for the track_names system */
	track_names_set_cue_count(s_num_tracks);
	{
		int i;
		for (i = 0; i < s_num_tracks; i++) {
			if (s_tracks[i].name[0])
				track_names_set_cue_title(i + 1, s_tracks[i].name);
		}
	}

	total = s_num_tracks;
	if (total > 0)
		rba_set_status("playlist loaded: %d sources, %d tracks", s_num_sources, total);
	else
		rba_set_status("playlist had no usable tracks");
	RBA_LOG("audio_playlist.json: %d sources, %d total tracks",
	        s_num_sources, total);
	return total;

malformed:
	rba_set_status("audio_playlist.json malformed");
fail:
	free(json);
	clear_playlist_state();
	return -1;
}

static int parse_cue_file(void)
{
	PHYSFS_File *f;
	PHYSFS_sint64 file_size;
	int cur_track = -1;
	int i, total_sectors;
	unsigned char has_index[MAX_TRACKS] = { 0 };
	int malformed = 0;

	f = open_ci("descent_ii.inst");
	if (!f) {
		RBA_LOG("Could not open CUE file (descent_ii.inst)");
		rba_set_status("legacy cue missing: descent_ii.inst");
		return 0;
	}

	s_num_tracks = 0;
	memset(s_tracks, 0, sizeof(s_tracks));

	while (!PHYSFS_eof(f)) {
		char line[512];
		int li = 0;
		/* Read one line */
		while (li < (int) sizeof(line) - 1 && !PHYSFS_eof(f)) {
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
					s_tracks[tnum - 1].source_index = 0;
					s_tracks[tnum - 1].file_index = 0;
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
						int len = (int) (end - q);
						char title[64];
						if (len > 63) len = 63;
						memcpy(title, q, len);
						title[len] = '\0';
						track_names_set_cue_title(cur_track, title);
						strncpy(s_tracks[cur_track - 1].name, title, 63);
						s_tracks[cur_track - 1].name[63] = '\0';
					}
				}
				continue;
			}
		}
		/* INDEX 01 MM:SS:FF */
		{
			int idx;
			int sector;
			char msf[32];
			if (cur_track >= 1 &&
			    sscanf(line, " INDEX %d %31s", &idx, msf) == 2 && idx == 1) {
				sector = cue_msf_to_sector(msf);
				if (sector < 0 || has_index[cur_track - 1]) {
					malformed = 1;
					break;
				}
				s_tracks[cur_track - 1].start_sector = sector;
				has_index[cur_track - 1] = 1;
			}
		}
	}
	PHYSFS_close(f);
	if (malformed || s_num_tracks < 1) goto malformed_cue;
	for (i = 0; i < s_num_tracks; i++)
		if (!has_index[i]) goto malformed_cue;

	/* Open the BIN file */
	bh_open(&s_gog_handle, "descent_ii.gog");
	if (!bh_valid(&s_gog_handle)) {
		RBA_LOG("Could not open BIN file (descent_ii.gog)");
		s_num_tracks = 0;
		rba_set_status("legacy BIN missing: descent_ii.gog");
		return 0;
	}

	/* Compute track lengths from successive start positions */
	file_size = bh_length(&s_gog_handle);
	if (file_size <= 0 || file_size / SECTOR_SIZE > INT_MAX)
		goto malformed_cue;
	total_sectors = (int) (file_size / SECTOR_SIZE);

	for (i = 0; i < s_num_tracks; i++) {
		if (s_tracks[i].start_sector < 0 ||
		    s_tracks[i].start_sector >= total_sectors ||
		    (i + 1 < s_num_tracks &&
		     s_tracks[i + 1].start_sector <= s_tracks[i].start_sector))
			goto malformed_cue;
		if (i + 1 < s_num_tracks)
			s_tracks[i].num_sectors = s_tracks[i + 1].start_sector - s_tracks[i].start_sector;
		else
			s_tracks[i].num_sectors = total_sectors - s_tracks[i].start_sector;
	}

	/* Set up single legacy source only after validating complete geometry. */
	s_num_sources = 1;
	memset(&s_sources[0], 0, sizeof(s_sources[0]));
	s_sources[0].bin_files[0] = s_gog_handle;
	s_sources[0].num_bins = 1;
	/* Clear s_gog_handle so only s_sources[0] owns the handle */
	s_gog_handle.pf = NULL;
	s_gog_handle.sf = NULL;
	s_sources[0].first_combined = 1;
	s_sources[0].legacy_disc_id = GOG_D2_DISCID;
	strncpy(s_sources[0].disc_label, "Descent II (GOG)", sizeof(s_sources[0].disc_label) - 1);

	/* Count audio tracks in source */
	{
		int ac = 0;
		for (i = 0; i < s_num_tracks; i++)
			if (s_tracks[i].type == 1) ac++;
		s_sources[0].audio_count = ac;
	}
	track_names_set_cue_count(s_num_tracks);

	for (i = 0; i < s_num_tracks; i++) {
		RBA_LOG("Track %d: %s  start=%d  len=%d",
		        i + 1, s_tracks[i].type ? "audio" : "data",
		        s_tracks[i].start_sector, s_tracks[i].num_sectors);
	}
	rba_set_status("legacy cue loaded: %d tracks", s_num_tracks);

	return s_num_tracks;

malformed_cue:
	bh_close(&s_gog_handle);
	s_num_tracks = 0;
	memset(s_tracks, 0, sizeof(s_tracks));
	track_names_set_cue_count(0);
	rba_set_status("legacy cue malformed");
	return 0;
}

/* ── Sector I/O ──────────────────────────────────────────────────────── */

/* Refill s_pcm_buf with raw CD audio frames.  Returns the number of new
 * stereo frames appended, 0 when the current track range is done, or -1
 * after a source I/O failure with no newly appended frames. */
static void record_source_io_error(int operation, const bin_handle_t *source)
{
	int source_index = -1;
	if (__atomic_load_n(&s_terminal_state, __ATOMIC_ACQUIRE) == RBA_TERMINAL_IO_ERROR)
		return;
	if (s_current_track >= 1 && s_current_track <= s_num_tracks)
		source_index = s_tracks[s_current_track - 1].source_index;
	s_error_source_index = source_index;
	s_error_track = s_current_track;
	s_error_sector = s_read_sector;
	s_error_operation = operation;
	s_error_platform_code = !source ? 0 : source->pf ? (int) PHYSFS_getLastErrorCode()
	                                                 : errno;
	__atomic_add_fetch(&s_source_io_errors_total, 1, __ATOMIC_RELAXED);
	__atomic_store_n(&s_terminal_state, RBA_TERMINAL_IO_ERROR, __ATOMIC_RELEASE);
	RBA_LOG("CD source I/O error: source=%d track=%d sector=%d operation=%d code=%d",
	        source_index, s_current_track, s_read_sector, operation, s_error_platform_code);
}

static int find_audio_track(int first, int last)
{
	int track;

	if (first < 1)
		first = 1;
	if (last > s_num_tracks)
		last = s_num_tracks;
	for (track = first; track <= last; track++)
		if (s_tracks[track - 1].type == 1)
			return track;
	return 0;
}

static int refill_pcm(void)
{
	int frames_read = 0;
	if (__atomic_load_n(&s_terminal_state, __ATOMIC_ACQUIRE) == RBA_TERMINAL_IO_ERROR)
		return -1;

	/* Shift unconsumed data to front */
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
		PHYSFS_sint64 offset;
		int i;

#ifdef INTROSPECT_ON
		{
			int test_operation = __atomic_exchange_n(&s_test_source_failure_operation,
			                                         RBA_IO_NONE, __ATOMIC_ACQ_REL);
			if (test_operation != RBA_IO_NONE) {
				record_source_io_error(test_operation, NULL);
				break;
			}
		}
#endif

		/* Advance track if current one is exhausted */
		while (s_read_sector >= s_track_end) {
			int next_track = find_audio_track(s_current_track + 1, s_play_last);
			if (next_track == 0)
				return frames_read; /* all tracks done */
			s_current_track = next_track;
			s_read_sector = s_tracks[s_current_track - 1].start_sector;
			s_track_end = s_read_sector + s_tracks[s_current_track - 1].num_sectors;
		}

		{
			bin_handle_t *src = get_track_handle(s_current_track);
			if (!src || !bh_valid(src)) {
				record_source_io_error(RBA_IO_HANDLE, src);
				break;
			}
			offset = (PHYSFS_sint64) s_read_sector * SECTOR_SIZE;
			if (!bh_seek(src, offset)) {
				record_source_io_error(RBA_IO_SEEK, src);
				break;
			}
			if (!bh_read(src, raw, SECTOR_SIZE)) {
				record_source_io_error(RBA_IO_READ, src);
				break;
			}
		}
		__atomic_add_fetch(&s_source_sectors_read_total, 1, __ATOMIC_RELAXED);

		/* Decode 16-bit LE stereo PCM */
		for (i = 0; i < FRAMES_PER_SECTOR; i++) {
			int b = i * 4;
			s_pcm_buf[(s_pcm_len + i) * 2] = (short) ((unsigned short) raw[b] | ((unsigned short) raw[b + 1] << 8));
			s_pcm_buf[(s_pcm_len + i) * 2 + 1] = (short) ((unsigned short) raw[b + 2] | ((unsigned short) raw[b + 3] << 8));
		}
		s_pcm_len += FRAMES_PER_SECTOR;
		frames_read += FRAMES_PER_SECTOR;
		s_read_sector++;
	}

	if (frames_read == 0 &&
	    __atomic_load_n(&s_terminal_state, __ATOMIC_ACQUIRE) == RBA_TERMINAL_IO_ERROR)
		return -1;
	return frames_read;
}

/* ── Resampling render (44100 → output rate) ─────────────────────────── */

/* Render up to max_frames stereo output frames.  Returns actual count. */
static int render_cd_frames(short *out, int max_frames)
{
	const double ratio = (double) CD_SAMPLE_RATE / (double) s_output_rate;
	int written = 0;

	while (written < max_frames && s_playing) {
		/* Need more input? */
		if (s_pcm_pos >= s_pcm_len - 1) {
			int refill_result = refill_pcm();
			if (refill_result <= 0 && s_pcm_pos >= s_pcm_len - 1) {
				s_playing = 0;
				s_paused = 0;
				if (refill_result == 0) {
					__atomic_store_n(&s_terminal_state, RBA_TERMINAL_COMPLETE, __ATOMIC_RELEASE);
					s_song_finished = 1;
				}
				break;
			}
		}

		/* Linear interpolation */
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

		/* Advance integer part of input position */
		{
			int consumed = (int) s_resample_frac;
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
	(void) data;

	RBA_LOG("CD render thread started");

	while (__atomic_load_n(&s_render_running, __ATOMIC_SEQ_CST)) {
		android_lifecycle_diagnostics_count(ANDROID_LIFECYCLE_COUNTER_REDBOOK_PRODUCER_WAKE);
		if (__atomic_load_n(&s_bg_paused, __ATOMIC_ACQUIRE)) {
			pthread_mutex_lock(&s_background_mutex);
			s_background_waiting = 1;
			pthread_cond_broadcast(&s_background_cond);
			while (__atomic_load_n(&s_bg_paused, __ATOMIC_ACQUIRE) &&
			       __atomic_load_n(&s_render_running, __ATOMIC_SEQ_CST))
				pthread_cond_wait(&s_background_cond, &s_background_mutex);
			s_background_waiting = 0;
			pthread_mutex_unlock(&s_background_mutex);
			continue;
		}
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

	pthread_mutex_lock(&s_background_mutex);
	s_background_waiting = 0;
	s_render_thread_alive = 0;
	pthread_cond_broadcast(&s_background_cond);
	pthread_mutex_unlock(&s_background_mutex);
	RBA_LOG("CD render thread exiting");
	return 0;
}

static int render_thread_start(void)
{
	if (s_render_thread) return 1;
	rb_reset();
	__atomic_store_n(&s_render_running, 1, __ATOMIC_SEQ_CST);
	pthread_mutex_lock(&s_background_mutex);
	s_render_thread_alive = 1;
	pthread_mutex_unlock(&s_background_mutex);
	s_render_thread = SDL_CreateThread(render_thread_func, NULL);
	if (!s_render_thread) {
		__atomic_store_n(&s_render_running, 0, __ATOMIC_SEQ_CST);
		pthread_mutex_lock(&s_background_mutex);
		s_render_thread_alive = 0;
		pthread_cond_broadcast(&s_background_cond);
		pthread_mutex_unlock(&s_background_mutex);
		return 0;
	}
	return 1;
}

static void render_thread_stop(void)
{
	if (!s_render_thread) return;
	__atomic_store_n(&s_render_running, 0, __ATOMIC_SEQ_CST);
	pthread_mutex_lock(&s_background_mutex);
	pthread_cond_broadcast(&s_background_cond);
	pthread_mutex_unlock(&s_background_mutex);
	SDL_WaitThread(s_render_thread, NULL);
	s_render_thread = NULL;
}

/* ── Mix_HookMusic callback ──────────────────────────────────────────── */

static void rba_music_callback(void *udata, Uint8 *stream, int len)
{
	int needed = len / (int) sizeof(short);
	short *out = (short *) stream;
	int got;
	(void) udata;

	s_rb_cb_count++;

	if (!s_playing || s_paused) {
		memset(stream, 0, len);
		return;
	}

	got = rb_read(out, needed);
	if (got > 0) {
		unsigned int generation;
		unsigned long long source_total;
		unsigned long long mixer_total;
		unsigned long long source_start;
		unsigned long long mixer_start;

		__atomic_add_fetch(&s_mixer_frames_delivered_total,
		                   (unsigned long long) got / 2u, __ATOMIC_RELAXED);
		generation = __atomic_load_n(&s_playback_generation, __ATOMIC_ACQUIRE);
		source_total = __atomic_load_n(&s_source_sectors_read_total, __ATOMIC_RELAXED);
		mixer_total = __atomic_load_n(&s_mixer_frames_delivered_total, __ATOMIC_RELAXED);
		source_start = __atomic_load_n(&s_generation_source_read_start, __ATOMIC_RELAXED);
		mixer_start = __atomic_load_n(&s_generation_mixer_frame_start, __ATOMIC_RELAXED);
		if (generation > 0 && source_total > source_start && mixer_total > mixer_start) {
			__atomic_add_fetch(&s_playback_proof_sequence, 1, __ATOMIC_ACQ_REL);
			__atomic_store_n(&s_proven_source_sectors_read, source_total - source_start, __ATOMIC_RELAXED);
			__atomic_store_n(&s_proven_mixer_frames_delivered, mixer_total - mixer_start, __ATOMIC_RELAXED);
			__atomic_store_n(&s_proven_first_track,
			                 __atomic_load_n(&s_generation_first_track, __ATOMIC_RELAXED),
			                 __ATOMIC_RELAXED);
			__atomic_store_n(&s_proven_source_index,
			                 __atomic_load_n(&s_generation_source_index, __ATOMIC_RELAXED),
			                 __ATOMIC_RELAXED);
			__atomic_store_n(&s_proven_playback_generation, generation, __ATOMIC_RELAXED);
			__atomic_add_fetch(&s_playback_proof_sequence, 1, __ATOMIC_RELEASE);
		}
	}
	if (got < needed) {
		memset(out + got, 0, (needed - got) * (int) sizeof(short));
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
			out[i] = (short) (out[i] * s_volume);
	}
}

/* ── Public RBA API ──────────────────────────────────────────────────── */

void RBAInit(void)
{
	int playlist_tracks;
	if (s_initialised) return;
	rba_set_status("initializing redbook");

	/* Query SDL mixer output rate */
	{
		int freq = 0;
		Uint16 fmt;
		int ch;
		if (Mix_QuerySpec(&freq, &fmt, &ch) && freq > 0)
			s_output_rate = freq;
		else
			s_output_rate = 48000;
		RBA_LOG("Output rate: %d Hz", s_output_rate);
	}

	playlist_tracks = parse_audio_playlist();
	if (playlist_tracks > 0) {
		RBA_LOG("Loaded multi-source playlist");
	} else if (playlist_tracks < 0) {
		RBA_LOG("Rejected invalid audio playlist: %s", s_init_status);
		return;
	} else if (parse_cue_file() < 1) {
		RBA_LOG("No usable tracks found in CUE/BIN");
		rba_set_status("no usable tracks found");
		s_num_tracks = 0;
		if (bh_valid(&s_gog_handle)) {
			bh_close(&s_gog_handle);
		}
		return;
	}

	s_initialised = 1;
	rba_set_status("ready: %d tracks, %d audio", s_num_tracks, RBAGetNumAudioTracks());
	RBAList();
}

void RBAExit(void)
{
	int i;
	RBAStop();
	render_thread_stop();
	/* Close all source BIN files */
	for (i = 0; i < s_num_sources; i++) {
		close_source_bins(&s_sources[i]);
	}
	bh_close(&s_gog_handle);
	s_initialised = 0;
	s_num_tracks = 0;
	s_num_sources = 0;
}

int RBAEnabled(void)
{
	return s_initialised;
}

static void playback_diagnostics_begin(int first_track)
{
	__atomic_store_n(&s_terminal_state, RBA_TERMINAL_NONE, __ATOMIC_RELEASE);
	s_error_source_index = -1;
	s_error_track = 0;
	s_error_sector = 0;
	s_error_operation = RBA_IO_NONE;
	s_error_platform_code = 0;
#ifdef INTROSPECT_ON
	__atomic_store_n(&s_test_source_failure_operation, RBA_IO_NONE, __ATOMIC_RELEASE);
#endif
	__atomic_add_fetch(&s_playback_diagnostics_sequence, 1, __ATOMIC_ACQ_REL);
	__atomic_store_n(&s_generation_source_read_start,
	                 __atomic_load_n(&s_source_sectors_read_total, __ATOMIC_RELAXED),
	                 __ATOMIC_RELAXED);
	__atomic_store_n(&s_generation_source_error_start,
	                 __atomic_load_n(&s_source_io_errors_total, __ATOMIC_RELAXED),
	                 __ATOMIC_RELAXED);
	__atomic_store_n(&s_generation_mixer_frame_start,
	                 __atomic_load_n(&s_mixer_frames_delivered_total, __ATOMIC_RELAXED),
	                 __ATOMIC_RELAXED);
	__atomic_store_n(&s_generation_first_track, first_track, __ATOMIC_RELAXED);
	__atomic_store_n(&s_generation_source_index,
	                 s_tracks[first_track - 1].source_index, __ATOMIC_RELAXED);
	__atomic_add_fetch(&s_playback_generation, 1, __ATOMIC_RELAXED);
	__atomic_add_fetch(&s_playback_diagnostics_sequence, 1, __ATOMIC_RELEASE);
}

void RBAGetPlaybackDiagnostics(unsigned int *generation, int *first_track,
                               int *source_index,
                               unsigned long long *source_sectors_read_total,
                               unsigned long long *mixer_frames_delivered_total,
                               unsigned long long *generation_source_sectors_read,
                               unsigned long long *generation_mixer_frames_delivered)
{
	unsigned int before_sequence;
	unsigned int after_sequence;
	unsigned int before;
	unsigned long long source_total;
	unsigned long long mixer_total;
	unsigned long long source_start;
	unsigned long long mixer_start;
	int snapshot_first_track;
	int snapshot_source_index;

	for (;;) {
		before_sequence = __atomic_load_n(&s_playback_diagnostics_sequence, __ATOMIC_ACQUIRE);
		if (before_sequence & 1u) continue;
		before = __atomic_load_n(&s_playback_generation, __ATOMIC_RELAXED);
		snapshot_first_track = __atomic_load_n(&s_generation_first_track, __ATOMIC_RELAXED);
		snapshot_source_index = __atomic_load_n(&s_generation_source_index, __ATOMIC_RELAXED);
		source_start = __atomic_load_n(&s_generation_source_read_start, __ATOMIC_RELAXED);
		mixer_start = __atomic_load_n(&s_generation_mixer_frame_start, __ATOMIC_RELAXED);
		source_total = __atomic_load_n(&s_source_sectors_read_total, __ATOMIC_RELAXED);
		mixer_total = __atomic_load_n(&s_mixer_frames_delivered_total, __ATOMIC_RELAXED);
		after_sequence = __atomic_load_n(&s_playback_diagnostics_sequence, __ATOMIC_ACQUIRE);
		if (before_sequence == after_sequence && !(after_sequence & 1u)) break;
	}

	if (generation) *generation = before;
	if (first_track) *first_track = snapshot_first_track;
	if (source_index) *source_index = snapshot_source_index;
	if (source_sectors_read_total) *source_sectors_read_total = source_total;
	if (mixer_frames_delivered_total) *mixer_frames_delivered_total = mixer_total;
	if (generation_source_sectors_read)
		*generation_source_sectors_read = source_total >= source_start ? source_total - source_start : 0;
	if (generation_mixer_frames_delivered)
		*generation_mixer_frames_delivered = mixer_total >= mixer_start ? mixer_total - mixer_start : 0;
}

void RBAGetPlaybackErrorDiagnostics(unsigned long long *source_io_errors_total,
                                    unsigned long long *generation_source_io_errors)
{
	unsigned int before_sequence;
	unsigned int after_sequence;
	unsigned long long error_total;
	unsigned long long error_start;

	for (;;) {
		before_sequence = __atomic_load_n(&s_playback_diagnostics_sequence, __ATOMIC_ACQUIRE);
		if (before_sequence & 1u) continue;
		error_start = __atomic_load_n(&s_generation_source_error_start, __ATOMIC_RELAXED);
		error_total = __atomic_load_n(&s_source_io_errors_total, __ATOMIC_RELAXED);
		after_sequence = __atomic_load_n(&s_playback_diagnostics_sequence, __ATOMIC_ACQUIRE);
		if (before_sequence == after_sequence && !(after_sequence & 1u)) break;
	}

	if (source_io_errors_total) *source_io_errors_total = error_total;
	if (generation_source_io_errors)
		*generation_source_io_errors = error_total >= error_start ? error_total - error_start : 0;
}

void RBAGetPlaybackTerminalDiagnostics(int *terminal_state, int *source_index,
                                       int *track, int *sector,
                                       int *operation, int *platform_code)
{
	int state = __atomic_load_n(&s_terminal_state, __ATOMIC_ACQUIRE);
	if (terminal_state) *terminal_state = state;
	if (source_index) *source_index = s_error_source_index;
	if (track) *track = s_error_track;
	if (sector) *sector = s_error_sector;
	if (operation) *operation = s_error_operation;
	if (platform_code) *platform_code = s_error_platform_code;
}

#ifdef INTROSPECT_ON
int RBAInvalidateCurrentSourceForTest(void)
{
	int write_position;
	if (!s_playing) return 0;
	__atomic_store_n(&s_test_source_failure_operation, RBA_IO_HANDLE, __ATOMIC_RELEASE);
	write_position = __atomic_load_n(&s_rb_wpos, __ATOMIC_ACQUIRE);
	__atomic_store_n(&s_rb_rpos, write_position, __ATOMIC_RELEASE);
	return 1;
}
#endif

void RBAGetLastPlaybackProof(unsigned int *generation, int *first_track,
                             int *source_index,
                             unsigned long long *source_sectors_read,
                             unsigned long long *mixer_frames_delivered)
{
	unsigned int before_sequence;
	unsigned int after_sequence;
	unsigned int before;
	int snapshot_first_track;
	int snapshot_source_index;
	unsigned long long snapshot_source_sectors_read;
	unsigned long long snapshot_mixer_frames_delivered;

	for (;;) {
		before_sequence = __atomic_load_n(&s_playback_proof_sequence, __ATOMIC_ACQUIRE);
		if (before_sequence & 1u) continue;
		before = __atomic_load_n(&s_proven_playback_generation, __ATOMIC_RELAXED);
		snapshot_first_track = __atomic_load_n(&s_proven_first_track, __ATOMIC_RELAXED);
		snapshot_source_index = __atomic_load_n(&s_proven_source_index, __ATOMIC_RELAXED);
		snapshot_source_sectors_read = __atomic_load_n(&s_proven_source_sectors_read, __ATOMIC_RELAXED);
		snapshot_mixer_frames_delivered = __atomic_load_n(&s_proven_mixer_frames_delivered, __ATOMIC_RELAXED);
		after_sequence = __atomic_load_n(&s_playback_proof_sequence, __ATOMIC_ACQUIRE);
		if (before_sequence == after_sequence && !(after_sequence & 1u)) break;
	}

	if (generation) *generation = before;
	if (first_track) *first_track = snapshot_first_track;
	if (source_index) *source_index = snapshot_source_index;
	if (source_sectors_read) *source_sectors_read = snapshot_source_sectors_read;
	if (mixer_frames_delivered) *mixer_frames_delivered = snapshot_mixer_frames_delivered;
}

/* Start playing a single track (1-based) */
int RBAPlayTrack(int track)
{
	if (!s_initialised) return -1;
	if (track < 1 || track > s_num_tracks) return -1;
	if (s_tracks[track - 1].type != 1) return -1; /* not audio */

	RBAStop();

	s_current_track = track;
	s_play_first = track;
	s_play_last = track;
	s_read_sector = s_tracks[track - 1].start_sector;
	s_track_end = s_read_sector + s_tracks[track - 1].num_sectors;
	s_pcm_len = 0;
	s_pcm_pos = 0;
	s_resample_frac = 0.0;
	s_song_finished = 0;
	s_paused = 0;
	s_rb_underruns = 0;
	s_rb_cb_count = 0;
	playback_diagnostics_begin(track);
	s_playing = 1;

	if (!render_thread_start()) {
		RBAStop();
		return -1;
	}
	Mix_HookMusic(rba_music_callback, NULL);

	RBA_DIAG("play_track track=%d type=%s disc_id=0x%08lx orig_track_order=%d start=%d len=%d",
	         track,
	         s_tracks[track - 1].type ? "audio" : "data",
	         (unsigned long) RBAGetDiscID(), GameCfg.OrigTrackOrder,
	         s_tracks[track - 1].start_sector,
	         s_tracks[track - 1].num_sectors);
	RBA_LOG("Playing track %d (sectors %d–%d)", track, s_read_sector, s_track_end - 1);
	return track;
}

/* Play tracks first..last (inclusive, 1-based), call hook when done */
int RBAPlayTracks(int first, int last, void (*hook_finished)(void))
{
	int first_audio;

	if (!s_initialised) return 0;
	if (first < 1 || first > s_num_tracks) return 0;
	if (last < first) last = first;
	if (last > s_num_tracks) last = s_num_tracks;
	first_audio = find_audio_track(first, last);
	if (first_audio == 0) return 0;

	RBAStop();

	s_finished_hook = hook_finished;
	s_current_track = first_audio;
	s_play_first = first_audio;
	s_play_last = last;
	s_read_sector = s_tracks[first_audio - 1].start_sector;
	s_track_end = s_read_sector + s_tracks[first_audio - 1].num_sectors;
	s_pcm_len = 0;
	s_pcm_pos = 0;
	s_resample_frac = 0.0;
	s_song_finished = 0;
	s_paused = 0;
	s_rb_underruns = 0;
	s_rb_cb_count = 0;
	playback_diagnostics_begin(first_audio);
	s_playing = 1;

	if (!render_thread_start()) {
		RBAStop();
		return 0;
	}
	Mix_HookMusic(rba_music_callback, NULL);

	RBA_DIAG("play_tracks first=%d last=%d first_type=%s disc_id=0x%08lx orig_track_order=%d first_start=%d first_len=%d",
	         first_audio, last,
	         s_tracks[first_audio - 1].type ? "audio" : "data",
	         (unsigned long) RBAGetDiscID(), GameCfg.OrigTrackOrder,
	         s_tracks[first_audio - 1].start_sector,
	         s_tracks[first_audio - 1].num_sectors);
	RBA_LOG("Playing tracks %d–%d", first_audio, last);
	return 1;
}

void RBAStop(void)
{
	s_playing = 0;
	s_paused = 0;
	if (!s_initialised) {
		__atomic_store_n(&s_terminal_state, RBA_TERMINAL_STOPPED, __ATOMIC_RELEASE);
#ifdef INTROSPECT_ON
		__atomic_store_n(&s_test_source_failure_operation, RBA_IO_NONE, __ATOMIC_RELEASE);
#endif
		return;
	}

	render_thread_stop();
	Mix_HookMusic(NULL, NULL);
	s_finished_hook = NULL;
	s_song_finished = 0;
	__atomic_store_n(&s_terminal_state, RBA_TERMINAL_STOPPED, __ATOMIC_RELEASE);
#ifdef INTROSPECT_ON
	__atomic_store_n(&s_test_source_failure_operation, RBA_IO_NONE, __ATOMIC_RELEASE);
#endif
	rb_reset();

	RBA_LOG("Playback stopped");
}

void RBASetVolume(int volume)
{
	/* volume: 0–8 from the game */
	s_volume = (volume > 0) ? (float) volume / 8.0f : 0.0f;
}

void RBAPause(void)
{
	if (!s_initialised || !s_playing || s_paused ||
	    __atomic_load_n(&s_terminal_state, __ATOMIC_ACQUIRE) != RBA_TERMINAL_NONE)
		return;
	s_paused = 1;
	RBA_LOG("Paused");
}

int RBAResume(void)
{
	if (!s_initialised) return -1;
	if (!s_playing || !s_paused ||
	    __atomic_load_n(&s_terminal_state, __ATOMIC_ACQUIRE) != RBA_TERMINAL_NONE)
		return 0;
	s_paused = 0;
	RBA_LOG("Resumed");
	return 1;
}

void RBABackgroundPause(void)
{
	__atomic_store_n(&s_bg_paused, 1, __ATOMIC_RELEASE);
	pthread_mutex_lock(&s_background_mutex);
	while (s_render_thread_alive &&
	       __atomic_load_n(&s_render_running, __ATOMIC_SEQ_CST) &&
	       !s_background_waiting)
		pthread_cond_wait(&s_background_cond, &s_background_mutex);
	pthread_mutex_unlock(&s_background_mutex);
}

void RBABackgroundResume(void)
{
	__atomic_store_n(&s_bg_paused, 0, __ATOMIC_RELEASE);
	pthread_mutex_lock(&s_background_mutex);
	pthread_cond_broadcast(&s_background_cond);
	pthread_mutex_unlock(&s_background_mutex);
}

int RBAPauseResume(void)
{
	if (!s_initialised || !s_playing ||
	    __atomic_load_n(&s_terminal_state, __ATOMIC_ACQUIRE) != RBA_TERMINAL_NONE)
		return 0;
	if (s_paused) {
		s_paused = 0;
		RBA_LOG("Toggle → resumed");
	} else {
		s_paused = 1;
		RBA_LOG("Toggle → paused");
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
	if (__atomic_load_n(&s_terminal_state, __ATOMIC_ACQUIRE) == RBA_TERMINAL_IO_ERROR) return -2;
	if (s_playing && !s_paused) return 1;
	if (s_playing && s_paused) return -1;
	return 0;
}

unsigned long RBAGetDiscID(void)
{
	/* Return disc ID for the source that owns the current track,
	 * falling back to the first source's disc ID.  This lets
	 * songs_haved2_cd() recognise a GOG image as an original D2 CD. */
	int src_idx;
	if (!s_initialised) return 0;
	if (s_current_track >= 1 && s_current_track <= s_num_tracks) {
		src_idx = s_tracks[s_current_track - 1].source_index;
		if (src_idx >= 0 && src_idx < s_num_sources)
			return s_sources[src_idx].legacy_disc_id;
	}
	if (s_num_sources > 0)
		return s_sources[0].legacy_disc_id;
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
void RBAEjectDisk(void) {}
void RBASetStereoAudio(RBACHANNELCTL *channels)
{
	(void) channels;
}
void RBASetQuadAudio(RBACHANNELCTL *channels)
{
	(void) channels;
}
void RBAGetAudioInfo(RBACHANNELCTL *channels)
{
	(void) channels;
}
void RBASetChannelVolume(int channel, int volume)
{
	(void) channel;
	(void) volume;
}
void RBADisable(void)
{
	s_initialised = 0;
}
void RBAEnable(void)
{ /* re-init would be needed */
}

/* ── Track control functions (multi-source) ──────────────────────────── */

/* Find next audio track after the current one, wrapping around */
int RBANextTrack(void)
{
	int i, start;
	if (!s_initialised || s_num_tracks == 0) return -1;
	start = (s_current_track >= 1 && s_current_track <= s_num_tracks)
	            ? s_current_track
	            : 1;
	for (i = 1; i <= s_num_tracks; i++) {
		int idx = ((start - 1 + i) % s_num_tracks);
		if (s_tracks[idx].type == 1)
			return RBAPlayTrack(idx + 1);
	}
	return -1;
}

/* Find previous audio track before the current one, wrapping around */
int RBAPrevTrack(void)
{
	int i, start;
	if (!s_initialised || s_num_tracks == 0) return -1;
	start = (s_current_track >= 1 && s_current_track <= s_num_tracks)
	            ? s_current_track
	            : 1;
	for (i = 1; i <= s_num_tracks; i++) {
		int idx = ((start - 1 - i + s_num_tracks) % s_num_tracks);
		if (s_tracks[idx].type == 1)
			return RBAPlayTrack(idx + 1);
	}
	return -1;
}

/* Play a specific audio track by number (1-based) */
int RBAPlaySpecificTrack(int track)
{
	if (!s_initialised) return -1;
	if (track < 1 || track > s_num_tracks) return -1;
	if (s_tracks[track - 1].type != 1) return -1;
	return RBAPlayTrack(track);
}

/* Get info about the current track.  Returns 0 on success, -1 on error.
 * out_name may be NULL; if non-NULL, receives null-terminated track name. */
int RBAGetCurrentTrackInfo(int *out_track, char *out_name, int name_size,
                           int *out_source_index)
{
	if (!s_initialised || !s_playing || s_current_track < 1 || s_current_track > s_num_tracks)
		return -1;
	if (out_track) *out_track = s_current_track;
	if (out_name && name_size > 0) {
		strncpy(out_name, s_tracks[s_current_track - 1].name, name_size - 1);
		out_name[name_size - 1] = '\0';
	}
	if (out_source_index) *out_source_index = s_tracks[s_current_track - 1].source_index;
	return 0;
}

/* Return the number of audio-type (not data) tracks in the combined table */
int RBAGetNumAudioTracks(void)
{
	int i, count = 0;
	if (!s_initialised) return 0;
	for (i = 0; i < s_num_tracks; i++)
		if (s_tracks[i].type == 1) count++;
	return count;
}

/* Get the name of a track by 1-based number.  Returns empty string if invalid.
 * Falls back to track_names_lookup() for names set via the CUE title system. */
const char *RBAGetTrackName(int track)
{
	if (!s_initialised || track < 1 || track > s_num_tracks)
		return "";
	if (s_tracks[track - 1].name[0])
		return s_tracks[track - 1].name;
	/* Fallback: try track name system (fingerprint-matched or CUE titles) */
	{
		unsigned long disc_id = RBAGetDiscID();
		const char *name = track_names_lookup(track, disc_id);
		if (name) return name;
	}
	return "";
}

/* Check if a track (1-based) is an audio track.  Returns 1 for audio, 0 for data or invalid. */
int RBAIsAudioTrack(int track)
{
	if (!s_initialised || track < 1 || track > s_num_tracks)
		return 0;
	return s_tracks[track - 1].type == 1 ? 1 : 0;
}

const char *RBAGetInitStatus(void)
{
	return s_init_status;
}
