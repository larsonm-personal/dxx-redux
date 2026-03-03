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

#define LOG_TAG "DXX-Surface"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
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
static int g_palette_dirty = 1;

/* ── JNI entry points called from Kotlin ────────────────────── */

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
            LOGI("ANativeWindow acquired (%dx%d)",
                 ANativeWindow_getWidth(g_native_window),
                 ANativeWindow_getHeight(g_native_window));
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

    if (!g_surface_ready || !g_native_window || !canvas) {
        pthread_mutex_unlock(&g_surface_mutex);
        return;
    }

    /* Rebuild palette LUT from the SDL canvas palette */
    SDL_Palette *pal = canvas->format->palette;
    if (pal) {
        for (int i = 0; i < pal->ncolors && i < 256; i++) {
            g_palette_argb[i] = (0xFFu << 24)
                              | ((uint32_t)pal->colors[i].r << 16)
                              | ((uint32_t)pal->colors[i].g << 8)
                              | ((uint32_t)pal->colors[i].b);
        }
    }

    int src_w = canvas->w;
    int src_h = canvas->h;

    /* Configure the native window buffer to match the canvas size */
    ANativeWindow_setBuffersGeometry(g_native_window, src_w, src_h,
                                     AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM);

    ANativeWindow_Buffer buf;
    if (ANativeWindow_lock(g_native_window, &buf, NULL) != 0) {
        pthread_mutex_unlock(&g_surface_mutex);
        return;  /* lock failed — surface may be transitioning */
    }

    /* Convert 8-bit paletted → ARGB8888 */
    const uint8_t *src = (const uint8_t *)canvas->pixels;
    uint32_t *dst = (uint32_t *)buf.bits;
    int dst_stride = buf.stride;  /* in pixels, not bytes */

    /* Blit row by row (canvas pitch may differ from buf stride) */
    for (int y = 0; y < src_h && y < buf.height; y++) {
        const uint8_t *src_row = src + y * canvas->pitch;
        uint32_t *dst_row = dst + y * dst_stride;
        int row_w = (src_w < buf.width) ? src_w : buf.width;
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

#endif /* ANDROID */
