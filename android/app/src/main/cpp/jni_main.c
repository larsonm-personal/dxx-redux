/*
 * jni_main.c — JNI entry point for the Android build of DXX-Redux.
 *
 * Provides a native method that Java/Kotlin can call to start the game engine.
 * For now this is a thin wrapper around the game's main().
 */

#include <jni.h>
#include <stdlib.h>
#include <android/log.h>
#include <physfs.h>
#include <SDL.h>
#ifdef INTROSPECT_ON
#include "game_introspect.h"
#endif

#define LOG_TAG "DXX-Redux"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

/* ── Global JNI references (used by android_input.c for C→Java callbacks) ── */
JavaVM  *g_jvm      = NULL;
jobject  g_activity  = NULL;   /* Global ref to MainActivity */

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
#endif /* INTROSPECT_ON */
