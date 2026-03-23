/*
 * Track name overlay -- maps CD track numbers to human-readable names
 * and forwards the result to the Java overlay layer via JNI.
 *
 * Shared between D1 and D2 android builds.  Contains hardcoded track
 * name tables for both games' GOG disc images plus the CUE-parsed
 * title fallback and overlay formatting.
 */

#include <stdio.h>
#include <string.h>

#include "pstypes.h"
#include "songs.h"
#include "track_names.h"

#include "android_jni_overlay.h"

/* -- CUE-parsed titles (filled by rbaudio_bin.c CUE parser) ---------- */

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

/* -- Hardcoded track name tables ------------------------------------- */

#define D1_GOG_DISCID 0x2e0d3a05u
#define D2_GOG_DISCID 0x7d0ff809u

/* Descent I GOG disc image: track 1 = DATA, tracks 2-11 = audio */
static const char *d1_track_names[] = {
	NULL,                   /*  0 -- unused (1-based) */
	NULL,                   /*  1 -- DATA */
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

/* Descent II GOG disc image: track 1 = DATA, tracks 2-15 = audio */
static const char *d2_track_names[] = {
	NULL,              /*  0 -- unused (1-based) */
	NULL,              /*  1 -- DATA */
	"Title",           /*  2 */
	"Base Return",     /*  3 */
	"Crawl",           /*  4 */
	"Gunner Down",     /*  5 */
	"Ratzez",          /*  6 */
	"Techno Industry", /*  7 */
	"Are You Descent", /*  8 */
	"Robot Jungle",    /*  9 */
	"The Well",        /* 10 */
	"Haunted",         /* 11 */
	"Are You Descent", /* 12 */
	"Cold Reality",    /* 13 */
	"Robot Jungle",    /* 14 */
	"Final Mission",   /* 15 */
};

#define D1_TRACK_COUNT ((int) (sizeof(d1_track_names) / sizeof(d1_track_names[0])))
#define D2_TRACK_COUNT ((int) (sizeof(d2_track_names) / sizeof(d2_track_names[0])))

static const char *lookup_redbook_name(int track, unsigned long disc_id)
{
	const char *cue = track_names_get_cue_title(track);
	if (cue) return cue;

	if (disc_id == D1_GOG_DISCID &&
	    track >= 1 && track < D1_TRACK_COUNT && d1_track_names[track])
		return d1_track_names[track];

	if (disc_id == D2_GOG_DISCID &&
	    track >= 1 && track < D2_TRACK_COUNT && d2_track_names[track])
		return d2_track_names[track];

	return NULL;
}

const char *track_names_lookup(int track, unsigned long disc_id)
{
	return lookup_redbook_name(track, disc_id);
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
		const char *name = lookup_redbook_name(track_or_song, disc_id);
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
