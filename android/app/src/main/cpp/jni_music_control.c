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
#include <pthread.h>
#include <android/log.h>

#include "songs.h"
#include "songs_android_shared.h"
#include "rbaudio.h"
#include "config.h"
#include "digi.h"
#include "game.h"
#include "gameseq.h"
#include "playsave.h"
#include "android_crash_handler.h"
#include "android_log.h"
#include "android_music_control.h"
#include "jni_string.h"

#define TAG       "DXX-MusicCtrl"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)

extern int tsf_music_get_paused(void);
typedef enum android_music_command_type {
	MUSIC_COMMAND_SOURCE,
	MUSIC_COMMAND_NEXT,
	MUSIC_COMMAND_PREVIOUS,
	MUSIC_COMMAND_TRACK,
	MUSIC_COMMAND_PREFER_MISSION,
	MUSIC_COMMAND_PLAY_ORDER,
	MUSIC_COMMAND_VOLUME,
	MUSIC_COMMAND_PAUSE
} android_music_command_type;

typedef struct android_music_command {
	android_music_command_type type;
	int value;
	int auxiliary;
} android_music_command;

enum { MUSIC_COMMAND_CAPACITY = 64 };
static pthread_mutex_t g_music_control_mutex = PTHREAD_MUTEX_INITIALIZER;
static android_music_command g_music_commands[MUSIC_COMMAND_CAPACITY];
static unsigned int g_music_command_read;
static unsigned int g_music_command_count;

static int music_enqueue(android_music_command_type type, int value, int auxiliary)
{
	unsigned int slot;
	pthread_mutex_lock(&g_music_control_mutex);
	if (g_music_command_count == MUSIC_COMMAND_CAPACITY) {
		pthread_mutex_unlock(&g_music_control_mutex);
		return 0;
	}
	slot = (g_music_command_read + g_music_command_count) % MUSIC_COMMAND_CAPACITY;
	g_music_commands[slot].type = type;
	g_music_commands[slot].value = value;
	g_music_commands[slot].auxiliary = auxiliary;
	++g_music_command_count;
	pthread_mutex_unlock(&g_music_control_mutex);
	return 1;
}

static int music_dequeue(android_music_command *command)
{
	pthread_mutex_lock(&g_music_control_mutex);
	if (!g_music_command_count) {
		pthread_mutex_unlock(&g_music_control_mutex);
		return 0;
	}
	*command = g_music_commands[g_music_command_read];
	g_music_command_read = (g_music_command_read + 1) % MUSIC_COMMAND_CAPACITY;
	--g_music_command_count;
	pthread_mutex_unlock(&g_music_control_mutex);
	return 1;
}

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
		return RBAPeekPlayStatus() == -1;
	if (GameCfg.MusicType == MUSIC_TYPE_BUILTIN || GameCfg.MusicType == MUSIC_TYPE_CUSTOM)
		return tsf_music_get_paused();
	return 0;
}

