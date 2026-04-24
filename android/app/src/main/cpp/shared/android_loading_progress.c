#include "android_loading_progress.h"

#ifdef ANDROID

#include <jni.h>
#include <pthread.h>
#include <string.h>
#include <time.h>

extern JavaVM *g_jvm;
extern jobject g_activity;

static pthread_mutex_t g_loading_progress_lock = PTHREAD_MUTEX_INITIALIZER;
static int g_loading_progress_active = 0;
static int g_loading_progress_total = 0;
static int g_loading_progress_done = 0;
static int g_loading_progress_has_flush = 0;
static char g_loading_progress_phase[64];
static char g_loading_progress_label[64];
static struct timespec g_loading_progress_last_flush;

static void loading_progress_copy_label(char *dst, size_t dst_size, const char *src)
{
	const char *name;
	size_t len;

	if (!dst_size)
		return;
	dst[0] = 0;
	if (!src || !*src)
		return;
	name = strrchr(src, '/');
	if (name)
		src = name + 1;
	name = strrchr(src, '\\');
	if (name)
		src = name + 1;
	len = strlen(src);
	if (len >= dst_size)
		len = dst_size - 1;
	memcpy(dst, src, len);
	dst[len] = 0;
}

static long long loading_progress_elapsed_ms(const struct timespec *now, const struct timespec *then)
{
	return (long long) (now->tv_sec - then->tv_sec) * 1000LL +
	       (long long) (now->tv_nsec - then->tv_nsec) / 1000000LL;
}

static void loading_progress_call_show(const char *phase, const char *label, int percent)
{
	JNIEnv *env;
	int attached = 0;
	if (!g_jvm || !g_activity)
		return;
	if ((*g_jvm)->GetEnv(g_jvm, (void **) &env, JNI_VERSION_1_6) != JNI_OK) {
		(*g_jvm)->AttachCurrentThread(g_jvm, &env, NULL);
		attached = 1;
	}
	{
		jclass cls = (*env)->GetObjectClass(env, g_activity);
		jmethodID mid = (*env)->GetMethodID(env, cls, "showLoadingProgress", "(Ljava/lang/String;Ljava/lang/String;I)V");
		if (mid) {
			jstring jphase = (*env)->NewStringUTF(env, phase ? phase : "");
			jstring jlabel = (*env)->NewStringUTF(env, label ? label : "");
			(*env)->CallVoidMethod(env, g_activity, mid, jphase, jlabel, percent);
			(*env)->DeleteLocalRef(env, jlabel);
			(*env)->DeleteLocalRef(env, jphase);
		}
		(*env)->DeleteLocalRef(env, cls);
	}
	if (attached)
		(*g_jvm)->DetachCurrentThread(g_jvm);
}

static void loading_progress_call_hide(void)
{
	JNIEnv *env;
	int attached = 0;
	if (!g_jvm || !g_activity)
		return;
	if ((*g_jvm)->GetEnv(g_jvm, (void **) &env, JNI_VERSION_1_6) != JNI_OK) {
		(*g_jvm)->AttachCurrentThread(g_jvm, &env, NULL);
		attached = 1;
	}
	{
		jclass cls = (*env)->GetObjectClass(env, g_activity);
		jmethodID mid = (*env)->GetMethodID(env, cls, "hideLoadingProgress", "()V");
		if (mid)
			(*env)->CallVoidMethod(env, g_activity, mid);
		(*env)->DeleteLocalRef(env, cls);
	}
	if (attached)
		(*g_jvm)->DetachCurrentThread(g_jvm);
}

static void loading_progress_flush_locked(int force)
{
	struct timespec now;
	char phase[sizeof(g_loading_progress_phase)];
	char label[sizeof(g_loading_progress_label)];
	int percent;

	if (!g_loading_progress_active)
		return;
	clock_gettime(CLOCK_MONOTONIC, &now);
	if (!force && g_loading_progress_has_flush &&
	    loading_progress_elapsed_ms(&now, &g_loading_progress_last_flush) < 300)
		return;
	g_loading_progress_last_flush = now;
	g_loading_progress_has_flush = 1;
	strcpy(phase, g_loading_progress_phase);
	strcpy(label, g_loading_progress_label);
	if (g_loading_progress_total > 0) {
		if (g_loading_progress_done < 0)
			g_loading_progress_done = 0;
		if (g_loading_progress_done > g_loading_progress_total)
			g_loading_progress_done = g_loading_progress_total;
		percent = (100 * g_loading_progress_done + g_loading_progress_total / 2) / g_loading_progress_total;
	} else {
		percent = 0;
	}
	loading_progress_call_show(phase, label, percent);
}

void android_loading_progress_begin(const char *phase_label, int total_items)
{
	pthread_mutex_lock(&g_loading_progress_lock);
	g_loading_progress_active = 1;
	g_loading_progress_total = total_items;
	g_loading_progress_done = 0;
	g_loading_progress_has_flush = 0;
	g_loading_progress_label[0] = 0;
	loading_progress_copy_label(g_loading_progress_phase, sizeof(g_loading_progress_phase), phase_label);
	if (!g_loading_progress_phase[0])
		strcpy(g_loading_progress_phase, "Prepare for Descent");
	pthread_mutex_unlock(&g_loading_progress_lock);
}

void android_loading_progress_step(const char *item_label)
{
	pthread_mutex_lock(&g_loading_progress_lock);
	if (!g_loading_progress_active) {
		pthread_mutex_unlock(&g_loading_progress_lock);
		return;
	}
	g_loading_progress_done++;
	if (item_label && *item_label)
		loading_progress_copy_label(g_loading_progress_label, sizeof(g_loading_progress_label), item_label);
	loading_progress_flush_locked(g_loading_progress_done == 1);
	pthread_mutex_unlock(&g_loading_progress_lock);
}

void android_loading_progress_end(void)
{
	pthread_mutex_lock(&g_loading_progress_lock);
	if (!g_loading_progress_active) {
		pthread_mutex_unlock(&g_loading_progress_lock);
		return;
	}
	if (g_loading_progress_total > 0)
		g_loading_progress_done = g_loading_progress_total;
	loading_progress_flush_locked(1);
	g_loading_progress_active = 0;
	g_loading_progress_total = 0;
	g_loading_progress_done = 0;
	g_loading_progress_has_flush = 0;
	g_loading_progress_phase[0] = 0;
	g_loading_progress_label[0] = 0;
	pthread_mutex_unlock(&g_loading_progress_lock);
	loading_progress_call_hide();
}

#else

void android_loading_progress_begin(const char *phase_label, int total_items)
{
	(void) phase_label;
	(void) total_items;
}

void android_loading_progress_step(const char *item_label)
{
	(void) item_label;
}

void android_loading_progress_end(void)
{
}

#endif