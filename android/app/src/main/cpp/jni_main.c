/*
 * jni_main.c — JNI entry point for the Android build of DXX-Redux.
 *
 * Provides a native method that Java/Kotlin can call to start the game engine.
 * For now this is a thin wrapper around the game's main().
 */

#include <jni.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <android/log.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <physfs.h>
#include <SDL.h>

#ifdef INTROSPECT_ON
#include "game_introspect.h"
#include "game_automate.h"
#include "console_ringbuf.h"
#include "debug_tex_overlay.h"
#endif
#include "pngfile.h"
#include "android_log.h"
#include "android_rewind.h"
#include "android_crash_handler.h"
#include "android_level_preview.h"

#define LOG_TAG   "DXX-Redux"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

/* ── Global JNI references (used by android_input.c for C→Java callbacks) ── */
JavaVM *g_jvm = NULL;
jobject g_activity = NULL; /* Global ref to MainActivity */

/* ── AAssetManager (used by digi_tsf_music.c to load the GM soundfont) ── */
AAssetManager *g_asset_manager = NULL;

/* Guard against double-launch: two startGame() calls in the same :game
 * process corrupt SDL/OpenGL/PhysFS globals and crash.  This happens when
 * the user presses Home (backgrounding the game) and then taps "Launch"
 * in the launcher, which starts a second MainActivity+thread in the same
 * process.  The static flag survives across Activity instances. */
static volatile int g_game_running = 0;

static char *android_consume_activity_string(JNIEnv *env, jobject activity, const char *method_name)
{
	jclass activity_class;
	jmethodID method;
	jstring value;
	const char *value_chars;
	char *copied;

	activity_class = (*env)->GetObjectClass(env, activity);
	if (!activity_class)
		return NULL;
	method = (*env)->GetMethodID(env, activity_class, method_name, "()Ljava/lang/String;");
	if (!method)
		return NULL;
	value = (jstring) (*env)->CallObjectMethod(env, activity, method);
	if (!value)
		return NULL;
	value_chars = (*env)->GetStringUTFChars(env, value, NULL);
	if (!value_chars) {
		(*env)->DeleteLocalRef(env, value);
		return NULL;
	}
	copied = strdup(value_chars);
	(*env)->ReleaseStringUTFChars(env, value, value_chars);
	(*env)->DeleteLocalRef(env, value);
	if (!copied || !copied[0]) {
		free(copied);
		return NULL;
	}
	return copied;
}

static const char *android_log_value(const char *value)
{
	return value ? value : "<null>";
}

static void android_log_startup_argv(const char *phase, int argc, char **argv,
                                     const char *input_demo_replay_path, const char *resume_save_path,
                                     const char *resume_callsign)
{
	int i;

	debug_log(DLOG_GAME,
	          "jni startup %s: argc=%d input_demo_replay=%s resume_save=%s resume_callsign=%s game_running=%d",
	          phase, argc, android_log_value(input_demo_replay_path),
	          android_log_value(resume_save_path), android_log_value(resume_callsign),
	          g_game_running);
	for (i = 0; i < argc; i++)
		debug_log(DLOG_GAME, "jni startup argv[%d]=%s", i,
		          i == 0 ? "<PHYSFS_AndroidInit>" : android_log_value(argv[i]));
}

/* Callable from Kotlin to check if main() is already executing. */
JNIEXPORT jboolean JNICALL
Java_com_dxxredux_app_MainActivity_nativeIsGameRunning(JNIEnv *env, jclass cls)
{
	return (jboolean) g_game_running;
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved)
{
	g_jvm = vm;
	LOGI("JNI_OnLoad: JavaVM cached");
	return JNI_VERSION_1_6;
}

/* The game's real entry point (d2/main/inferno.c) */
extern int main(int argc, char *argv[]);

