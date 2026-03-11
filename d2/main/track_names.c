/*
 * Track name overlay — maps CD track numbers to human-readable names
 * and forwards the result to the Java overlay layer via JNI.
 *
 * Two sources of names are tried, in priority order:
 *   1. TITLE fields parsed from the GOG CUE sheet (descent_ii.inst)
 *   2. A hardcoded table for Descent II CD tracks
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
#include <jni.h>
#include <android/log.h>
extern JavaVM  *g_jvm;
extern jobject  g_activity;
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

/* ── Hardcoded Descent II CD track names ─────────────────────────────
 *
 * These names are ONLY used when the disc ID matches the GOG release
 * (D2_1_DISCID / 0x7d0ff809).  Other disc variants have different
 * track orderings and will be added later.
 *
 * Physical CD layout (GOG disc image):
 *   Track  1 : DATA
 *   Tracks 2–15 : Audio
 *
 * The names below are indexed by physical 1-based track number.
 */

#define D2_GOG_DISCID 0x7d0ff809u

static const char *d2_track_names[] = {
	NULL,               /*  0 — unused (tracks are 1-based) */
	NULL,               /*  1 — DATA track */
	"Title",            /*  2 */
	"Base Return",      /*  3 */
	"Crawl",            /*  4 */
	"Gunner Down",      /*  5 */
	"Ratzez",           /*  6 */
	"Techno Industry",  /*  7 */
	"Are You Descent",  /*  8 */
	"Robot Jungle",     /*  9 */
	"The Well",         /* 10 */
	"Haunted",          /* 11 */
	"Are You Descent",  /* 12 */
	"Cold Reality",     /* 13 */
	"Robot Jungle",     /* 14 */
	"Final Mission",    /* 15 */
};

#define D2_TRACK_COUNT ((int)(sizeof(d2_track_names) / sizeof(d2_track_names[0])))

static const char *lookup_redbook_name(int track, unsigned long disc_id)
{
	/* Prefer CUE-parsed title */
	const char *cue = track_names_get_cue_title(track);
	if (cue) return cue;

	/* Hardcoded table — only for the GOG disc */
	if (disc_id == D2_GOG_DISCID &&
	    track >= 1 && track < D2_TRACK_COUNT && d2_track_names[track])
		return d2_track_names[track];

	return NULL;
}

/* ── Overlay state ───────────────────────────────────────────────────── */

#define OVERLAY_TEXT_LEN 80

static char  s_overlay_text[OVERLAY_TEXT_LEN];

/* Send track name to Java overlay on Android, no-op elsewhere */
static void send_track_name_to_java(const char *name)
{
#ifdef ANDROID
	JNIEnv *env;
	int attached = 0;
	if (!g_jvm || !g_activity) return;
	if ((*g_jvm)->GetEnv(g_jvm, (void**)&env, JNI_VERSION_1_6) != JNI_OK) {
		(*g_jvm)->AttachCurrentThread(g_jvm, &env, NULL);
		attached = 1;
	}
	jclass cls = (*env)->GetObjectClass(env, g_activity);
	jmethodID mid = (*env)->GetMethodID(env, cls, "showTrackName", "(Ljava/lang/String;)V");
	if (mid) {
		jstring jstr = (*env)->NewStringUTF(env, name);
		(*env)->CallVoidMethod(env, g_activity, mid, jstr);
		(*env)->DeleteLocalRef(env, jstr);
	}
	if (attached) (*g_jvm)->DetachCurrentThread(g_jvm);
#else
	(void)name;
#endif
}

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

	send_track_name_to_java(s_overlay_text);
}

/* ── Level name overlay ──────────────────────────────────────────────── */

static void send_level_name_to_java(const char *name)
{
#ifdef ANDROID
	JNIEnv *env;
	int attached = 0;
	if (!g_jvm || !g_activity) return;
	if ((*g_jvm)->GetEnv(g_jvm, (void**)&env, JNI_VERSION_1_6) != JNI_OK) {
		(*g_jvm)->AttachCurrentThread(g_jvm, &env, NULL);
		attached = 1;
	}
	jclass cls = (*env)->GetObjectClass(env, g_activity);
	jmethodID mid = (*env)->GetMethodID(env, cls, "showLevelName", "(Ljava/lang/String;)V");
	if (mid) {
		jstring jstr = (*env)->NewStringUTF(env, name);
		(*env)->CallVoidMethod(env, g_activity, mid, jstr);
		(*env)->DeleteLocalRef(env, jstr);
	}
	if (attached) (*g_jvm)->DetachCurrentThread(g_jvm);
#else
	(void)name;
#endif
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
	send_level_name_to_java(buf);
}
