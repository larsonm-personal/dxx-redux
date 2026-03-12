/*
 * messagebox.c — Android stub for platform message boxes.
 * Uses __android_log_print since Android has no native desktop-style message box.
 */

#include <android/log.h>

#define LOG_TAG "DXX-Redux"

void msgbox_warning(char *message)
{
	__android_log_print(ANDROID_LOG_WARN, LOG_TAG, "WARNING: %s", message);
}

void msgbox_error(const char *message)
{
	__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "ERROR: %s", message);
}
