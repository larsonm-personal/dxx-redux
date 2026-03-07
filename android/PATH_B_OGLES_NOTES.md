# Path B: OpenGL ES Rendering on Android

## Goal
Replace the 8-bit paletted software renderer with OpenGL ES 1.1 hardware rendering
on Android, matching desktop visual quality (bilinear filtering, smooth font scaling,
proper shading).

## Current State (Software Renderer)
- `android/app/src/main/cpp/CMakeLists.txt` line 276: `set(OPENGL OFF CACHE BOOL "" FORCE)`
- SDL dummy video driver (`SDL_VIDEO_DRIVER_DUMMY=1` in `SDL_config_android.h`)
- Pipeline: 8-bit paletted SDL_Surface → palette→ARGB8888 row-by-row conversion
  in `android_surface.c` → `ANativeWindow_unlockAndPost()`
- No texture filtering, no smooth fonts, no hardware acceleration

## Existing OGLES Code
The codebase already has extensive `#ifdef OGLES` paths written for Raspberry Pi:
- `d2/arch/ogl/ogl.c` — texture uploads, rendering, custom `glFrustumf()`/`glOrthof()`
- `d2/arch/ogl/gr.c` — EGL initialization, context management, `ogl_init_window()`
- Uses EGL + GLES 1.1 fixed-function pipeline
- `FNTScaleX`/`FNTScaleY` for smooth font scaling via textured quads

### EGL Init in `d2/arch/ogl/gr.c`
```
ogl_init_window():
  1. eglGetDisplay()
  2. eglInitialize()
  3. eglChooseConfig() — RGB565, depth 16, EGL_WINDOW_BIT
  4. eglCreateWindowSurface() — with native window handle
  5. eglCreateContext() — GLES 1.x
  6. eglMakeCurrent()
```

### Native Window Types
| Platform | Window Type            | Source                  |
|----------|------------------------|-------------------------|
| RPI      | DISPMANX_WINDOW_T      | bcm_host DispmanX API   |
| X11      | X11 Window             | SDL_GetWMInfo()         |
| Android  | ANativeWindow*         | Already in android_surface.c |

## Implementation Plan

### 1. CMake Changes (`android/app/src/main/cpp/CMakeLists.txt`)
- Change `set(OPENGL OFF ...)` → `set(OPENGL ON ...)`
- Add compile definition `-DOGLES`
- Link `EGL` and `GLESv1_CM` (both provided by Android NDK, no find_package needed):
  ```cmake
  target_link_libraries(d2x-redux ... EGL GLESv1_CM)
  ```
- Include `d2/arch/ogl/` sources instead of (or alongside) `d2/arch/sdl/gr.c`
- Remove or guard `android_surface.c` palette blit (no longer needed with OGL)

### 2. Android EGL Branch in `d2/arch/ogl/gr.c`
Add `#ifdef ANDROID` block in `ogl_init_window()`:
```c
#ifdef ANDROID
    extern ANativeWindow *g_native_window;  // from android_surface.c / jni_main.c
    eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(eglDisplay, &major, &minor);
    eglChooseConfig(eglDisplay, configAttribs, &eglConfig, 1, &numConfigs);
    eglSurface = eglCreateWindowSurface(eglDisplay, eglConfig,
                                         (EGLNativeWindowType)g_native_window, NULL);
    eglContext = eglCreateContext(eglDisplay, eglConfig, EGL_NO_CONTEXT, contextAttribs);
    eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext);
#endif
```

### 3. Surface Lifecycle
- `nativeSetSurface(surface)` already stores `ANativeWindow*`
- On `surfaceCreated`: EGL init with that window
- On `surfaceDestroyed`: `eglDestroySurface()` + `eglMakeCurrent(NULL)`
- On `surfaceChanged`: recreate EGL surface at new size
- Need thread synchronization: EGL calls must happen on the GL/game thread

### 4. Frame Presentation
- Replace `android_surface_blit()` in `gr_flip()` with `eglSwapBuffers(eglDisplay, eglSurface)`
- The OGL `gr_flip()` in `d2/arch/ogl/gr.c` already calls `eglSwapBuffers()` for OGLES

### 5. SDL Interaction
- SDL dummy driver still initializes (provides event pump, timers, audio routing)
- OGL bypasses SDL for video — SDL_Surface/screen not used
- `gr_set_mode()` in `d2/arch/ogl/gr.c` already creates its own canvas/bitmap

### 6. Remove X11/DispmanX Dependencies
- The existing `ogl_init_window()` includes `<X11/Xlib.h>` and `<SDL_syswm.h>`
- Android path must not include these — use `#ifdef ANDROID` to exclude
- Remove `bcm_host_init()` calls (RPI-specific)

### 7. Font Scaling (Automatic)
- OGL path sets `FNTScaleX = (float)w / 640.0` and `FNTScaleY = (float)h / 480.0`
- Fonts render as textured quads with bilinear filtering → smooth at any resolution
- This is one of the biggest visual improvements over software rendering

### 8. Texture Format Differences
- Desktop GL: `GL_RGBA8`, `GL_RGB8`
- OGLES: `GL_RGBA`, `GL_RGB` (no bit-width suffix)
- Already handled by existing `#ifdef OGLES` in `d2/arch/ogl/ogl.c`

## Key Files to Modify
1. `android/app/src/main/cpp/CMakeLists.txt` — enable OPENGL, link EGL/GLES, add ogl sources
2. `d2/arch/ogl/gr.c` — add `#ifdef ANDROID` EGL init using ANativeWindow
3. `d2/arch/ogl/ogl.c` — may need minor tweaks for Android GLES headers
4. `android/app/src/main/cpp/jni_main.c` — surface lifecycle JNI changes
5. `d2/arch/sdl/gr.c` — guard `android_surface_blit()` with `#ifndef OGL`

## Risks and Considerations
- **Thread safety**: EGL context is thread-bound; game loop and surface callbacks
  run on different threads. Need careful synchronization.
- **Surface loss**: Android can destroy surfaces on pause/resume. Must handle
  EGL surface recreation and texture re-upload gracefully.
- **GLES 1.1 limitations**: No shaders, fixed-function only. Already handled
  by existing OGLES code paths.
- **Fallback**: Keep software renderer buildable via CMake option for debugging.
- **GLEW not needed**: Android NDK provides GLES headers directly. Remove GLEW
  dependency for Android builds.

## Expected Visual Improvements
- Bilinear texture filtering (smooth walls/floors instead of chunky pixels)
- Smooth font scaling at any resolution via textured quads
- Proper alpha blending for transparent effects
- Hardware-accelerated rendering = higher FPS at higher resolutions
- Resolution independence — can render at display native resolution
