/*
 * jni_main.c — JNI entry point for the Android build of DXX-Redux.
 *
 * Provides a native method that Java/Kotlin can call to start the game engine.
 * For now this is a thin wrapper around the game's main().
 */

#include <jni.h>
#include <android/log.h>
#include <physfs.h>

#define LOG_TAG "DXX-Redux"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

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
}
