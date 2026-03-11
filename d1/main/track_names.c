/*
 * Track name overlay — maps CD track numbers to human-readable names
 * and forwards the result to the Java overlay layer via JNI.
 *
 * Two sources of names are tried, in priority order:
 *   1. TITLE fields parsed from the GOG CUE sheet (descent.inst)
 *   2. A hardcoded table for Descent I CD tracks
 *
 * For MIDI tracks the overlay simply shows "MIDI Track N".
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "pstypes.h"
#include "songs.h"
#include "track_names.h"
#include "gameseq.h"

#ifdef ANDROID
#include "android_jni_overlay.h"
#endif

/* ── CUE-parsed titles (filled by track_names_parse_cue_titles) ────── */

#define MAX_CUE_TRACKS 100
#define CUE_TITLE_LEN   64

static char  s_cue_titles[MAX_CUE_TRACKS][CUE_TITLE_LEN];
static int   s_cue_title_count = 0;

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

/* ── Hardcoded Descent I CD track names ──────────────────────────────
 *
 * Physical CD layout (GOG disc image):
 *   Track  1 : DATA
 *   Tracks 2–11 : Audio (10 tracks)
 *
 * The names below are indexed by physical 1-based track number.
 */

#define D1_GOG_DISCID 0x2e0d3a05u

static const char *d1_track_names[] = {
	NULL,                   /*  0 — unused (tracks are 1-based) */
	NULL,                   /*  1 — DATA track */
	"Descent Main Theme",   /*  2 */
	"Adrenaline Nightmare", /*  3 */
	"Level 7 Briefing",     /*  4 */
	"Briefing Theme",       /*  5 */
	"Techno Industry",      /*  6 */
	"Primitive Screwhead",  /*  7 */
	"Untitled",             /*  8 */
	"Alien Encounter",      /*  9 */
	"Cold Reality",         /* 10 */
	"Robotic Menace",       /* 11 */
};

#define D1_TRACK_COUNT ((int)(sizeof(d1_track_names) / sizeof(d1_track_names[0])))

static const char *lookup_redbook_name(int track, unsigned long disc_id)
{
	/* Prefer CUE-parsed title */
	const char *cue = track_names_get_cue_title(track);
	if (cue) return cue;

	/* Hardcoded table — only for the GOG disc */
	if (disc_id == D1_GOG_DISCID &&
	    track >= 1 && track < D1_TRACK_COUNT && d1_track_names[track])
		return d1_track_names[track];

	return NULL;
}

/* ── Overlay state ───────────────────────────────────────────────────── */

#define OVERLAY_TEXT_LEN 80

static char  s_overlay_text[OVERLAY_TEXT_LEN];

void track_overlay_notify(int track_or_song, int is_midi, unsigned long disc_id)
{
	if (is_midi)
	{
		/* MIDI tracks: "MIDI Track N" where N is 1-based level song index */
		int display_num = track_or_song - SONG_FIRST_LEVEL_SONG + 1;
		if (display_num < 1) display_num = track_or_song;
		snprintf(s_overlay_text, OVERLAY_TEXT_LEN, "MIDI Track %d", display_num);
	}
	else
	{
		const char *name = lookup_redbook_name(track_or_song, disc_id);
		if (name)
			snprintf(s_overlay_text, OVERLAY_TEXT_LEN, "%s", name);
		else
			snprintf(s_overlay_text, OVERLAY_TEXT_LEN, "Track %d", track_or_song);
	}

#ifdef ANDROID
	android_send_track_name(s_overlay_text);
#endif
}

/* ── Level name overlay ──────────────────────────────────────────────── */

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
#ifdef ANDROID
	android_send_level_name(buf);
#endif
}
