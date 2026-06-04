/*
 * android_surface.c — Bridge between SDL framebuffer and Android ANativeWindow.
 *
 * The game renders to an 8-bit paletted SDL_Surface (canvas).
 * After each gr_flip(), this module converts the paletted pixels to RGBA8888
 * and writes them into the ANativeWindow for display on a SurfaceView.
 */

#ifdef ANDROID

#include <jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/log.h>
#include <string.h>
#include <pthread.h>
#include <SDL.h>

#define LOG_TAG   "DXX-Surface"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

/* ── Global state ───────────────────────────────────────────── */
static ANativeWindow *g_native_window = NULL;
static int g_surface_ready = 0;
static pthread_mutex_t g_surface_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Current palette in ARGB8888, rebuilt whenever SDL_SetColors is called
 * or gr_palette_load runs.  We rebuild it lazily from the canvas palette
 * in android_
 * surface_
 * blit(). */
static uint32_t g_palette_argb[256];
static int g_last_geo_w = 0, g_last_geo_h = 0;
static volatile int g_app_paused = 0;
static volatile int g_egl_surface_stale = 0;
static int g_had_surface = 0;
static int g_surface_view_w = 0;
static int g_surface_view_h = 0;

/* ── JNI entry points called from Kotlin ────────────────────── */

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeSetSurfaceSize(JNIEnv *env, jobject thiz, jint width, jint height)
{
	(void) env;
	(void) thiz;
	pthread_mutex_lock(&g_surface_mutex);
	g_surface_view_w = width > 0 ? width : 0;
	g_surface_view_h = height > 0 ? height : 0;
	LOGI("SurfaceView size %dx%d", g_surface_view_w, g_surface_view_h);
	pthread_mutex_unlock(&g_surface_mutex);
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeSetSurface(JNIEnv *env, jobject thiz, jobject surface)
{
	pthread_mutex_lock(&g_surface_mutex);

	if (g_native_window) {
		ANativeWindow_release(g_native_window);
		g_native_window = NULL;
	}

	if (surface) {
		g_native_window = ANativeWindow_fromSurface(env, surface);
		if (g_native_window) {
			g_surface_ready = 1;
			g_last_geo_w = 0;
			g_last_geo_h = 0;
			if (g_had_surface) {
				g_egl_surface_stale = 1;
				LOGI("ANativeWindow re-acquired — EGL surface marked stale");
			}
			g_had_surface = 1;
			LOGI("ANativeWindow acquired (%dx%d), view=%dx%d",
			     ANativeWindow_getWidth(g_native_window),
			     ANativeWindow_getHeight(g_native_window),
			     g_surface_view_w, g_surface_view_h);
		} else {
			g_surface_ready = 0;
			LOGE("Failed to get ANativeWindow from surface");
		}
	} else {
		g_surface_ready = 0;
		LOGI("Surface destroyed");
	}

	pthread_mutex_unlock(&g_surface_mutex);
}

/* ── Called from gr_flip() in arch/sdl/gr.c ─────────────────── */

static int g_blit_count = 0;

void android_surface_blit(SDL_Surface *canvas)
{
	pthread_mutex_lock(&g_surface_mutex);

	if (!g_surface_ready || !g_native_window || !canvas || g_app_paused) {
		static int skip_count = 0;
		if (skip_count < 5 || (skip_count % 1000) == 0)
			LOGI("blit skip #%d: ready=%d win=%d canvas=%d paused=%d",
			     skip_count, g_surface_ready, g_native_window != NULL, canvas != NULL, g_app_paused);
		skip_count++;
		pthread_mutex_unlock(&g_surface_mutex);
		return;
	}

	/* Rebuild palette LUT from the SDL canvas palette */
	SDL_Palette *pal = canvas->format->palette;
	if (pal) {
		for (int i = 0; i < pal->ncolors && i < 256; i++) {
			g_palette_argb[i] = (0xFFu << 24) | ((uint32_t) pal->colors[i].r << 16) | ((uint32_t) pal->colors[i].g << 8) | ((uint32_t) pal->colors[i].b);
		}
	}

	int src_w = canvas->w;
	int src_h = canvas->h;

	/* Only reconfigure geometry when dimensions change */
	if (src_w != g_last_geo_w || src_h != g_last_geo_h) {
		LOGI("setBuffersGeometry %dx%d", src_w, src_h);
		int geo_ret = ANativeWindow_setBuffersGeometry(g_native_window, src_w, src_h,
		                                               AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM);
		if (geo_ret != 0) {
			LOGE("setBuffersGeometry failed: %d", geo_ret);
			pthread_mutex_unlock(&g_surface_mutex);
			return;
		}
		g_last_geo_w = src_w;
		g_last_geo_h = src_h;
	}

	ANativeWindow_Buffer buf;
	if (ANativeWindow_lock(g_native_window, &buf, NULL) != 0) {
		LOGE("ANativeWindow_lock failed");
		pthread_mutex_unlock(&g_surface_mutex);
		return; /* lock failed — surface may be transitioning */
	}

	/* Convert 8-bit paletted → ARGB8888 */
	const uint8_t *src = (const uint8_t *) canvas->pixels;
	uint32_t *dst = (uint32_t *) buf.bits;
	int dst_stride = buf.stride; /* in pixels, not bytes */

	/* Keyboard viewport offset: when the soft keyboard is visible,
	 * shift the canvas upward so the text input field is visible.
	 * Rows beyond the canvas end are filled with black. */
	extern int android_get_keyboard_y_offset(int canvas_h);
	extern volatile int g_blit_y_offset;
	int y_offset = android_get_keyboard_y_offset(src_h);
	g_blit_y_offset = y_offset;

	/* Blit row by row (canvas pitch may differ from buf stride) */
	for (int y = 0; y < src_h && y < buf.height; y++) {
		uint32_t *dst_row = dst + y * dst_stride;
		int row_w = (src_w < buf.width) ? src_w : buf.width;
		int src_y = y + y_offset;
		if (src_y >= src_h) {
			memset(dst_row, 0, row_w * sizeof(uint32_t));
			continue;
		}
		const uint8_t *src_row = src + src_y * canvas->pitch;
		for (int x = 0; x < row_w; x++) {
			dst_row[x] = g_palette_argb[src_row[x]];
		}
	}

	ANativeWindow_unlockAndPost(g_native_window);

	g_blit_count++;
	if (g_blit_count == 1 || (g_blit_count % 300) == 0) {
		LOGI("blit #%d  canvas=%dx%d  buf=%dx%d  palette[1]=0x%08X",
		     g_blit_count, src_w, src_h, buf.width, buf.height,
		     g_palette_argb[1]);
	}

	pthread_mutex_unlock(&g_surface_mutex);
}

/* ── App lifecycle pause/resume ──────────────────────────────
 * Called from nativeOnPause/nativeOnResume (UI thread) to prevent
 * the rendering thread from touching ANativeWindow while the app
 * is backgrounded.  The mutex ensures any in-progress blit finishes
 * before the flag takes effect.
 */

void android_surface_pause(void)
{
	pthread_mutex_lock(&g_surface_mutex);
	g_app_paused = 1;
	LOGI("surface paused (app backgrounded)");
	pthread_mutex_unlock(&g_surface_mutex);
}

void android_surface_resume(void)
{
	pthread_mutex_lock(&g_surface_mutex);
	g_app_paused = 0;
	LOGI("surface resumed (app foregrounded)");
	pthread_mutex_unlock(&g_surface_mutex);
}

/* ── Accessors for EGL lifecycle ─────────────────────────────── */

int android_surface_egl_needs_recreate(void)
{
	if (g_egl_surface_stale) {
		g_egl_surface_stale = 0;
		return 1;
	}
	return 0;
}

int android_surface_is_paused(void)
{
	return g_app_paused;
}

/* ── Cleanup ────────────────────────────────────────────────── */

void android_surface_cleanup(void)
{
	pthread_mutex_lock(&g_surface_mutex);
	if (g_native_window) {
		ANativeWindow_release(g_native_window);
		g_native_window = NULL;
	}
	g_surface_ready = 0;
	pthread_mutex_unlock(&g_surface_mutex);
}

/* ── Accessor for OGL EGL init ──────────────────────────────── */

ANativeWindow *android_surface_get_native_window(void)
{
	return g_native_window;
}

int android_surface_get_display_width(void)
{
	if (g_surface_view_w > 0)
		return g_surface_view_w;
	return g_native_window ? ANativeWindow_getWidth(g_native_window) : 0;
}

int android_surface_get_display_height(void)
{
	if (g_surface_view_h > 0)
		return g_surface_view_h;
	return g_native_window ? ANativeWindow_getHeight(g_native_window) : 0;
}

#endif /* ANDROID */
