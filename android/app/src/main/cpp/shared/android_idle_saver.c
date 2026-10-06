/* Android screen saver policy runs on the engine thread; JNI only posts intent */
#include <jni.h>
#include <SDL.h>
#include "game.h"
#include "object.h"
#include "newdemo.h"
#include "android_idle_saver.h"
#include "android_lifecycle_diagnostics.h"
#include "android_lifecycle_actions.h"
#include "android_save_meta.h"
#include "state_android_shared.h"
#include "android_log.h"

extern void mix_background_pause(void);
extern void mix_background_resume(void);
extern void RBABackgroundPause(void);
extern void RBABackgroundResume(void);

static unsigned int g_activity_generation;
/* MainActivity enables the policy; preview and metadata engines leave it off */
static unsigned int g_timeout_ms;
static int g_state;
static int g_music_suspended;
static int g_audio_suspended;
static unsigned int g_seen_generation;
static unsigned int g_idle_since;
static int g_mode;
static unsigned int g_checkpoints;
#ifdef INTROSPECT_ON
static int g_fail_save_once;
JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeIdleSaverFailSaveOnce(JNIEnv *env, jobject thiz)
{
	(void) env;
	(void) thiz;
	__atomic_store_n(&g_fail_save_once, 1, __ATOMIC_RELEASE);
}
#endif

unsigned int android_idle_saver_checkpoints(void)
{
	return g_checkpoints;
}

int android_idle_saver_state(void)
{
	return __atomic_load_n(&g_state, __ATOMIC_ACQUIRE);
}

int android_idle_saver_hidden(void)
{
	int state = android_idle_saver_state();
	return state == ANDROID_IDLE_SLEEP || state == ANDROID_IDLE_MULTIPLAYER;
}

int android_idle_saver_music_suspended(void)
{
	return g_music_suspended;
}
int android_idle_saver_audio_suspended(void)
{
	return g_audio_suspended;
}

void android_idle_saver_foreground_audio(void)
{
	if (!g_music_suspended) {
		mix_background_resume();
		RBABackgroundResume();
	}
	if (!g_audio_suspended) androidaud_background_resume();
}

static void idle_publish(int state)
{
	if (state != android_idle_saver_state()) {
		debug_log(DLOG_DORMANCY, "idle screen saver state=%d", state);
		__atomic_store_n(&g_state, state, __ATOMIC_RELEASE);
	}
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeIdleSaverActivity(JNIEnv *env, jobject thiz)
{
	(void) env;
	(void) thiz;
	__atomic_add_fetch(&g_activity_generation, 1, __ATOMIC_RELEASE);
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeIdleSaverTimeout(JNIEnv *env, jobject thiz, jint milliseconds)
{
	(void) env;
	(void) thiz;
	__atomic_store_n(&g_timeout_ms, milliseconds > 0 ? (unsigned int) milliseconds : 0, __ATOMIC_RELEASE);
	__atomic_add_fetch(&g_activity_generation, 1, __ATOMIC_RELEASE);
}

JNIEXPORT jint JNICALL
Java_com_dxxredux_app_MainActivity_nativeIdleSaverState(JNIEnv *env, jobject thiz)
{
	(void) env;
	(void) thiz;
	return android_idle_saver_state();
}

void android_idle_saver_tick(int has_game_window, int multiplayer)
{
	unsigned int now = SDL_GetTicks();
	unsigned int generation = __atomic_load_n(&g_activity_generation, __ATOMIC_ACQUIRE);
	unsigned int timeout = __atomic_load_n(&g_timeout_ms, __ATOMIC_ACQUIRE);
	int foreground = android_lifecycle_diagnostics_requested_visibility() == ANDROID_LIFECYCLE_VISIBILITY_FOREGROUND;
	int paused = has_game_window && game_is_time_paused();
	int mode = has_game_window && Newdemo_state != ND_STATE_PLAYBACK
	               ? (multiplayer ? 2 : (paused ? 1 : 0))
	               : 0;
	int state = android_idle_saver_state();

	/* Keep a sleeping single-player session quiet after display wake until play resumes */
	if (g_music_suspended && (!has_game_window || !paused || multiplayer) &&
	    (state != ANDROID_IDLE_MULTIPLAYER || generation != g_seen_generation || mode != g_mode || !timeout)) {
		g_music_suspended = 0;
		g_audio_suspended = 0;
		if (foreground) android_idle_saver_foreground_audio();
	}
	if (!timeout || !mode || mode != g_mode || generation != g_seen_generation) {
		g_mode = mode;
		g_seen_generation = generation;
		g_idle_since = now;
		idle_publish(timeout && mode ? ANDROID_IDLE_COUNTING : ANDROID_IDLE_DISABLED);
		state = android_idle_saver_state();
	}
	if (!foreground) {
		if (!android_idle_saver_hidden()) g_idle_since = now;
		return;
	}
	if (!timeout || !mode || android_idle_saver_hidden() || state == ANDROID_IDLE_SAVE_FAILED)
		return;
	if ((unsigned int) (now - g_idle_since) < timeout) {
		if (timeout - (unsigned int) (now - g_idle_since) <= 10000)
			idle_publish(ANDROID_IDLE_WARNING);
		return;
	}
	if (mode == 1) {
		int result;
#ifdef INTROSPECT_ON
		if (__atomic_exchange_n(&g_fail_save_once, 0, __ATOMIC_ACQ_REL)) result = -1;
		else
#endif
			result = state_android_save_lifecycle_checkpoint(
			    ANDROID_SAVE_META_SLOT_AUTO_MINIMIZE, ANDROID_SAVE_DESC_AUTO_MINIMIZE,
			    ANDROID_SAVE_META_KIND_AUTO_MINIMIZE);
		/* Input during the write invalidates this sleep request */
		if (__atomic_load_n(&g_activity_generation, __ATOMIC_ACQUIRE) != generation)
			return;
		if (result <= 0) {
			debug_log(DLOG_DORMANCY, "idle screen saver checkpoint unavailable result=%d", result);
			idle_publish(ANDROID_IDLE_SAVE_FAILED);
			return;
		}
		++g_checkpoints;
		g_audio_suspended = 1;
		androidaud_background_pause();
	}
	g_music_suspended = 1;
	mix_background_pause();
	RBABackgroundPause();
	/* Also cover input arriving while the audio producers were quiescing */
	if (__atomic_load_n(&g_activity_generation, __ATOMIC_ACQUIRE) != generation)
		return;
	idle_publish(mode == 1 ? ANDROID_IDLE_SLEEP : ANDROID_IDLE_MULTIPLAYER);
}
