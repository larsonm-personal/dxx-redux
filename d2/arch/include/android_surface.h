/*
 * android_surface.h — Bridge between SDL framebuffer and Android ANativeWindow.
 */

#ifndef ANDROID_SURFACE_H
#define ANDROID_SURFACE_H

#ifdef ANDROID
#include <SDL.h>
#include <android/native_window.h>

/* Blit the 8-bit paletted canvas to the Android native window.
 * Called from gr_flip() after the normal SDL blit. */
void android_surface_blit(SDL_Surface *canvas);

/* Release the native window (called on shutdown). */
void android_surface_cleanup(void);

/* Return the current ANativeWindow pointer (for EGL surface creation). */
ANativeWindow *android_surface_get_native_window(void);
#endif

#endif /* ANDROID_SURFACE_H */
