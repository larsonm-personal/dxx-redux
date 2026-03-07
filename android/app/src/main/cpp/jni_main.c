/*
 * jni_main.c — JNI entry point for the Android build of DXX-Redux.
 *
 * Provides a native method that Java/Kotlin can call to start the game engine.
 * For now this is a thin wrapper around the game's main().
 */

#include <jni.h>
#include <stdlib.h>
#include <android/log.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <physfs.h>
#include <SDL.h>

#ifdef INTROSPECT_ON
#include "game_introspect.h"
#include "game_automate.h"
#endif

#define LOG_TAG "DXX-Redux"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

/* ── Global JNI references (used by android_input.c for C→Java callbacks) ── */
JavaVM  *g_jvm      = NULL;
jobject  g_activity  = NULL;   /* Global ref to MainActivity */

/* ── AAssetManager (used by digi_tsf_music.c to load the GM soundfont) ── */
AAssetManager *g_asset_manager = NULL;

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
    androidInit.jnienv = (void *)env;
    androidInit.context = (void *)thiz;   /* Activity is a valid Context */

    char *argv[] = { (char *)&androidInit, NULL };
    main(1, argv);

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
    jmethodID myPid   = (*env)->GetStaticMethodID(env, processCls, "myPid", "()I");
    jmethodID killProc = (*env)->GetStaticMethodID(env, processCls, "killProcess", "(I)V");
    jint pid = (*env)->CallStaticIntMethod(env, processCls, myPid);
    (*env)->CallStaticVoidMethod(env, processCls, killProc, pid);
}

JNIEXPORT jint JNICALL
Java_com_dxxredux_app_MainActivity_nativeGetGameWidth(JNIEnv *env, jobject thiz)
{
    extern unsigned int grd_curscreen_w(void);
    return (jint)grd_curscreen_w();
}

JNIEXPORT jint JNICALL
Java_com_dxxredux_app_MainActivity_nativeGetGameHeight(JNIEnv *env, jobject thiz)
{
    extern unsigned int grd_curscreen_h(void);
    return (jint)grd_curscreen_h();
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeQuit(JNIEnv *env, jobject thiz)
{
    LOGI("nativeQuit: pushing SDL_QUIT event");
    SDL_Event ev;
    ev.type = SDL_QUIT;
    SDL_PushEvent(&ev);
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

/* ── Automation: load and run a JSON script of input steps ────────── */
JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeLoadAutomationScript(JNIEnv *env, jobject thiz, jstring jpath)
{
    const char *path = (*env)->GetStringUTFChars(env, jpath, NULL);
    game_automate_load_script(path);
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
    tsf_music_set_gain_db((float)gain_db);
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeSetMusicVoices(JNIEnv *env, jobject thiz, jint max_voices)
{
    tsf_music_set_max_voices((int)max_voices);
}
#endif /* INTROSPECT_ON */
