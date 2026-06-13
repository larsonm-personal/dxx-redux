/*
 * jni_music_control.c -- JNI bridge for in-game track control.
 *
 * Uses the unified songs_*() API so next/prev/play/info work across
 * all music types (BUILTIN/MIDI, REDBOOK, CUSTOM/jukebox).
 * Falls back to RBA* for Redbook-specific queries (track names, types).
 */

#include <jni.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <android/log.h>

#include "songs.h"
#include "rbaudio.h"
#include "config.h"
#include "digi.h"
#include "game.h"
#include "gameseq.h"
#include "playsave.h"

#define TAG       "DXX-MusicCtrl"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)

extern int tsf_music_get_paused(void);
static int music_clamp(int value, int min_value, int max_value)
{
	if (value < min_value)
		return min_value;
	if (value > max_value)
		return max_value;
	return value;
}

static void music_json_escape(const char *src, char *dst, int dst_size)
{
	int pos = 0;
	unsigned char ch;

	if (!dst || dst_size <= 0)
		return;
	if (!src)
		src = "";
	while ((ch = (unsigned char) *src++) != '\0' && pos < dst_size - 1) {
		switch (ch) {
			case '\\':
			case '"':
				if (pos >= dst_size - 2)
					goto done;
				dst[pos++] = '\\';
				dst[pos++] = (char) ch;
				break;
			case '\n':
				if (pos >= dst_size - 2)
					goto done;
				dst[pos++] = '\\';
				dst[pos++] = 'n';
				break;
			case '\r':
				if (pos >= dst_size - 2)
					goto done;
				dst[pos++] = '\\';
				dst[pos++] = 'r';
				break;
			case '\t':
				if (pos >= dst_size - 2)
					goto done;
				dst[pos++] = '\\';
				dst[pos++] = 't';
				break;
			default:
				if (ch < 0x20)
					ch = ' ';
				dst[pos++] = (char) ch;
				break;
		}
	}

done:
	dst[pos] = '\0';
}

static const char *music_current_source(void)
{
	switch (GameCfg.MusicType) {
		case MUSIC_TYPE_BUILTIN:
			return android_music_get_prefer_mission_soundtrack() ? "mission" : "midi";
		case MUSIC_TYPE_REDBOOK:
			return "cd";
		case MUSIC_TYPE_CUSTOM:
			return "files";
		default:
			return "none";
	}
}

static int music_is_paused(void)
{
	if (GameCfg.MusicType == MUSIC_TYPE_REDBOOK)
		return RBAPeekPlayStatus() < 0;
	if (GameCfg.MusicType == MUSIC_TYPE_BUILTIN || GameCfg.MusicType == MUSIC_TYPE_CUSTOM)
		return tsf_music_get_paused();
	return 0;
}

static void music_replay_current(void)
{
	songs_uninit();
	if (Game_wind)
		songs_play_level_song(Current_level_num, 0);
	else
		songs_play_song(SONG_TITLE, 1);
}

static void music_save_config(void)
{
	WriteConfigFile();
	if (Player_num >= 0 && Player_num < MAX_PLAYERS)
		write_player_file();
}

JNIEXPORT jint JNICALL
Java_com_dxxredux_app_MainActivity_nativeNextTrack(JNIEnv *env, jobject thiz)
{
	return songs_next_track();
}

JNIEXPORT jint JNICALL
Java_com_dxxredux_app_MainActivity_nativePrevTrack(JNIEnv *env, jobject thiz)
{
	return songs_prev_track();
}

JNIEXPORT jint JNICALL
Java_com_dxxredux_app_MainActivity_nativePlaySpecificTrack(
    JNIEnv *env, jobject thiz, jint track)
{
	return songs_play_specific_track(track);
}

JNIEXPORT jstring JNICALL
Java_com_dxxredux_app_MainActivity_nativeGetTrackName(
    JNIEnv *env, jobject thiz, jint track)
{
	const char *name = RBAGetTrackName(track);
	return (*env)->NewStringUTF(env, name ? name : "");
}

JNIEXPORT jint JNICALL
Java_com_dxxredux_app_MainActivity_nativeGetCurrentTrackNum(
    JNIEnv *env, jobject thiz)
{
	return RBAGetTrackNum();
}

JNIEXPORT jint JNICALL
Java_com_dxxredux_app_MainActivity_nativeGetNumAudioTracks(
    JNIEnv *env, jobject thiz)
{
	return RBAGetNumAudioTracks();
}

JNIEXPORT jint JNICALL
Java_com_dxxredux_app_MainActivity_nativeGetTotalTracks(
    JNIEnv *env, jobject thiz)
{
	return RBAGetNumberOfTracks();
}

JNIEXPORT jboolean JNICALL
Java_com_dxxredux_app_MainActivity_nativeIsAudioTrack(
    JNIEnv *env, jobject thiz, jint track)
{
	return RBAIsAudioTrack(track) ? JNI_TRUE : JNI_FALSE;
}

/*
 * Unified current track info: "musicType|trackIndex|totalTracks|trackName"
 * Works for BUILTIN, REDBOOK, and CUSTOM music types.
 * Returns empty string if nothing is playing.
 */
JNIEXPORT jstring JNICALL
Java_com_dxxredux_app_MainActivity_nativeGetCurrentTrackInfo(
    JNIEnv *env, jobject thiz)
{
	int type = 0, track = 0, total = 0;
	char name[128] = "";
	char buf[256];

	if (songs_get_track_info(&type, &track, &total, name, sizeof(name)) == 0) {
		snprintf(buf, sizeof(buf), "%d|%d|%d|%s", type, track, total, name);
	} else {
		buf[0] = '\0';
	}
	return (*env)->NewStringUTF(env, buf);
}

