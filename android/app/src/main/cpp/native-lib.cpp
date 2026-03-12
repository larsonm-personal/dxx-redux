// Hello-world JNI function to verify the Android NDK toolchain.
// This will be replaced with the real game entry point later.

#include <jni.h>
#include <string>
#include <android/log.h>

#define LOG_TAG   "dxx-redux"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

extern "C" JNIEXPORT jstring JNICALL
Java_com_dxxredux_app_MainActivity_helloFromNative(JNIEnv *env, jobject /* this */)
{
	LOGI("Native library loaded successfully");
	return env->NewStringUTF("DXX-Redux NDK build works!");
}
