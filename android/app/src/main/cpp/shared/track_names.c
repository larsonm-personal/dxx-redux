/*
 * Track name overlay -- maps CD track numbers to human-readable names
 * and forwards the result to the Java overlay layer via JNI.
 *
 * Shared between D1 and D2 android builds.  Track names are populated
 * at load time from audio_playlist.json (written by AudioSourceManager
 * with fingerprint-matched or known_discs.json5-sourced names).
 */

#include <stdio.h>
#include <string.h>

#include "pstypes.h"
#include "songs.h"
#include "track_names.h"

#include "android_jni_overlay.h"

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

/* -- Overlay formatting ---------------------------------------------- */

#define OVERLAY_TEXT_LEN 80

static char s_overlay_text[OVERLAY_TEXT_LEN];

void track_overlay_notify(int track_or_song, int is_midi, unsigned long disc_id)
{
	if (is_midi) {
		int display_num = track_or_song - SONG_FIRST_LEVEL_SONG + 1;
		if (display_num < 1) display_num = track_or_song;
		snprintf(s_overlay_text, OVERLAY_TEXT_LEN, "MIDI Track %d", display_num);
	} else {
		const char *name = track_names_lookup(track_or_song, disc_id);
		if (name)
			snprintf(s_overlay_text, OVERLAY_TEXT_LEN, "%s", name);
		else
			snprintf(s_overlay_text, OVERLAY_TEXT_LEN, "Track %d", track_or_song);
	}

	android_send_track_name(s_overlay_text);
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
}
