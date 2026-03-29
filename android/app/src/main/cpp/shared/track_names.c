/*
 * Track name overlay -- maps CD track numbers to human-readable names
 * and forwards the result to the Java overlay layer via JNI.
 *
 * Shared between D1 and D2 android builds.  Track names are populated
 * at load time from audio_playlist.json (written by AudioSourceManager
 * with fingerprint-matched or known_discs.json5-sourced names).
 *
 * Jukebox (custom music) names come from custom_music_names.json,
 * a sidecar written by CustomAudioSetManager alongside the M3U playlist.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pstypes.h"
#include "songs.h"
#include "track_names.h"

#include "android_jni_overlay.h"
#include "overlay_ringbuf.h"

/* -- CUE-parsed / fingerprint-matched titles ------------------------- */
/* Populated by rbaudio_bin.c when loading audio_playlist.json.
 * Fingerprint-matched names from the playlist override CUE TITLE fields. */

#define MAX_CUE_TRACKS 100
#define CUE_TITLE_LEN  64

static char s_cue_titles[MAX_CUE_TRACKS][CUE_TITLE_LEN];
static int s_cue_title_count = 0;

void track_names_set_cue_count(int n)
{
	if (n > MAX_CUE_TRACKS) n = MAX_CUE_TRACKS;
	s_cue_title_count = n;
}

void track_names_set_cue_title(int track, const char *title)
{
	if (track < 1 || track > MAX_CUE_TRACKS) return;
	strncpy(s_cue_titles[track - 1], title, CUE_TITLE_LEN - 1);
	s_cue_titles[track - 1][CUE_TITLE_LEN - 1] = '\0';
}

const char *track_names_get_cue_title(int track)
{
	if (track < 1 || track > s_cue_title_count) return NULL;
	if (s_cue_titles[track - 1][0] == '\0') return NULL;
	return s_cue_titles[track - 1];
}

const char *track_names_lookup(int track, unsigned long disc_id)
{
	(void) disc_id;
	return track_names_get_cue_title(track);
}

/* -- Jukebox (custom music) name table ------------------------------- */
/* Loaded from custom_music_names.json written by CustomAudioSetManager.
 * Maps absolute file paths to chromaprint-decoded track names. */

#define MAX_JUKEBOX_NAMES 256
#define JUKEBOX_PATH_LEN  256
#define JUKEBOX_NAME_LEN  128

static char s_jb_paths[MAX_JUKEBOX_NAMES][JUKEBOX_PATH_LEN];
static char s_jb_names[MAX_JUKEBOX_NAMES][JUKEBOX_NAME_LEN];
static int s_jb_count = 0;

/* Minimal JSON string extraction: advance *pp past opening '"',
 * copy contents into buf (handling \" escapes), null-terminate.
 * Returns 1 on success, 0 if not a valid JSON string. */
static int jb_parse_string(const char **pp, char *buf, int bufsz)
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

void jukebox_names_load(const char *json_path)
{
	FILE *f;
	long len;
	char *buf, *p;

	s_jb_count = 0;

	f = fopen(json_path, "rb");
	if (!f) return;

	fseek(f, 0, SEEK_END);
	len = ftell(f);
	if (len <= 2 || len > 512 * 1024) {
		fclose(f);
		return;
	}
	fseek(f, 0, SEEK_SET);

	buf = (char *) malloc(len + 1);
	if (!buf) {
		fclose(f);
		return;
	}
	if (fread(buf, 1, len, f) != (size_t) len) {
		free(buf);
		fclose(f);
		return;
	}
	buf[len] = '\0';
	fclose(f);

	/* Parse flat JSON object: {"path":"name", ...} */
	p = buf;
	while (*p && *p != '{') p++;
	if (*p == '{') p++;

	while (*p && s_jb_count < MAX_JUKEBOX_NAMES) {
		while (*p && *p != '"' && *p != '}') p++;
		if (*p != '"') break;

		if (!jb_parse_string((const char **) &p,
		                     s_jb_paths[s_jb_count], JUKEBOX_PATH_LEN))
			break;

		while (*p && *p != ':') p++;
		if (*p == ':') p++;
		while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;

		if (!jb_parse_string((const char **) &p,
		                     s_jb_names[s_jb_count], JUKEBOX_NAME_LEN))
			break;

		s_jb_count++;
	}

	free(buf);
}

const char *jukebox_names_lookup(const char *filepath)
{
	int i;
	if (!filepath) return NULL;
	for (i = 0; i < s_jb_count; i++) {
		if (strcmp(s_jb_paths[i], filepath) == 0)
			return s_jb_names[i];
	}
	return NULL;
}

/* -- Overlay formatting ---------------------------------------------- */

#define OVERLAY_TEXT_LEN 80

static char s_overlay_text[OVERLAY_TEXT_LEN];

/* Return 1 if name is exactly "[unknown] - [untitled]" (useless AcoustID placeholder) */
static int is_placeholder_name(const char *name)
{
	return name && strcmp(name, "[unknown] - [untitled]") == 0;
}

void track_overlay_notify(int track_or_song, int is_midi, unsigned long disc_id)
{
	if (is_midi) {
		int display_num = track_or_song - SONG_FIRST_LEVEL_SONG + 1;
		if (display_num < 1) display_num = track_or_song;
		snprintf(s_overlay_text, OVERLAY_TEXT_LEN, "MIDI Track %d", display_num);
	} else {
		const char *name = track_names_lookup(track_or_song, disc_id);
		if (name && !is_placeholder_name(name))
			snprintf(s_overlay_text, OVERLAY_TEXT_LEN, "%s", name);
		else
			snprintf(s_overlay_text, OVERLAY_TEXT_LEN, "Track %d", track_or_song);
	}

	android_send_track_name(s_overlay_text);
	overlay_ringbuf_add("track", s_overlay_text);
}

void level_overlay_notify(int level_num, const char *level_name)
{
	char buf[OVERLAY_TEXT_LEN];
	int has_name = level_name && level_name[0];
	if (level_num < 0) {
		if (has_name)
			snprintf(buf, OVERLAY_TEXT_LEN, "Secret Level %d: %s", -level_num, level_name);
		else
			snprintf(buf, OVERLAY_TEXT_LEN, "Secret Level %d", -level_num);
	} else {
		if (has_name)
			snprintf(buf, OVERLAY_TEXT_LEN, "Level %d: %s", level_num, level_name);
		else
			snprintf(buf, OVERLAY_TEXT_LEN, "Level %d", level_num);
	}
	android_send_level_name(buf);
	overlay_ringbuf_add("level", buf);
}

void track_overlay_notify_jukebox(const char *filename)
{
	char buf[OVERLAY_TEXT_LEN];
	const char *name, *base, *p;

	if (!filename || !filename[0]) return;

	/* Try chromaprint-decoded name first */
	name = jukebox_names_lookup(filename);
	if (name && name[0]) {
		snprintf(buf, OVERLAY_TEXT_LEN, "%s", name);
		android_send_track_name(buf);
		overlay_ringbuf_add("jukebox", buf);
		return;
	}

	/* Fallback: strip path and extension */
	base = filename;
	for (p = filename; *p; p++) {
		if (*p == '/' || *p == '\\')
			base = p + 1;
	}

	strncpy(buf, base, OVERLAY_TEXT_LEN - 1);
	buf[OVERLAY_TEXT_LEN - 1] = '\0';
	p = strrchr(buf, '.');
	if (p)
		buf[p - buf] = '\0';

	android_send_track_name(buf);
	overlay_ringbuf_add("jukebox", buf);
}
