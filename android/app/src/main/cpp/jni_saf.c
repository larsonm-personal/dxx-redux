/*
 * jni_saf.c — JNI bridge for the SAF leave-in-place system.
 *
 * Provides saf_open_file() which the custom PhysFS archiver calls
 * to acquire a native file descriptor for a SAF content URI.
 * The call goes: C archiver → this bridge → Java/Kotlin
 * (MainActivity.openSafFile) → ContentResolver → native fd.
 */

#include <jni.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <android/log.h>

#define LOG_TAG "DXX-SAF"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

/* Globals from jni_main.c */
extern JavaVM  *g_jvm;
extern jobject  g_activity;   /* Global ref to MainActivity */

/*
 * Open a file identified by a SAF content URI (or a plain path for testing).
 * Returns a native fd (caller owns it and must close() when done), or -1.
 *
 * For adb testing: if content_uri starts with '/', it's treated as a plain
 * filesystem path and opened directly — no JNI round-trip needed.
 */
int saf_open_file(const char *content_uri)
{
    /* Test mode: plain filesystem path (for adb-based testing) */
    if (content_uri[0] == '/') {
        int fd = open(content_uri, O_RDONLY);
        if (fd < 0)
            LOGE("saf_open_file: open(\"%s\") failed: errno=%d", content_uri, errno);
        return fd;
    }

    /* Production mode: go through JNI → ContentResolver */
    if (!g_jvm || !g_activity) {
        LOGE("saf_open_file: g_jvm or g_activity not set");
        return -1;
    }

    JNIEnv *env = NULL;
    int need_detach = 0;

    jint rc = (*g_jvm)->GetEnv(g_jvm, (void **)&env, JNI_VERSION_1_6);
    if (rc == JNI_EDETACHED) {
        if ((*g_jvm)->AttachCurrentThread(g_jvm, &env, NULL) != JNI_OK) {
            LOGE("saf_open_file: AttachCurrentThread failed");
            return -1;
        }
        need_detach = 1;
    } else if (rc != JNI_OK) {
        LOGE("saf_open_file: GetEnv failed (rc=%d)", rc);
        return -1;
    }

    jclass cls = (*env)->GetObjectClass(env, g_activity);
    jmethodID mid = (*env)->GetMethodID(env, cls, "openSafFile",
                                        "(Ljava/lang/String;)I");
    if (!mid) {
        LOGE("saf_open_file: openSafFile method not found");
        (*env)->DeleteLocalRef(env, cls);
        if (need_detach) (*g_jvm)->DetachCurrentThread(g_jvm);
        return -1;
    }

    jstring juri = (*env)->NewStringUTF(env, content_uri);
    int fd = (*env)->CallIntMethod(env, g_activity, mid, juri);

    /* Check for Java exceptions */
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);
        fd = -1;
    }

    (*env)->DeleteLocalRef(env, juri);
    (*env)->DeleteLocalRef(env, cls);

    if (need_detach)
        (*g_jvm)->DetachCurrentThread(g_jvm);

    if (fd < 0)
        LOGE("saf_open_file: Java returned -1 for \"%s\"", content_uri);

    return fd;
}