/* Returns GameCfg.MusicType (0=none, 1=builtin, 2=redbook, 3=custom). */
JNIEXPORT jint JNICALL
Java_com_dxxredux_app_MainActivity_nativeGetMusicType(
    JNIEnv *env, jobject thiz)
{
	return GameCfg.MusicType;
}

JNIEXPORT jstring JNICALL
Java_com_dxxredux_app_MainActivity_nativeGetMusicOverlayState(
    JNIEnv *env, jobject thiz)
{
	int type = 0, track = -1, total = 0;
	char name[128] = "";
	char escaped_name[256];
	char *tracks = (char *) malloc(32768);
	char *buf = (char *) malloc(33792);
	jstring result;
	(void) thiz;

	if (!tracks || !buf) {
		free(tracks);
		free(buf);
		return (*env)->NewStringUTF(env, "{}");
	}

	if (songs_get_track_info(&type, &track, &total, name, sizeof(name)) != 0) {
		type = GameCfg.MusicType;
		track = -1;
		total = 0;
		name[0] = '\0';
	}
	music_json_escape(name, escaped_name, sizeof(escaped_name));
	songs_get_track_list(tracks, 32768);

	snprintf(buf, 33792,
	         "{\"musicType\":%d,\"source\":\"%s\",\"preferMissionSoundtrack\":%d,"
	         "\"playOrder\":%d,\"oneTrackPerLevel\":%d,\"volume\":%d,"
	         "\"paused\":%d,\"currentTrack\":%d,\"totalTracks\":%d,"
	         "\"currentName\":\"%s\",\"tracks\":%s}",
	         type,
	         music_current_source(),
	         android_music_get_prefer_mission_soundtrack(),
	         GameCfg.CMLevelMusicPlayOrder,
	         GameCfg.CMLevelMusicPlayOrder == MUSIC_CM_PLAYORDER_LEVEL,
	         GameCfg.MusicVolume,
	         music_is_paused(),
	         track,
	         total,
	         escaped_name,
	         tracks);
	result = (*env)->NewStringUTF(env, buf);
	free(tracks);
	free(buf);
	return result;
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeSetMissionSoundtrackPreference(
    JNIEnv *env, jobject thiz, jboolean enabled)
{
	(void) env;
	(void) thiz;
	android_music_set_prefer_mission_soundtrack(enabled ? 1 : 0);
}

JNIEXPORT jboolean JNICALL
Java_com_dxxredux_app_MainActivity_nativeSetMusicSource(
    JNIEnv *env, jobject thiz, jstring source)
{
	const char *src;
	int new_type;
	int prefer_mission;
	(void) thiz;

	src = source ? (*env)->GetStringUTFChars(env, source, NULL) : NULL;
	if (!src)
		return JNI_FALSE;

	new_type = GameCfg.MusicType;
	prefer_mission = 0;
	if (!strcmp(src, "mission")) {
		new_type = MUSIC_TYPE_BUILTIN;
		prefer_mission = 1;
	} else if (!strcmp(src, "midi")) {
		new_type = MUSIC_TYPE_BUILTIN;
	} else if (!strcmp(src, "cd")) {
		new_type = MUSIC_TYPE_REDBOOK;
	} else if (!strcmp(src, "files")) {
		new_type = MUSIC_TYPE_CUSTOM;
	} else {
		(*env)->ReleaseStringUTFChars(env, source, src);
		return JNI_FALSE;
	}
	(*env)->ReleaseStringUTFChars(env, source, src);

	GameCfg.MusicType = new_type;
	android_music_set_prefer_mission_soundtrack(prefer_mission);
	music_save_config();
	music_replay_current();
	return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL
Java_com_dxxredux_app_MainActivity_nativeSetMusicOneTrackPerLevel(
    JNIEnv *env, jobject thiz, jboolean enabled)
{
	int play_order = enabled ? MUSIC_CM_PLAYORDER_LEVEL : MUSIC_CM_PLAYORDER_CONT;
	(void) env;
	(void) thiz;
	if (GameCfg.CMLevelMusicPlayOrder == play_order)
		return JNI_TRUE;
	GameCfg.CMLevelMusicPlayOrder = play_order;
	music_save_config();
	if (Game_wind)
		music_replay_current();
	return JNI_TRUE;
}

JNIEXPORT jint JNICALL
Java_com_dxxredux_app_MainActivity_nativeSetMusicVolume(
    JNIEnv *env, jobject thiz, jint volume)
{
	(void) env;
	(void) thiz;
	GameCfg.MusicVolume = (ubyte) music_clamp(volume, 0, 8);
	songs_set_volume(GameCfg.MusicVolume);
	music_save_config();
	return GameCfg.MusicVolume;
}

JNIEXPORT jboolean JNICALL
Java_com_dxxredux_app_MainActivity_nativeSetMusicPaused(
    JNIEnv *env, jobject thiz, jboolean paused)
{
	(void) env;
	(void) thiz;
	if (music_is_paused() != (paused ? 1 : 0))
		songs_pause_resume();
	return JNI_TRUE;
}

/* Returns JSON array of playable tracks for the track picker popup. */
JNIEXPORT jstring JNICALL
Java_com_dxxredux_app_MainActivity_nativeGetTrackList(
    JNIEnv *env, jobject thiz)
{
	char *buf = (char *) malloc(32768);
	jstring result;
	(void) thiz;
	if (!buf)
		return (*env)->NewStringUTF(env, "[]");
	songs_get_track_list(buf, 32768);
	result = (*env)->NewStringUTF(env, buf);
	free(buf);
	return result;
}