JNIEXPORT jstring JNICALL
Java_com_dxxredux_app_MainActivity_helloFromNative(JNIEnv *env, jobject thiz)
{
	LOGI("D2X-Redux native library loaded (full game build).");
	return (*env)->NewStringUTF(env, "D2X-Redux engine loaded!");
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_startGame(JNIEnv *env, jobject thiz)
{
	/* Guard: if main() is already running in another thread, do nothing.
	 * This prevents the double-launch SIGSEGV that occurs when a second
	 * Activity starts in the :game process while the first is still alive
	 * (e.g. user presses Home then taps "Launch" in the launcher). */
	if (g_game_running) {
		LOGE("startGame() called while game already running -- aborting to avoid crash");
		debug_log(DLOG_GAME, "jni startup rejected: startGame called while native game is already running");
		/* Finish this redundant Activity so the user sees the running game. */
		jclass cls = (*env)->GetObjectClass(env, thiz);
		jmethodID mid = (*env)->GetMethodID(env, cls, "finish", "()V");
		if (mid)
			(*env)->CallVoidMethod(env, thiz, mid);
		return;
	}
	g_game_running = 1;

	LOGI("Starting D2X-Redux game engine...");

	/* Cache a global reference to the Activity for C→Java callbacks. */
	g_activity = (*env)->NewGlobalRef(env, thiz);

	/* Cache AAssetManager for native asset access (soundfont loading etc.) */
	{
		jclass actCls = (*env)->GetObjectClass(env, thiz);
		jmethodID getAssets = (*env)->GetMethodID(env, actCls, "getAssets",
		                                          "()Landroid/content/res/AssetManager;");
		jobject jAssetMgr = (*env)->CallObjectMethod(env, thiz, getAssets);
		g_asset_manager = AAssetManager_fromJava(env, jAssetMgr);
		LOGI("AAssetManager cached for native asset access");
	}

	/* Query native audio sample rate and buffer size from AudioManager.
	 * This lets OpenSL ES avoid AudioFlinger resampling. */
	{
		extern int g_android_native_sample_rate;
		extern int g_android_native_buffer_frames;

		jclass actCls = (*env)->GetObjectClass(env, thiz);
		jmethodID getSysSvc = (*env)->GetMethodID(env, actCls,
		                                          "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
		jstring audioStr = (*env)->NewStringUTF(env, "audio");
		jobject audioMgr = (*env)->CallObjectMethod(env, thiz, getSysSvc, audioStr);
		(*env)->DeleteLocalRef(env, audioStr);

		if (audioMgr) {
			jclass amCls = (*env)->GetObjectClass(env, audioMgr);
			jmethodID getProp = (*env)->GetMethodID(env, amCls,
			                                        "getProperty", "(Ljava/lang/String;)Ljava/lang/String;");

			/* PROPERTY_OUTPUT_SAMPLE_RATE */
			jstring keyRate = (*env)->NewStringUTF(env,
			                                       "android.media.property.OUTPUT_SAMPLE_RATE");
			jstring valRate = (*env)->CallObjectMethod(env, audioMgr, getProp, keyRate);
			if (valRate) {
				const char *s = (*env)->GetStringUTFChars(env, valRate, NULL);
				g_android_native_sample_rate = atoi(s);
				(*env)->ReleaseStringUTFChars(env, valRate, s);
				(*env)->DeleteLocalRef(env, valRate);
			}
			(*env)->DeleteLocalRef(env, keyRate);

			/* PROPERTY_OUTPUT_FRAMES_PER_BUFFER */
			jstring keyBuf = (*env)->NewStringUTF(env,
			                                      "android.media.property.OUTPUT_FRAMES_PER_BUFFER");
			jstring valBuf = (*env)->CallObjectMethod(env, audioMgr, getProp, keyBuf);
			if (valBuf) {
				const char *s = (*env)->GetStringUTFChars(env, valBuf, NULL);
				g_android_native_buffer_frames = atoi(s);
				(*env)->ReleaseStringUTFChars(env, valBuf, s);
				(*env)->DeleteLocalRef(env, valBuf);
			}
			(*env)->DeleteLocalRef(env, keyBuf);
			(*env)->DeleteLocalRef(env, audioMgr);
		}
		LOGI("Native audio: rate=%d buf=%d",
		     g_android_native_sample_rate, g_android_native_buffer_frames);
	}

	/* Tell SDL 1.2 to use the dummy video driver.
	 * The dummy video driver's Available() only returns 1 when this is set.
	 * Audio uses our custom Android OpenSL ES driver (no env var needed). */
	setenv("SDL_VIDEODRIVER", "dummy", 1);

	/*
	 * PhysFS on Android expects argv[0] to be a pointer to a
	 * PHYSFS_AndroidInit struct (cast to char*).  It uses the JNIEnv
	 * and Android Context to resolve the APK path and internal storage dir.
	 * PhysFS does NOT hold references past PHYSFS_init(), so locals are fine.
	 */
	PHYSFS_AndroidInit androidInit;
	char *input_demo_replay_path = NULL;
	char *resume_save_path = NULL;
	char *resume_callsign = NULL;
	char *pilot_callsign = NULL;
	char *argv_startup[] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
	int argc = 1;
	androidInit.jnienv = (void *) env;
	androidInit.context = (void *) thiz; /* Activity is a valid Context */

	argv_startup[0] = (char *) &androidInit;
	pilot_callsign = android_consume_activity_string(env, thiz, "consumePilotCallsign");
	input_demo_replay_path = android_consume_activity_string(env, thiz, "consumeInputDemoReplayPath");
	if (input_demo_replay_path) {
		LOGI("Launching input demo replay: %s", input_demo_replay_path);
		argv_startup[argc++] = "-inputdemo-replay";
		argv_startup[argc++] = input_demo_replay_path;
	} else {
		const char *startup_pilot = pilot_callsign;

		resume_save_path = android_consume_activity_string(env, thiz, "consumeResumeSavePath");
		resume_callsign = android_consume_activity_string(env, thiz, "consumeResumeCallsign");
		if (resume_save_path && resume_save_path[0]) {
			if (resume_callsign && resume_callsign[0]) {
				LOGI("Launching startup resume for pilot '%s': %s",
				     resume_callsign, resume_save_path);
				startup_pilot = resume_callsign;
			} else {
				LOGI("Launching startup resume with save-derived pilot: %s", resume_save_path);
			}
		}
		if (startup_pilot && startup_pilot[0]) {
			LOGI("Launching startup pilot: %s", startup_pilot);
			argv_startup[argc++] = "-pilot";
			argv_startup[argc++] = (char *) startup_pilot;
		}
		if (resume_save_path && resume_save_path[0]) {
			argv_startup[argc++] = "-resume-save";
			argv_startup[argc++] = resume_save_path;
		}
	}
	android_log_startup_argv("before-main", argc, argv_startup,
	                         input_demo_replay_path, resume_save_path, resume_callsign);
	main(argc, argv_startup);
	debug_log(DLOG_GAME, "jni startup main returned");
	free(input_demo_replay_path);
	free(resume_save_path);
	free(resume_callsign);
	free(pilot_callsign);

	g_game_running = 0;

	/* Engine has exited (user quit or fatal error).
	 * Call Activity.finish() so the activity closes instead of freezing
	 * on the last rendered frame. */
	LOGI("Game engine exited, finishing activity...");
	jclass cls = (*env)->GetObjectClass(env, g_activity);
	jmethodID mid = (*env)->GetMethodID(env, cls, "finish", "()V");
	if (mid) {
		(*env)->CallVoidMethod(env, g_activity, mid);
	}

	(*env)->DeleteGlobalRef(env, g_activity);
	g_activity = NULL;

	/*
	 * Kill the process so the next launch starts with clean native state.
	 * SDL, PhysFS, and engine statics all assume a single init/deinit
	 * cycle per process; re-entering main() without a process restart
	 * causes "Audio device is already opened" / graphics init failures.
	 *
	 * The activity manager preserves the task stack, so SetupActivity
	 * will be recreated in a fresh process automatically.
	 */
	LOGI("Killing process for clean restart...");
	jclass processCls = (*env)->FindClass(env, "android/os/Process");
	jmethodID myPid = (*env)->GetStaticMethodID(env, processCls, "myPid", "()I");
	jmethodID killProc = (*env)->GetStaticMethodID(env, processCls, "killProcess", "(I)V");
	jint pid = (*env)->CallStaticIntMethod(env, processCls, myPid);
	(*env)->CallStaticVoidMethod(env, processCls, killProc, pid);
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_LevelPreviewActivity_startLevelPreview(
    JNIEnv *env, jobject thiz, jstring jrequest_path, jstring jdata_dir)
{
	const char *request_path = NULL;
	const char *data_dir = NULL;
	PHYSFS_AndroidInit android_init;
	char *argv_preview[7];
	int result;
	const char *error;

	if (g_game_running || !jrequest_path || !jdata_dir)
		return;
	g_game_running = 1;
	g_activity = (*env)->NewGlobalRef(env, thiz);
	request_path = (*env)->GetStringUTFChars(env, jrequest_path, NULL);
	if (request_path)
		data_dir = (*env)->GetStringUTFChars(env, jdata_dir, NULL);
	if (!request_path || !data_dir) {
		if (request_path)
			(*env)->ReleaseStringUTFChars(env, jrequest_path, request_path);
		if (data_dir)
			(*env)->ReleaseStringUTFChars(env, jdata_dir, data_dir);
		if (g_activity) {
			(*env)->DeleteGlobalRef(env, g_activity);
			g_activity = NULL;
		}
		g_game_running = 0;
		return;
	}
	setenv("SDL_VIDEODRIVER", "dummy", 1);
	setenv("DXX_ANDROID_LEVEL_PREVIEW_DATA_DIR", data_dir, 1);
	android_init.jnienv = (void *) env;
	android_init.context = (void *) thiz;
	argv_preview[0] = (char *) &android_init;
	argv_preview[1] = "-hogdir";
	argv_preview[2] = (char *) data_dir;
	argv_preview[3] = "-nosound";
	argv_preview[4] = "-nomusic";
	argv_preview[5] = "-level-preview-request";
	argv_preview[6] = (char *) request_path;
	debug_log(DLOG_GAME, "level preview starting request=%s data=%s", request_path, data_dir);
	result = main(7, argv_preview);
	error = android_level_preview_last_error();
	if (result && (!error || !error[0]))
		error = "Native preview exited with an error";
	{
		jclass cls = (*env)->GetObjectClass(env, thiz);
		jmethodID finished = (*env)->GetMethodID(
		    env, cls, "onNativePreviewFinished", "(Ljava/lang/String;)V");
		jstring jerror = (*env)->NewStringUTF(env, error ? error : "");
		if (finished)
			(*env)->CallVoidMethod(env, thiz, finished, jerror);
		(*env)->DeleteLocalRef(env, jerror);
	}
	(*env)->ReleaseStringUTFChars(env, jrequest_path, request_path);
	(*env)->ReleaseStringUTFChars(env, jdata_dir, data_dir);
	unsetenv("DXX_ANDROID_LEVEL_PREVIEW_DATA_DIR");
	g_game_running = 0;
	if (g_activity) {
		(*env)->DeleteGlobalRef(env, g_activity);
		g_activity = NULL;
	}
	{
		jclass process_cls = (*env)->FindClass(env, "android/os/Process");
		jmethodID my_pid = (*env)->GetStaticMethodID(env, process_cls, "myPid", "()I");
		jmethodID kill_process = (*env)->GetStaticMethodID(env, process_cls, "killProcess", "(I)V");
		jint pid = (*env)->CallStaticIntMethod(env, process_cls, my_pid);
		(*env)->CallStaticVoidMethod(env, process_cls, kill_process, pid);
	}
}

/*
 * android_finish_and_exit -- clean fatal exit for Error() calls.
 *
 * Error() normally calls exit(1), but on Android that kills the process
 * without finishing the Activity, leaving a frozen splash screen.
 * This function attaches to the JVM, calls Activity.finish(), then
 * _exit(1) to skip atexit handlers (which can hang in SDL cleanup).
 */
void android_finish_and_exit(void)
{
	LOGE("android_finish_and_exit: fatal error, cleaning up");
	g_game_running = 0;

	if (g_jvm && g_activity) {
		JNIEnv *env = NULL;
		int attached = 0;
		if ((*g_jvm)->GetEnv(g_jvm, (void **) &env, JNI_VERSION_1_6) != JNI_OK) {
			if ((*g_jvm)->AttachCurrentThread(g_jvm, &env, NULL) == JNI_OK) {
				attached = 1;
			}
		}
		if (env) {
			jclass cls = (*env)->GetObjectClass(env, g_activity);
			jmethodID mid = (*env)->GetMethodID(env, cls, "finish", "()V");
			if (mid)
				(*env)->CallVoidMethod(env, g_activity, mid);

			jclass processCls = (*env)->FindClass(env, "android/os/Process");
			jmethodID myPid = (*env)->GetStaticMethodID(env, processCls, "myPid", "()I");
			jmethodID killProc = (*env)->GetStaticMethodID(env, processCls, "killProcess", "(I)V");
			jint pid = (*env)->CallStaticIntMethod(env, processCls, myPid);
			(*env)->CallStaticVoidMethod(env, processCls, killProc, pid);

			if (attached)
				(*g_jvm)->DetachCurrentThread(g_jvm);
		}
	}
	/* Fallback if JNI cleanup didn't kill the process */
	_exit(1);
}

JNIEXPORT jint JNICALL
Java_com_dxxredux_app_MainActivity_nativeGetGameWidth(JNIEnv *env, jobject thiz)
{
	extern unsigned int grd_curscreen_w(void);
	return (jint) grd_curscreen_w();
}

JNIEXPORT jint JNICALL
Java_com_dxxredux_app_MainActivity_nativeGetGameHeight(JNIEnv *env, jobject thiz)
{
	extern unsigned int grd_curscreen_h(void);
	return (jint) grd_curscreen_h();
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeQuit(JNIEnv *env, jobject thiz)
{
	LOGI("nativeQuit: pushing SDL_QUIT event");
	SDL_Event ev;
	ev.type = SDL_QUIT;
	SDL_PushEvent(&ev);
}

/* ── Weapon state query for touch overlay weapon wheels ──────────
 * Returns an int array:
 *   [0]     = primary_weapon_flags
 *   [1]     = secondary_weapon_flags
 *   [2]     = player flags (for ammo rack detection)
 *   [3..12] = primary_ammo[0..9]
 *   [13..22]= secondary_ammo[0..9]
 *   [23..32]= effective primary_ammo_max[0..9]  (doubled if ammo rack, D2 only)
 *   [33..42]= effective secondary_ammo_max[0..9] (doubled if ammo rack, D2 only)
 *
 * D1 has only 5 primary/secondary weapons; slots 5..9 are zeroed.
 */
#include "player.h"
#include "weapon.h"
#include "game.h"
#ifdef DXX_BUILD_DESCENT_II
extern int MarkerObject[];
#endif

#ifdef DXX_BUILD_DESCENT_II
extern ubyte Primary_last_was_super[MAX_PRIMARY_WEAPONS];
extern ubyte Secondary_last_was_super[MAX_SECONDARY_WEAPONS];
#endif

/* Shared constant: PLAYER_FLAGS_AMMO_RACK = 128 (duplicated in WeaponState.kt, D2 only) */
/* Array layout: [0]=priFlags, [1]=secFlags, [2]=playerFlags,
 *   [3..12]=priAmmo, [13..22]=secAmmo, [23..32]=priMax, [33..42]=secMax,
 *   [43]=currentPrimary, [44]=currentSecondary, [45]=currentBomb,
 *   [46]=laserLevel, [47..51]=primaryLastWasSuper[0..4],
 *   [52..56]=secondaryLastWasSuper[0..4], [57]=energy, [58]=afterburnerChargePct
 * Indices 43-58 duplicated in WeaponState.kt */

JNIEXPORT jintArray JNICALL
Java_com_dxxredux_app_MainActivity_nativeGetWeaponState(JNIEnv *env, jobject thiz)
{
	extern int Player_num;

	enum { WS_PRIMARY_PAIR_SLOTS = 5,
		   WS_SECONDARY_PAIR_SLOTS = 5,
		   WS_SIZE = 59 };
	jint buf[WS_SIZE];
	memset(buf, 0, sizeof(buf));

	buf[0] = (jint) Players[Player_num].primary_weapon_flags;
	buf[1] = (jint) Players[Player_num].secondary_weapon_flags;
	buf[2] = (jint) Players[Player_num].flags;

#ifdef DXX_BUILD_DESCENT_II
	int has_rack = (Players[Player_num].flags & PLAYER_FLAGS_AMMO_RACK) ? 1 : 0;
#else
	int has_rack = 0;
#endif
	int rack_mult = has_rack ? 2 : 1;

	int i;
	for (i = 0; i < MAX_PRIMARY_WEAPONS; i++) {
		buf[3 + i] = (jint) Players[Player_num].primary_ammo[i];
		buf[23 + i] = (jint) (Primary_ammo_max[i] * rack_mult);
	}
	for (i = 0; i < MAX_SECONDARY_WEAPONS; i++) {
		buf[13 + i] = (jint) Players[Player_num].secondary_ammo[i];
		buf[33 + i] = (jint) (Secondary_ammo_max[i] * rack_mult);
	}

	buf[43] = (jint) Players[Player_num].primary_weapon;
	buf[44] = (jint) Players[Player_num].secondary_weapon;
	buf[45] = (jint) which_bomb();
	buf[46] = (jint) Players[Player_num].laser_level;
#ifdef DXX_BUILD_DESCENT_II
	for (i = 0; i < WS_PRIMARY_PAIR_SLOTS && i < MAX_PRIMARY_WEAPONS; i++)
		buf[47 + i] = (jint) Primary_last_was_super[i];
	for (i = 0; i < WS_SECONDARY_PAIR_SLOTS && i < MAX_SECONDARY_WEAPONS; i++)
		buf[47 + WS_PRIMARY_PAIR_SLOTS + i] = (jint) Secondary_last_was_super[i];
#endif
	buf[57] = (jint) (Players[Player_num].energy / F1_0);
#ifdef DXX_BUILD_DESCENT_II
	buf[58] = (jint) ((Players[Player_num].afterburner_charge * 100) / F1_0);
#endif

	jintArray result = (*env)->NewIntArray(env, WS_SIZE);
	if (result)
		(*env)->SetIntArrayRegion(env, result, 0, WS_SIZE, buf);
	return result;
}

/* Shared constants duplicated in AutomapTouchPolicy.kt:
 * -1=unavailable marker slot, 0=free marker slot, 1=placed marker slot.
 * D2_MARKER_OBJECT_COUNT duplicates NUM_MARKERS from d2/main/automap.h.
 */
JNIEXPORT jintArray JNICALL
Java_com_dxxredux_app_MainActivity_nativeGetAutomapMarkerState(JNIEnv *env, jobject thiz)
{
	enum { MARKER_STATE_SIZE = 9,
		   D2_MARKER_OBJECT_COUNT = 16,
		   MARKER_SLOT_UNAVAILABLE = -1,
		   MARKER_SLOT_FREE = 0,
		   MARKER_SLOT_PLACED = 1 };
	jint buf[MARKER_STATE_SIZE];
	int i;

	for (i = 0; i < MARKER_STATE_SIZE; i++)
		buf[i] = MARKER_SLOT_UNAVAILABLE;

#ifdef DXX_BUILD_DESCENT_II
	{
		extern int Game_mode;
		extern int Player_num;
		int max_slots = (Game_mode & GM_MULTI) ? 2 : MARKER_STATE_SIZE;
		for (i = 0; i < max_slots; i++) {
			int marker_num = (Player_num * 2) + i;
			if (marker_num >= 0 && marker_num < D2_MARKER_OBJECT_COUNT)
				buf[i] = (MarkerObject[marker_num] == -1) ? MARKER_SLOT_FREE : MARKER_SLOT_PLACED;
		}
	}
#endif

	jintArray result = (*env)->NewIntArray(env, MARKER_STATE_SIZE);
	if (result)
		(*env)->SetIntArrayRegion(env, result, 0, MARKER_STATE_SIZE, buf);
	return result;
}

#ifdef INTROSPECT_ON
/* ── Introspection: return current game state as a JSON string ────── */
JNIEXPORT jstring JNICALL
Java_com_dxxredux_app_MainActivity_nativeGetGameState(JNIEnv *env, jobject thiz)
{
	char *json = game_introspect_get_state();
	if (!json) {
		return (*env)->NewStringUTF(env, "{\"error\": \"introspection not enabled\"}");
	}
	jstring result = (*env)->NewStringUTF(env, json);
	free(json);
	return result;
}

/* ── Introspection: request a dump to the pre-configured file path ── */
JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeRequestIntrospect(JNIEnv *env, jobject thiz)
{
	game_introspect_request();
}

/* ── Introspection: set the file path for dumps ──────────────────── */
JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeSetIntrospectPath(JNIEnv *env, jobject thiz, jstring jpath)
{
	const char *path = (*env)->GetStringUTFChars(env, jpath, NULL);
	game_introspect_set_path(path);
	(*env)->ReleaseStringUTFChars(env, jpath, path);
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_LevelPreviewActivity_nativeRequestIntrospect(JNIEnv *env, jobject thiz)
{
	(void) env;
	(void) thiz;
	game_introspect_request();
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_LevelPreviewActivity_nativeRequestClose(JNIEnv *env, jobject thiz)
{
	(void) env;
	(void) thiz;
	android_level_preview_request_close();
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_LevelPreviewActivity_nativeSetIntrospectPath(
    JNIEnv *env, jobject thiz, jstring jpath)
{
	const char *path;
	(void) thiz;
	if (!jpath)
		return;
	path = (*env)->GetStringUTFChars(env, jpath, NULL);
	if (path) {
		game_introspect_set_path(path);
		(*env)->ReleaseStringUTFChars(env, jpath, path);
	}
}

#ifndef DXX_BUILD_DESCENT_II
JNIEXPORT void JNICALL
Java_com_dxxredux_app_NativeTextureLookupCache_nativeClearD1(JNIEnv *env, jclass clazz)
{
	(void) env;
	(void) clazz;
	clear_texture_lookup_cache();
}
#endif

#ifdef DXX_BUILD_DESCENT_II
JNIEXPORT void JNICALL
Java_com_dxxredux_app_NativeTextureLookupCache_nativeClearD2(JNIEnv *env, jclass clazz)
{
	(void) env;
	(void) clazz;
	clear_texture_lookup_cache();
}
#endif

/* ── Automation: load and run a JSON script of input steps ────────── */
JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeLoadAutomationScript(JNIEnv *env, jobject thiz, jstring jpath,
                                                              jint start_step, jstring jrun_id)
{
	const char *path = (*env)->GetStringUTFChars(env, jpath, NULL);
	const char *run_id = (*env)->GetStringUTFChars(env, jrun_id, NULL);
	game_automate_load_script(path, (int) start_step, run_id);
	(*env)->ReleaseStringUTFChars(env, jrun_id, run_id);
	(*env)->ReleaseStringUTFChars(env, jpath, path);
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeSetAutomationPath(JNIEnv *env, jobject thiz, jstring jpath)
{
	const char *path = (*env)->GetStringUTFChars(env, jpath, NULL);
	game_automate_set_path(path);
	(*env)->ReleaseStringUTFChars(env, jpath, path);
}

/* ── Audio tuning: adjust TSF global gain in dB ──────────────────── */
extern void tsf_music_set_gain_db(float db);
extern void tsf_music_set_max_voices(int n);

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeSetMusicGain(JNIEnv *env, jobject thiz, jfloat gain_db)
{
	tsf_music_set_gain_db((float) gain_db);
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeSetMusicVoices(JNIEnv *env, jobject thiz, jint max_voices)
{
	tsf_music_set_max_voices((int) max_voices);
}

/* -- Console ring buffer: return recent con_printf output as JSON ----- */
JNIEXPORT jstring JNICALL
Java_com_dxxredux_app_MainActivity_nativeGetConsoleSince(JNIEnv *env, jobject thiz, jlong since_seq)
{
	char *json = console_ringbuf_get_json((uint64_t) since_seq, 200);
	if (!json) {
		return (*env)->NewStringUTF(env, "{\"next_seq\":0,\"lines\":[]}");
	}
	jstring result = (*env)->NewStringUTF(env, json);
	free(json);
	return result;
}

/* ── Debug flags: toggle debug overlays from adb/Kotlin ────────── */
extern volatile int gles3_shim_debug_mode;

static const char *merged_wall_debug_mode_name(int mode)
{
	switch (mode) {
		case MERGED_WALL_DEBUG_OVERLAY_ALPHA:
			return "alpha";
		case MERGED_WALL_DEBUG_OVERLAY_RGB:
			return "rgb";
		default:
			return "off";
	}
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeSetDebugFlag(JNIEnv *env, jobject thiz,
                                                      jstring jname, jint value)
{
	const char *name = (*env)->GetStringUTFChars(env, jname, NULL);
	if (strcmp(name, "tex_overlay") == 0)
		g_debug_tex_overlay_active = (int) value;
	else if (strcmp(name, "merged_wall_mode") == 0) {
		int clamped = (int) value;
		int old = (int) g_merged_wall_debug_mode;

		if (clamped < MERGED_WALL_DEBUG_NONE)
			clamped = MERGED_WALL_DEBUG_NONE;
		if (clamped > MERGED_WALL_DEBUG_OVERLAY_RGB)
			clamped = MERGED_WALL_DEBUG_OVERLAY_RGB;
		LOGI("debug flag: merged_wall_mode %d(%s) -> %d(%s)",
		     old, merged_wall_debug_mode_name(old),
		     clamped, merged_wall_debug_mode_name(clamped));
		debug_log(DLOG_TEXTURE,
		          "[mwall_mode] toggle: old=%d(%s) new=%d(%s)",
		          old, merged_wall_debug_mode_name(old),
		          clamped, merged_wall_debug_mode_name(clamped));
		g_merged_wall_debug_mode = clamped;
	} else if (strcmp(name, "merged_wall_snapshot") == 0) {
		if (value) {
			android_merged_wall_request_snapshot(
			    value == MERGED_WALL_REQUEST_PROBE ? MERGED_WALL_REQUEST_PROBE
			                                       : MERGED_WALL_REQUEST_SNAPSHOT);
			LOGI("debug flag: merged_wall_snapshot requested at frame=%d",
			     (int) g_merged_wall_snapshot_request_frame);
		}
	} else if (strcmp(name, "gfx_mode") == 0)
		gles3_shim_debug_mode = (int) value;
	else if (strcmp(name, "aniso_level") == 0) {
		extern int ogl_aniso_level;
		extern volatile int g_aniso_pending_apply;
		ogl_aniso_level = (int) value;
		g_aniso_pending_apply = 1;
	} else if (strcmp(name, "msaa_level") == 0) {
		extern int ogl_msaa_samples;
		extern volatile int g_msaa_pending_apply;
		ogl_msaa_samples = (int) value;
		g_msaa_pending_apply = 1;
	} else
		LOGE("nativeSetDebugFlag: unknown flag '%s'", name);
	(*env)->ReleaseStringUTFChars(env, jname, name);
}
#endif /* INTROSPECT_ON */

/* ── Graphics options: set MSAA/AF from Kotlin (all builds) ────── */
#include "config.h"
#include "palette.h"
#include "shared/android_graphics_options.h"
#include "shared/coop_indicator_lines.h"
JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeSetGraphicsOption(JNIEnv *env, jobject thiz,
                                                           jstring jname, jint value)
{
	const char *name = (*env)->GetStringUTFChars(env, jname, NULL);
	int persist = strcmp(name, "alpha_effects") &&
	              strcmp(name, "dynlight_color") &&
	              strcmp(name, "main_view_fov_locked");
	LOGI("graphics option: %s=%d", name, (int) value);
	if (!android_graphics_set_option(name, (int) value, persist))
		LOGE("nativeSetGraphicsOption: unknown option '%s'", name);
	(*env)->ReleaseStringUTFChars(env, jname, name);
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeSetRoundedCornerTextInsets(JNIEnv *env, jobject thiz,
                                                                    jint surface_width,
                                                                    jint surface_height,
                                                                    jint top_left_px,
                                                                    jint bottom_left_px,
                                                                    jint top_right_px,
                                                                    jint bottom_right_px)
{
	(void) env;
	(void) thiz;
	android_graphics_set_rounded_corner_text_insets((int) surface_width,
	                                                (int) surface_height,
	                                                (int) top_left_px,
	                                                (int) bottom_left_px,
	                                                (int) top_right_px,
	                                                (int) bottom_right_px);
}

JNIEXPORT jint JNICALL
Java_com_dxxredux_app_MainActivity_nativeGetGammaLevel(JNIEnv *env, jobject thiz)
{
	(void) env;
	(void) thiz;
	return (jint) gr_palette_get_gamma();
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeSetCoopIndicatorOptions(JNIEnv *env,
                                                                 jobject thiz,
                                                                 jboolean showNearestPlayerLine,
                                                                 jboolean showGuidebotLine)
{
	LOGI("coop indicator options: nearest=%d guidebot=%d",
	     showNearestPlayerLine ? 1 : 0,
	     showGuidebotLine ? 1 : 0);
	coop_indicator_lines_set_options(showNearestPlayerLine ? 1 : 0,
	                                 showGuidebotLine ? 1 : 0);
}

/* ── Debug logging: per-category enable/disable from Kotlin ────── */
JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeSetDebugLogEnabled(JNIEnv *env, jobject thiz,
                                                            jint category, jboolean on)
{
	debug_log_set_enabled((int) category, on ? 1 : 0);
}

/* ── Auto-net: set up automatic join/host from the matchmaking lobby ── */
extern int auto_join_pending;
extern char auto_join_host_addr[];
extern int auto_join_host_port;
extern int auto_join_my_port;

extern int auto_host_pending;
extern int auto_host_my_port;
extern char auto_host_mission[];
extern int auto_host_mode;
extern int auto_host_max_players;
extern int auto_host_level_num;
extern int auto_host_difficulty;
extern int auto_host_coop_qol;
extern int auto_host_full_death_spew;
extern int auto_host_player_spew_no_expire;
extern int auto_host_clients_can_request_rewind;
extern int auto_host_observer;
extern char auto_net_callsign[];
extern char auto_net_client_id[];

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeSetCallsign(JNIEnv *env, jobject thiz,
                                                     jstring jCallsign)
{
	const char *cs = (*env)->GetStringUTFChars(env, jCallsign, NULL);
	strncpy(auto_net_callsign, cs, 9);
	auto_net_callsign[9] = '\0';
	(*env)->ReleaseStringUTFChars(env, jCallsign, cs);
	LOGI("nativeSetCallsign: %s", auto_net_callsign);
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeSetClientId(JNIEnv *env, jobject thiz,
                                                     jstring jClientId)
{
	const char *cid = (*env)->GetStringUTFChars(env, jClientId, NULL);
	strncpy(auto_net_client_id, cid, 36);
	auto_net_client_id[36] = '\0';
	(*env)->ReleaseStringUTFChars(env, jClientId, cid);
	LOGI("nativeSetClientId: %s", auto_net_client_id);
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeSetAutoJoin(JNIEnv *env, jobject thiz,
                                                     jstring jHostAddr, jint hostPort, jint myPort)
{
	const char *addr = (*env)->GetStringUTFChars(env, jHostAddr, NULL);
	strncpy(auto_join_host_addr, addr, 127);
	auto_join_host_addr[127] = '\0';
	(*env)->ReleaseStringUTFChars(env, jHostAddr, addr);

	auto_join_host_port = (int) hostPort;
	auto_join_my_port = (int) myPort;
	android_rewind_set_clients_can_request(0);
	auto_join_pending = 1;
	LOGI("nativeSetAutoJoin: %s:%d (my port %d)", auto_join_host_addr, auto_join_host_port, auto_join_my_port);
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeSetAutoHost(JNIEnv *env, jobject thiz,
                                                     jint myPort, jstring jMission, jint mode,
                                                     jint maxPlayers, jint levelNum, jint difficulty,
                                                     jboolean coopQol, jboolean fullDeathSpew,
                                                     jboolean playerSpewNoExpire,
                                                     jboolean clientsCanRequestRewind,
                                                     jboolean hostObserver)
{
	const char *mission = (*env)->GetStringUTFChars(env, jMission, NULL);
	strncpy(auto_host_mission, mission, 63);
	auto_host_mission[63] = '\0';
	(*env)->ReleaseStringUTFChars(env, jMission, mission);

	auto_host_my_port = (int) myPort;
	auto_host_mode = (int) mode;
	auto_host_max_players = (int) maxPlayers;
	auto_host_level_num = (int) levelNum;
	auto_host_difficulty = (int) difficulty;
	auto_host_coop_qol = coopQol ? 1 : 0;
	auto_host_full_death_spew = fullDeathSpew ? 1 : 0;
	auto_host_player_spew_no_expire = playerSpewNoExpire ? 1 : 0;
	auto_host_clients_can_request_rewind = clientsCanRequestRewind ? 1 : 0;
	auto_host_observer = hostObserver ? 1 : 0;
	android_rewind_set_clients_can_request(auto_host_clients_can_request_rewind);
	auto_host_pending = 1;
	debug_log(DLOG_COOP_DESYNC,
	          "[COOP] nativeSetAutoHost: port=%d mission=%s mode=%d max=%d level=%d diff=%d coop_qol=%d client_rewind=%d host_observer=%d",
	          auto_host_my_port, auto_host_mission, auto_host_mode,
	          auto_host_max_players, auto_host_level_num,
	          auto_host_difficulty, auto_host_coop_qol,
	          auto_host_clients_can_request_rewind,
	          auto_host_observer);
	LOGI("nativeSetAutoHost: port=%d mission=%s mode=%d max=%d lvl=%d diff=%d coop_qol=%d full_death_spew=%d player_spew_no_expire=%d client_rewind=%d host_observer=%d",
	     auto_host_my_port, auto_host_mission, auto_host_mode,
	     auto_host_max_players, auto_host_level_num, auto_host_difficulty,
	     auto_host_coop_qol, auto_host_full_death_spew,
	     auto_host_player_spew_no_expire,
	     auto_host_clients_can_request_rewind,
	     auto_host_observer);
}

/* ── Multiplayer ping query for network stats overlay ────────────
 * Returns an int array:
 *   [0]           = N_players (number of active players)
 *   [1]           = Player_num (my player index)
 *   [2..9]        = ping[0..7] for each player slot (ms, 0 if unused)
 *
 * Shared constant: MAX_PLAYERS = 8 (duplicated in MultiplayerStatsOverlay.kt)
 */
#include "multi.h"

JNIEXPORT jintArray JNICALL
Java_com_dxxredux_app_MainActivity_nativeGetMultiplayerPings(JNIEnv *env, jobject thiz)
{
	extern int Player_num;
	extern int N_players;

	enum { MP_SIZE = 10 }; /* 2 header + 8 player slots */
	jint buf[MP_SIZE];
	memset(buf, 0, sizeof(buf));

	buf[0] = (jint) N_players;
	buf[1] = (jint) Player_num;

	int i;
	for (i = 0; i < MAX_PLAYERS; i++) {
		buf[2 + i] = (jint) Netgame.players[i].ping;
	}

	jintArray result = (*env)->NewIntArray(env, MP_SIZE);
	if (result)
		(*env)->SetIntArrayRegion(env, result, 0, MP_SIZE, buf);
	return result;
}

/* -- Multiplayer packet stats for network stats overlay --------
 * Returns an int array:
 *   [0]            = UDP_num_sendto  (total packets sent by engine)
 *   [1]            = UDP_num_recvfrom (total packets received by engine)
 *   [2..9]         = loss[0..7]   outbound loss % per player slot (0-100)
 *   [10..17]       = rx_loss[0..7] inbound loss % per player slot (0-100)
 *
 * Shared constant: MAX_PLAYERS = 8 (duplicated in MultiplayerStatsOverlay.kt)
 */
JNIEXPORT jintArray JNICALL
Java_com_dxxredux_app_MainActivity_nativeGetMultiplayerPacketStats(JNIEnv *env, jobject thiz)
{
	extern int UDP_num_sendto;
	extern int UDP_num_recvfrom;

	enum { PS_SIZE = 18 }; /* 2 header + 8 loss + 8 rx_loss */
	jint buf[PS_SIZE];
	memset(buf, 0, sizeof(buf));

	buf[0] = (jint) UDP_num_sendto;
	buf[1] = (jint) UDP_num_recvfrom;

	int i;
	for (i = 0; i < MAX_PLAYERS; i++) {
		buf[2 + i] = (jint) Netgame.players[i].loss;
		buf[10 + i] = (jint) Netgame.players[i].rx_loss;
	}

	jintArray result = (*env)->NewIntArray(env, PS_SIZE);
	if (result)
		(*env)->SetIntArrayRegion(env, result, 0, PS_SIZE, buf);
	return result;
}

/* -- Netgame state query for matchmaking game-state updates --------
 * Returns an int array:
 *   [0] = game_status  (NETSTAT_MENU=0, NETSTAT_PLAYING=1, etc.)
 *   [1] = numconnected (currently connected players)
 *   [2] = max_numplayers
 *   [3] = levelnum     (current level in the netgame struct)
 *   [4] = gamemode      (Game_mode bitmask; & 4 == network game)
 *
 * Shared constants duplicated in MatchmakingService.kt:
 *   NETSTAT_MENU=0, NETSTAT_PLAYING=1
 *
 * android port: game state polling for matchmaking server updates
 */
JNIEXPORT jintArray JNICALL
Java_com_dxxredux_app_MainActivity_nativeGetNetgameState(JNIEnv *env, jobject thiz)
{
	extern int Game_mode;
	enum { NGS_SIZE = 5 };
	jint buf[NGS_SIZE];

	buf[0] = (jint) Netgame.game_status;
	buf[1] = (jint) Netgame.numconnected;
	buf[2] = (jint) Netgame.max_numplayers;
	buf[3] = (jint) Netgame.levelnum;
	buf[4] = (jint) Game_mode;

	jintArray result = (*env)->NewIntArray(env, NGS_SIZE);
	if (result)
		(*env)->SetIntArrayRegion(env, result, 0, NGS_SIZE, buf);
	return result;
}

/* ── Coop robot kill stats for coop QoL overlay ──────────────────
 * Returns an int array:
 *   [0]  = total robots killed (sum across all players)
 *   [1]  = num_robots_level (current denominator, grows with matcen spawns)
 *   [2]  = Coop_total_robot_score (total score value of all robots at level start)
 *   [3]  = N_players
 *   [4]  = Player_num (local player index)
 *   [5..5+2*MAX_PLAYERS-1] = per-player pairs: [kills, score_earned] x MAX_PLAYERS
 *
 * Shared constant: MAX_PLAYERS = 8 (duplicated in CoopStatsOverlay.kt)
 *
 * android port: coop QoL overlay
 */
#include "robot.h"
#include "game.h"
JNIEXPORT jintArray JNICALL
Java_com_dxxredux_app_MainActivity_nativeGetCoopRobotStats(JNIEnv *env, jobject thiz)
{
	extern int Player_num;
	extern int N_players;
	extern int Game_mode;

	enum { CS_SIZE = 5 + 2 * MAX_PLAYERS }; /* 5 header + 16 per-player */
	jint buf[CS_SIZE];
	memset(buf, 0, sizeof(buf));

	if (!(Game_mode & GM_MULTI_COOP)) {
		/* Not in coop -- return zeros */
		jintArray result = (*env)->NewIntArray(env, CS_SIZE);
		if (result)
			(*env)->SetIntArrayRegion(env, result, 0, CS_SIZE, buf);
		return result;
	}

	int total_killed = 0;
	int i;
	for (i = 0; i < MAX_PLAYERS; i++)
		total_killed += Coop_kill_stats[i].robots_killed;

	buf[0] = (jint) total_killed;
	buf[1] = (jint) Players[Player_num].num_robots_level;
	buf[2] = (jint) Coop_total_robot_score;
	buf[3] = (jint) N_players;
	buf[4] = (jint) Player_num;

	for (i = 0; i < MAX_PLAYERS; i++) {
		buf[5 + i * 2] = (jint) Coop_kill_stats[i].robots_killed;
		buf[5 + i * 2 + 1] = (jint) Coop_kill_stats[i].score_earned;
	}

	jintArray result = (*env)->NewIntArray(env, CS_SIZE);
	if (result)
		(*env)->SetIntArrayRegion(env, result, 0, CS_SIZE, buf);
	return result;
}

/* ── Teammate status for coop QoL overlay ──────────────────
 * Returns an int array:
 *   [0]  = N_players
 *   [1]  = Player_num
 *   [2]  = Game_mode
 *   [3..3+5*MAX_PLAYERS-1] = per-player groups of 5:
 *       [connected, shields_pct, energy_pct, secondary_weapon, secondary_ammo]
 *
 * Shields/energy as percentage (0-200, can exceed 100 from powerups)
 *
 * Shared constant: MAX_PLAYERS = 8 (duplicated in CoopStatsOverlay.kt)
 *
 * android port: coop QoL overlay
 */
JNIEXPORT jintArray JNICALL
Java_com_dxxredux_app_MainActivity_nativeGetTeammateStatus(JNIEnv *env, jobject thiz)
{
	extern int Player_num;
	extern int N_players;
	extern int Game_mode;

	enum { TS_FIELDS = 5 };
	enum { TS_SIZE = 3 + TS_FIELDS * MAX_PLAYERS };
	jint buf[TS_SIZE];
	memset(buf, 0, sizeof(buf));

	buf[0] = (jint) N_players;
	buf[1] = (jint) Player_num;
	buf[2] = (jint) Game_mode;

	int i;
	for (i = 0; i < MAX_PLAYERS; i++) {
		int base = 3 + i * TS_FIELDS;
		buf[base] = (jint) Players[i].connected;
		/* Convert fix shields/energy to integer value (0-200 range) */
		buf[base + 1] = (jint) (Players[i].shields / F1_0);
		buf[base + 2] = (jint) (Players[i].energy / F1_0);
		buf[base + 3] = (jint) Players[i].secondary_weapon;
		buf[base + 4] = (jint) Players[i].secondary_ammo[Players[i].secondary_weapon];
	}

	jintArray result = (*env)->NewIntArray(env, TS_SIZE);
	if (result)
		(*env)->SetIntArrayRegion(env, result, 0, TS_SIZE, buf);
	return result;
}

/* ── Video stats query for video info overlay ──────────────────
 * Returns an int array:
 *   [0]  = g_current_fps   (frames per second, 1-second window)
 *   [1]  = total_loaded     (textures with a loaded GL texture)
 *   [2]  = hires_count      (hi-res PNG replacement textures)
 *   [3]  = max_hires_w      (max width among hi-res textures)
 *   [4]  = max_hires_h      (max height among hi-res textures)
 *   [5]  = ogl_max_texture_size (engine GL texture cap)
 *   [6]  = tex_memory_kb    (GPU texture memory in KB)
 *   [7]  = render_w         (game render width)
 *   [8]  = render_h         (game render height)
 *   [9]  = display_w        (native display width)
 *   [10] = display_h        (native display height)
 *   [11] = frame_time_us    (last frame time in microseconds)
 *   [12] = frame_time_avg   (rolling avg over 60 frames, us)
 *   [13] = frame_time_max   (max over 60 frames, us)
 *   [14] = tex_binds        (texture bind calls this frame)
 *   [15] = tex_bind_reuse   (texture bind cache hits this frame)
 *   [16] = draw_polys       (flat+textured polygons this frame)
 *   [17] = cache_time_ms    (time spent in last ogl_cache_level_textures)
 *   [18] = aniso_level       (current anisotropic filtering level, 0=off)
 *   [19] = aniso_max         (max aniso level supported by GPU)
 *   [30] = merged_wall_mode      (OFF/Alpha/RGB debug view)
 *   [31] = reserved
 *   [32] = swap_time_us          (eglSwapBuffers or swap wrapper time)
 *   [33] = msaa_resolve_time_us  (MSAA resolve/blit time)
 *   [34] = gl_error_time_us      (end-frame glGetError drain time)
 *   [35] = cache_ktx2_read_ms    (KTX2 read/parse time during cache pass)
 *   [36] = cache_png_read_ms     (PNG/JPG/TGA read/decode time during cache pass)
 *   [37] = cache_upload_ms       (texture upload time during cache pass)
 *   [38] = cache_mask_ms         (mask load/upload time during cache pass)
 *
 * android port: video diagnostics overlay
 */
#include "piggy.h"
#include "ogl_init.h"
JNIEXPORT jintArray JNICALL
Java_com_dxxredux_app_MainActivity_nativeGetVideoStats(JNIEnv *env, jobject thiz)
{
	extern int g_current_fps;
	extern int g_frame_time_us, g_frame_time_avg_us, g_frame_time_max_us;
	extern int r_texbinds, r_texbind_reuse;
	extern int r_polyc, r_tpolyc;
	extern int r_shader_switches, r_mask_draws;
	extern int g_cache_time_ms;
	extern int ogl_max_texture_size;
	extern GLfloat ogl_maxanisotropy;
	extern int ogl_aniso_level;
	extern int ogl_msaa_samples;
	extern int ogl_msaa_max_samples;
	extern int ogl_gpu_timer_available;
	extern int g_gpu_time_us;
	extern int ogl_color_depth;
	extern int g_texfilt_level;
	extern int g_swap_time_us;
	extern int g_msaa_resolve_time_us;
	extern int g_gl_error_time_us;
	extern int g_cache_ktx2_read_ms;
	extern int g_cache_png_read_ms;
	extern int g_cache_upload_ms;
	extern int g_cache_mask_ms;
	int ogl_get_texture_bytes(void);
	int android_surface_get_display_width(void);
	int android_surface_get_display_height(void);
	extern unsigned int grd_curscreen_w(void);
	extern unsigned int grd_curscreen_h(void);

	enum { VS_SIZE = 39 };
	jint buf[VS_SIZE];

	buf[0] = (jint) g_current_fps;

	/* Count hi-res textures (same logic as game_introspect.cpp) */
	int total = 0, replaced = 0, max_w = 0, max_h = 0;
	for (int i = 0; i < Num_bitmap_files; i++) {
		ogl_texture *t = GameBitmaps[i].gltexture;
		if (!t || t->w == 0)
			continue;
		total++;
		if (t->is_png) {
			replaced++;
			if (t->w > max_w) max_w = t->w;
			if (t->h > max_h) max_h = t->h;
		}
	}
	buf[1] = (jint) total;
	buf[2] = (jint) replaced;
	buf[3] = (jint) max_w;
	buf[4] = (jint) max_h;
	buf[5] = (jint) ogl_max_texture_size;
	buf[6] = (jint) (ogl_get_texture_bytes() / 1024);
	buf[7] = (jint) grd_curscreen_w();
	buf[8] = (jint) grd_curscreen_h();
	buf[9] = (jint) android_surface_get_display_width();
	buf[10] = (jint) android_surface_get_display_height();
	buf[11] = (jint) g_frame_time_us;
	buf[12] = (jint) g_frame_time_avg_us;
	buf[13] = (jint) g_frame_time_max_us;
	buf[14] = (jint) r_texbinds;
	buf[15] = (jint) r_texbind_reuse;
	buf[16] = (jint) (r_polyc + r_tpolyc);
	buf[17] = (jint) g_cache_time_ms;
	buf[18] = (jint) ogl_aniso_level;
	buf[19] = (jint) ogl_maxanisotropy;
	buf[20] = (jint) ogl_msaa_samples;
	buf[21] = (jint) ogl_msaa_max_samples;
	buf[22] = (jint) g_gpu_time_us;
	buf[23] = (jint) ogl_gpu_timer_available;
	buf[24] = (jint) r_shader_switches;
	buf[25] = (jint) r_mask_draws;
	buf[26] = (jint) ogl_color_depth;
	buf[27] = (jint) g_texfilt_level;
	buf[28] = (jint) GameCfg.MenuTexFilt;
	buf[29] = (jint) GameCfg.HudTexFilt;
#ifdef INTROSPECT_ON
	buf[30] = (jint) g_merged_wall_debug_mode;
	buf[31] = 0;
#else
	buf[30] = 0;
	buf[31] = 0;
#endif
	buf[32] = (jint) g_swap_time_us;
	buf[33] = (jint) g_msaa_resolve_time_us;
	buf[34] = (jint) g_gl_error_time_us;
	buf[35] = (jint) g_cache_ktx2_read_ms;
	buf[36] = (jint) g_cache_png_read_ms;
	buf[37] = (jint) g_cache_upload_ms;
	buf[38] = (jint) g_cache_mask_ms;

	jintArray result = (*env)->NewIntArray(env, VS_SIZE);
	if (result)
		(*env)->SetIntArrayRegion(env, result, 0, VS_SIZE, buf);
	return result;
}

/* ── Coop warp-to-player status + trigger ──────────────────────
 * nativeGetCoopWarpStatus() returns an int array:
 *   [0] = available       (1 if warp button should show)
 *   [1] = target_pnum     (player index of target)
 *   [2] = cooldown_secs   (seconds left on cooldown, 0 = ready)
 *   [3] = engaged         (1 if recently engaged with robots)
 *
 * nativeCoopWarpExecute() triggers the warp, returns 1 on success
 * nativeCoopWarpCycleTarget() advances to the next eligible target
 *
 * android port: coop QoL warp system
 */
#include "coop_warp.h"

JNIEXPORT jintArray JNICALL
Java_com_dxxredux_app_MainActivity_nativeGetCoopWarpStatus(JNIEnv *env, jobject thiz)
{
	enum { WS_SIZE = 4 };
	jint buf[WS_SIZE];
	memset(buf, 0, sizeof(buf));

	coop_warp_status st;
	coop_warp_get_status(&st);

	buf[0] = (jint) st.available;
	buf[1] = (jint) st.target_pnum;
	buf[2] = (jint) st.cooldown_secs_left;
	buf[3] = (jint) st.engaged;

	jintArray result = (*env)->NewIntArray(env, WS_SIZE);
	if (result)
		(*env)->SetIntArrayRegion(env, result, 0, WS_SIZE, buf);
	return result;
}

JNIEXPORT jstring JNICALL
Java_com_dxxredux_app_MainActivity_nativeGetCoopWarpTargetName(JNIEnv *env, jobject thiz)
{
	coop_warp_status st;
	coop_warp_get_status(&st);

	if (st.available)
		return (*env)->NewStringUTF(env, st.target_callsign);
	return (*env)->NewStringUTF(env, "");
}

JNIEXPORT jint JNICALL
Java_com_dxxredux_app_MainActivity_nativeCoopWarpExecute(JNIEnv *env, jobject thiz)
{
	return (jint) coop_warp_execute();
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeCoopWarpCycleTarget(JNIEnv *env, jobject thiz)
{
	coop_warp_cycle_target();
}

/* -- Host migration notification --
 * Called from multi_disconnect_player() when the surviving player becomes
 * the new host.  Calls MainActivity.onHostMigration() so the Kotlin layer
 * can resume LAN broadcasting for the migrated game.
 *
 * android port: coop host migration support
 */
void android_notify_host_migration(void)
{
	JNIEnv *env = NULL;
	int attached = 0;

	if (!g_jvm || !g_activity)
		return;

	if ((*g_jvm)->GetEnv(g_jvm, (void **) &env, JNI_VERSION_1_6) != JNI_OK) {
		if ((*g_jvm)->AttachCurrentThread(g_jvm, &env, NULL) == JNI_OK)
			attached = 1;
		else
			return;
	}

	jclass cls = (*env)->GetObjectClass(env, g_activity);
	jmethodID mid = (*env)->GetMethodID(env, cls, "onHostMigration", "()V");
	if (mid)
		(*env)->CallVoidMethod(env, g_activity, mid);

	if (attached)
		(*g_jvm)->DetachCurrentThread(g_jvm);
}

/* -- Escort (Guide-Bot) owner status for coop overlay --
 * Returns the escort owner player index, or -1 if no guidebot / not yet freed.
 *
 * android port: coop guidebot multiplayer support
 */
#ifdef DXX_BUILD_DESCENT_II
#include "escort.h"
JNIEXPORT jint JNICALL
Java_com_dxxredux_app_MainActivity_nativeGetEscortOwnerPlayer(JNIEnv *env, jobject thiz)
{
	extern int Game_mode;
	extern int Player_num;
	if (!(Game_mode & GM_MULTI_COOP))
		return (jint) -1;
	return (jint) Escort_owner_player;
}

JNIEXPORT jboolean JNICALL
Java_com_dxxredux_app_MainActivity_nativeIsEscortOwner(JNIEnv *env, jobject thiz)
{
	extern int Game_mode;
	extern int Player_num;
	if (!(Game_mode & GM_MULTI_COOP))
		return JNI_FALSE;
	return (Escort_owner_player == Player_num) ? JNI_TRUE : JNI_FALSE;
}
#else
/* D1 has no Guide-Bot */
JNIEXPORT jint JNICALL
Java_com_dxxredux_app_MainActivity_nativeGetEscortOwnerPlayer(JNIEnv *env, jobject thiz)
{
	return (jint) -1;
}

JNIEXPORT jboolean JNICALL
Java_com_dxxredux_app_MainActivity_nativeIsEscortOwner(JNIEnv *env, jobject thiz)
{
	return JNI_FALSE;
}
#endif

/* Returns the callsign of the current escort owner, or empty string if none */
JNIEXPORT jstring JNICALL
Java_com_dxxredux_app_MainActivity_nativeGetEscortOwnerCallsign(JNIEnv *env, jobject thiz)
{
#ifdef DXX_BUILD_DESCENT_II
	extern int Game_mode;
	if ((Game_mode & GM_MULTI_COOP) &&
	    Escort_owner_player >= 0 && Escort_owner_player < MAX_PLAYERS)
		return (*env)->NewStringUTF(env, Players[Escort_owner_player].callsign);
#endif
	return (*env)->NewStringUTF(env, "");
}

/* Returns true when the guidebot has been released (cage walls destroyed) */
JNIEXPORT jboolean JNICALL
Java_com_dxxredux_app_MainActivity_nativeIsBuddyReleased(JNIEnv *env, jobject thiz)
{
#ifdef DXX_BUILD_DESCENT_II
	return Buddy_allowed_to_talk ? JNI_TRUE : JNI_FALSE;
#else
	return JNI_FALSE;
#endif
}
