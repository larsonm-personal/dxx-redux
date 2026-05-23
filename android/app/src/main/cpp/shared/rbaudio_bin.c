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
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

#ifdef ANDROID
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
#include "../extract/cue_parser.h"

#ifdef ANDROID
#include <android/log.h>
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
static int s_current_track = 0; /* 1-based */
static int s_play_first = 0;
static int s_play_last = 0;
static int s_read_sector = 0; /* next sector to read */
static int s_track_end = 0;   /* sector past end of current track */
static float s_volume = 1.0f;
static int s_rb_underruns = 0; /* callback found buffer empty */
static int s_rb_cb_count = 0;  /* total callbacks */

static void (*s_finished_hook)(void) = NULL;
static volatile int s_song_finished = 0;
static int s_logged_data_track_decode = 0;

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

/* parse_msf kept for backward compat convenience */
static int parse_msf(const char *msf)
{
	int m = 0, s = 0, f = 0;
	if (sscanf(msf, "%d:%d:%d", &m, &s, &f) < 3) return 0;
	return m * 60 * 75 + s * 75 + f;
}

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
	FILE *sf = NULL;
	PHYSFS_sint64 size;
	char *buf;

	if (path_is_local(path)) {
		sf = fopen(path, "rb");
		if (!sf) return NULL;
		if (fseek(sf, 0, SEEK_END) != 0) {
			fclose(sf);
			return NULL;
		}
		size = (PHYSFS_sint64) ftell(sf);
		if (size < 0 || fseek(sf, 0, SEEK_SET) != 0) {
			fclose(sf);
			return NULL;
		}
	} else {
		pf = open_ci(path);
		if (!pf) return NULL;
		size = PHYSFS_fileLength(pf);
		if (size < 0) {
			PHYSFS_close(pf);
			return NULL;
		}
	}

	buf = (char *) malloc((size_t) size + 1);
	if (!buf) {
		if (sf)
			fclose(sf);
		else if (pf)
			PHYSFS_close(pf);
		return NULL;
	}

	if (sf) {
		if (fread(buf, 1, (size_t) size, sf) != (size_t) size) {
			free(buf);
			fclose(sf);
			return NULL;
		}
		fclose(sf);
	} else {
		if (PHYSFS_read(pf, buf, 1, (PHYSFS_uint32) size) != size) {
			free(buf);
			PHYSFS_close(pf);
			return NULL;
		}
		PHYSFS_close(pf);
	}

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
	int total;

	f = PHYSFS_openRead("audio_playlist.json");
	if (!f) {
		rba_set_status("audio_playlist.json missing");
		return 0;
	}

	fsize = PHYSFS_fileLength(f);
	if (fsize <= 0 || fsize > 64 * 1024) {
		rba_set_status("audio_playlist.json invalid size: %lld", (long long) fsize);
		PHYSFS_close(f);
		return 0;
	}

	json = (char *) malloc((size_t) fsize + 1);
	if (!json) {
		rba_set_status("audio_playlist.json alloc failed");
		PHYSFS_close(f);
		return 0;
	}
	if (PHYSFS_read(f, json, 1, (PHYSFS_uint32) fsize) != fsize) {
		rba_set_status("audio_playlist.json read failed");
		free(json);
		PHYSFS_close(f);
		return 0;
	}
	json[fsize] = '\0';
	PHYSFS_close(f);

	s_num_tracks = 0;
	s_num_sources = 0;
	memset(s_tracks, 0, sizeof(s_tracks));
	memset(s_sources, 0, sizeof(s_sources));

	/* Find "sources" array */
	p = strstr(json, "\"sources\"");
	if (!p) {
		rba_set_status("audio_playlist.json missing sources");
		free(json);
		return 0;
	}
	p += 9;
	p = pj_skip_ws(p);
	if (*p == ':') p++;
	p = pj_skip_ws(p);
	if (*p != '[') {
		rba_set_status("audio_playlist.json malformed sources");
		free(json);
		return 0;
	}
	p++; /* skip '[' */

	while (s_num_sources < MAX_SOURCES) {
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

		memset(tn_names, 0, sizeof(tn_names));
		memset(bin_names, 0, sizeof(bin_names));
		memset(bin_ptrs, 0, sizeof(bin_ptrs));

		p = pj_skip_ws(p);
		if (*p == ']') break;
		if (*p == ',') {
			p++;
			p = pj_skip_ws(p);
		}
		if (*p != '{') break;
		p++; /* skip '{' */

		/* Parse object keys */
		while (*p && *p != '}') {
			p = pj_skip_ws(p);
			if (*p == ',') {
				p++;
				p = pj_skip_ws(p);
			}
			if (*p == '}') break;

			if (!pj_string(&p, key, sizeof(key))) break;
			p = pj_skip_ws(p);
			if (*p == ':') p++;
			p = pj_skip_ws(p);

			if (strcmp(key, "cue") == 0) {
				pj_string(&p, cue_name, sizeof(cue_name));
			} else if (strcmp(key, "bins") == 0) {
				/* Array of bin filenames -- preserve all entries for multi-BIN CUEs */
				if (*p == '[') {
					p++;
					while (*p && *p != ']') {
						char bin_entry[256] = { 0 };
						p = pj_skip_ws(p);
						if (*p == ',') {
							p++;
							continue;
						}
						if (*p == ']') break;
						if (*p == '"' && pj_string(&p, bin_entry, sizeof(bin_entry))) {
							if (num_bins < CUE_MAX_FILES) {
								strncpy(bin_names[num_bins], bin_entry, sizeof(bin_names[num_bins]) - 1);
								bin_names[num_bins][sizeof(bin_names[num_bins]) - 1] = '\0';
								bin_ptrs[num_bins] = bin_names[num_bins];
								num_bins++;
							}
						} else {
							pj_skip_value(&p);
						}
					}
					if (*p == ']') p++;
				}
			} else if (strcmp(key, "label") == 0) {
				pj_string(&p, label, sizeof(label));
			} else if (strcmp(key, "legacy_disc_id") == 0) {
				legacy_id = pj_long(&p);
			} else if (strcmp(key, "track_names") == 0) {
				/* {"2":"Title","3":"Base Return",...} */
				if (*p == '{') {
					p++;
					while (*p && *p != '}') {
						char tk[16] = { 0 };
						int tnum;
						p = pj_skip_ws(p);
						if (*p == ',') {
							p++;
							continue;
						}
						if (*p == '}') break;
						if (!pj_string(&p, tk, sizeof(tk))) break;
						p = pj_skip_ws(p);
						if (*p == ':') p++;
						p = pj_skip_ws(p);
						tnum = atoi(tk);
						if (tnum >= 1 && tnum <= MAX_TRACKS) {
							pj_string(&p, tn_names[tnum - 1], 64);
							tn_has_names = 1;
						} else {
							pj_skip_value(&p);
						}
					}
					if (*p == '}') p++;
				}
			} else {
				pj_skip_value(&p);
			}
		}
		if (*p == '}') p++; /* skip '}' */

		if (cue_name[0] && num_bins > 0) {
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
				memset(&s_sources[src], 0, sizeof(s_sources[src]));
				s_num_sources--;
				continue;
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

	free(json);

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
			char msf[32];
			if (cur_track >= 1 &&
			    sscanf(line, " INDEX %d %31s", &idx, msf) == 2 && idx == 1) {
				s_tracks[cur_track - 1].start_sector = parse_msf(msf);
			}
		}
	}
	PHYSFS_close(f);

	track_names_set_cue_count(s_num_tracks);

	/* Open the BIN file */
	bh_open(&s_gog_handle, "descent_ii.gog");
	if (!bh_valid(&s_gog_handle)) {
		RBA_LOG("Could not open BIN file (descent_ii.gog)");
		s_num_tracks = 0;
		rba_set_status("legacy BIN missing: descent_ii.gog");
		return 0;
	}

	/* Set up single legacy source */
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

	/* Compute track lengths from successive start positions */
	file_size = bh_length(&s_sources[0].bin_files[0]);
	total_sectors = (int) (file_size / SECTOR_SIZE);

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
	rba_set_status("legacy cue loaded: %d tracks", s_num_tracks);

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

	while (s_pcm_len + FRAMES_PER_SECTOR <= PCM_BUF_FRAMES) {
		unsigned char raw[SECTOR_SIZE];
		PHYSFS_sint64 offset;
		int i;

		/* Advance track if current one is exhausted */
		while (s_read_sector >= s_track_end) {
			if (s_current_track >= s_play_last) {
				return frames_read; /* all tracks done */
			}
			s_current_track++;
			s_read_sector = s_tracks[s_current_track - 1].start_sector;
			s_track_end = s_read_sector + s_tracks[s_current_track - 1].num_sectors;
		}

		if (!s_logged_data_track_decode &&
		    s_current_track >= 1 && s_current_track <= s_num_tracks &&
		    s_tracks[s_current_track - 1].type != 1) {
			s_logged_data_track_decode = 1;
			RBA_DIAG("decoding data track current=%d play_range=%d-%d disc_id=0x%08lx orig_track_order=%d start=%d len=%d",
			         s_current_track, s_play_first, s_play_last,
			         (unsigned long) RBAGetDiscID(), GameCfg.OrigTrackOrder,
			         s_tracks[s_current_track - 1].start_sector,
			         s_tracks[s_current_track - 1].num_sectors);
		}

		{
			bin_handle_t *src = get_track_handle(s_current_track);
			if (!src || !bh_valid(src)) break;
			offset = (PHYSFS_sint64) s_read_sector * SECTOR_SIZE;
			if (!bh_seek(src, offset)) break;
			if (!bh_read(src, raw, SECTOR_SIZE)) break;
		}

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
			if (refill_pcm() == 0 && s_pcm_pos >= s_pcm_len - 1) {
				/* End of all tracks */
				s_playing = 0;
				s_song_finished = 1;
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

	if (parse_audio_playlist() >= 2) {
		RBA_LOG("Loaded multi-source playlist");
	} else if (parse_cue_file() < 2) {
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
	s_logged_data_track_decode = 0;
	s_playing = 1;

	render_thread_start();
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
	if (!s_initialised) return 0;
	if (first < 1 || first > s_num_tracks) return 0;
	if (last < first) last = first;
	if (last > s_num_tracks) last = s_num_tracks;

	RBAStop();

	s_finished_hook = hook_finished;
	s_current_track = first;
	s_play_first = first;
	s_play_last = last;
	s_read_sector = s_tracks[first - 1].start_sector;
	s_track_end = s_read_sector + s_tracks[first - 1].num_sectors;
	s_pcm_len = 0;
	s_pcm_pos = 0;
	s_resample_frac = 0.0;
	s_song_finished = 0;
	s_paused = 0;
	s_rb_underruns = 0;
	s_rb_cb_count = 0;
	s_logged_data_track_decode = 0;
	s_playing = 1;

	render_thread_start();
	Mix_HookMusic(rba_music_callback, NULL);

	RBA_DIAG("play_tracks first=%d last=%d first_type=%s disc_id=0x%08lx orig_track_order=%d first_start=%d first_len=%d",
	         first, last,
	         s_tracks[first - 1].type ? "audio" : "data",
	         (unsigned long) RBAGetDiscID(), GameCfg.OrigTrackOrder,
	         s_tracks[first - 1].start_sector,
	         s_tracks[first - 1].num_sectors);
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
	s_logged_data_track_decode = 0;
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