static void music_replay_current(void)
{
	crash_breadcrumb_v("music replay current: type=%d prefer=%d game_wind=%d level=%d",
	                   GameCfg.MusicType,
	                   android_music_get_prefer_mission_soundtrack(),
	                   Game_wind ? 1 : 0,
	                   Current_level_num);
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

int android_music_control_apply_pending(void)
{
	android_music_command command;
	if (!music_dequeue(&command))
		return 0;
	do {
		switch (command.type) {
			case MUSIC_COMMAND_SOURCE:
				GameCfg.MusicType = command.value;
				android_music_set_prefer_mission_soundtrack(command.auxiliary);
				music_save_config();
				music_replay_current();
				break;
			case MUSIC_COMMAND_NEXT: songs_next_track(); break;
			case MUSIC_COMMAND_PREVIOUS: songs_prev_track(); break;
			case MUSIC_COMMAND_TRACK: songs_play_specific_track(command.value); break;
			case MUSIC_COMMAND_PREFER_MISSION:
				android_music_set_prefer_mission_soundtrack(command.value);
				break;
			case MUSIC_COMMAND_PLAY_ORDER:
				GameCfg.CMLevelMusicPlayOrder = command.value;
				music_save_config();
				break;
			case MUSIC_COMMAND_VOLUME:
				GameCfg.MusicVolume = (ubyte) command.value;
				songs_set_volume(GameCfg.MusicVolume);
				music_save_config();
				break;
			case MUSIC_COMMAND_PAUSE:
				if (command.value) songs_pause();
				else songs_resume();
				break;
		}
	} while (music_dequeue(&command));
	return 1;
}

JNIEXPORT jint JNICALL
Java_com_dxxredux_app_MainActivity_nativeNextTrack(JNIEnv *env, jobject thiz)
{
	return music_enqueue(MUSIC_COMMAND_NEXT, 0, 0);
}

JNIEXPORT jint JNICALL
Java_com_dxxredux_app_MainActivity_nativePrevTrack(JNIEnv *env, jobject thiz)
{
	return music_enqueue(MUSIC_COMMAND_PREVIOUS, 0, 0);
}

JNIEXPORT jint JNICALL
Java_com_dxxredux_app_MainActivity_nativePlaySpecificTrack(
    JNIEnv *env, jobject thiz, jint track)
{
	return music_enqueue(MUSIC_COMMAND_TRACK, track, 0);
}

JNIEXPORT jstring JNICALL
Java_com_dxxredux_app_MainActivity_nativeGetTrackName(
    JNIEnv *env, jobject thiz, jint track)
{
	const char *name = RBAGetTrackName(track);
	return dxx_jni_string_from_utf8(env, name ? name : "");
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
	return dxx_jni_string_from_utf8(env, buf);
}

/* Returns GameCfg.MusicType (0=none, 1=builtin, 2=redbook, 3=custom). */
JNIEXPORT jint JNICALL
Java_com_dxxredux_app_MainActivity_nativeGetMusicType(
    JNIEnv *env, jobject thiz)
{
	return GameCfg.MusicType;
}

static char *music_alloc_track_list(size_t *out_length)
{
	const int required = songs_get_track_list(NULL, 0);
	char *buffer;
	if (required < 0 || (unsigned int) required > SONGS_TRACK_LIST_MAX_BYTES)
		return NULL;
	buffer = (char *) malloc((size_t) required + 1);
	if (!buffer)
		return NULL;
	if (songs_get_track_list(buffer, (size_t) required + 1) != required) {
		free(buffer);
		return NULL;
	}
	if (out_length)
		*out_length = (size_t) required;
	return buffer;
}

JNIEXPORT jstring JNICALL
Java_com_dxxredux_app_MainActivity_nativeGetMusicOverlayState(
    JNIEnv *env, jobject thiz)
{
	int type = 0, track = -1, total = 0;
	char name[128] = "";
	char escaped_name[256];
	size_t tracks_length = 0;
	char *tracks = music_alloc_track_list(&tracks_length);
	const size_t buf_size = tracks_length + 1024;
	char *buf = tracks ? (char *) malloc(buf_size) : NULL;
	int written;
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
	written = snprintf(buf, buf_size,
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
	if (written < 0 || (size_t) written >= buf_size) {
		free(tracks);
		free(buf);
		return (*env)->NewStringUTF(env, "{}");
	}
	result = dxx_jni_string_from_utf8(env, buf);
	free(tracks);
	free(buf);
	return result;
}

JNIEXPORT jboolean JNICALL
Java_com_dxxredux_app_MainActivity_nativeIsMusicSourceChangePending(JNIEnv *env, jobject thiz)
{
	int pending;
	(void) env;
	(void) thiz;
	pthread_mutex_lock(&g_music_control_mutex);
	pending = g_music_command_count != 0;
	pthread_mutex_unlock(&g_music_control_mutex);
	return pending ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeSetMissionSoundtrackPreference(
    JNIEnv *env, jobject thiz, jboolean enabled)
{
	(void) env;
	(void) thiz;
	music_enqueue(MUSIC_COMMAND_PREFER_MISSION, enabled ? 1 : 0, 0);
}

JNIEXPORT jboolean JNICALL
Java_com_dxxredux_app_MainActivity_nativeSetMusicSource(
    JNIEnv *env, jobject thiz, jstring source)
{
	char *src;
	char src_copy[16];
	int new_type;
	int prefer_mission;
	(void) thiz;

	if (!source || !dxx_jni_string_to_utf8(env, source, &src))
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
		free(src);
		return JNI_FALSE;
	}
	snprintf(src_copy, sizeof(src_copy), "%s", src);
	free(src);

	if (!music_enqueue(MUSIC_COMMAND_SOURCE, new_type, prefer_mission))
		return JNI_FALSE;
	crash_breadcrumb_v("music source queued: source=%s type=%d prefer=%d", src_copy, new_type, prefer_mission);
	LOGI("music source queued: source=%s type=%d prefer=%d", src_copy, new_type, prefer_mission);
	debug_log(DLOG_GAME, "music source queued: source=%s type=%d prefer=%d", src_copy, new_type, prefer_mission);
	return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL
Java_com_dxxredux_app_MainActivity_nativeSetMusicOneTrackPerLevel(
    JNIEnv *env, jobject thiz, jboolean enabled)
{
	int play_order = enabled ? MUSIC_CM_PLAYORDER_LEVEL : MUSIC_CM_PLAYORDER_CONT;
	(void) env;
	(void) thiz;
	return music_enqueue(MUSIC_COMMAND_PLAY_ORDER, play_order, 0) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jint JNICALL
Java_com_dxxredux_app_MainActivity_nativeSetMusicVolume(
    JNIEnv *env, jobject thiz, jint volume)
{
	(void) env;
	(void) thiz;
	volume = music_clamp(volume, 0, 8);
	return music_enqueue(MUSIC_COMMAND_VOLUME, volume, 0) ? volume : -1;
}

JNIEXPORT jboolean JNICALL
Java_com_dxxredux_app_MainActivity_nativeSetMusicPaused(
    JNIEnv *env, jobject thiz, jboolean paused)
{
	(void) env;
	(void) thiz;
	return music_enqueue(MUSIC_COMMAND_PAUSE, paused ? 1 : 0, 0) ? JNI_TRUE : JNI_FALSE;
}

/* Returns JSON array of playable tracks for the track picker popup. */
JNIEXPORT jstring JNICALL
Java_com_dxxredux_app_MainActivity_nativeGetTrackList(
    JNIEnv *env, jobject thiz)
{
	char *buf = music_alloc_track_list(NULL);
	jstring result;
	(void) thiz;
	if (!buf)
		return (*env)->NewStringUTF(env, "[]");
	result = dxx_jni_string_from_utf8(env, buf);
	free(buf);
	return result;
}
