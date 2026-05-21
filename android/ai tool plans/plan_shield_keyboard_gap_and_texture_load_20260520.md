# Shield keyboard gap and texture load plan

## Status

- [x] Research current code paths and prior notes
- [ ] Implement Shield keyboard-gap fill
- [ ] Add focused texture-load instrumentation
- [ ] Brighten loading progress bar border and fill
- [ ] Build and validate on Android

## User reports

1. On Shield, the "Select starting level" menu shifts upward for the soft keyboard, but the space below the drawn area that remains visible to the left and right of the centered keyboard shows stale menu content instead of a stable solid color
2. Texture loading on Shield is much slower than expected and needs instrumentation to show where the time goes
3. The loading progress bar border and fill need about 2x the current brightness

## Findings

### 1. Keyboard-gap artifact is on the Android OGL menu path, not the software blit path

Relevant anchors:

- `d2/arch/ogl/ogl.c` `gr_flip()` and `ogl_end_frame()`
- `d1/arch/ogl/ogl.c` same blocks
- `d2/include/internal.h` and `d1/include/internal.h` `OGL_VIEWPORT(...)`
- `android/app/src/main/cpp/android_input.c` `android_get_keyboard_y_offset()`

What the code does now:

- Android OGL menus shift upward by changing the viewport with `glViewport(0, canvas_h - h + koff, w, h)`
- `koff` comes from `android_get_keyboard_y_offset()` and is already remapped for scaled menus
- This path updates `g_blit_y_offset` for touch remapping, so the keyboard shift itself is intentional and already wired through the rest of the Android input path

Local hypothesis:

- The Shield artifact comes from the newly exposed bottom strip not being explicitly repainted after the viewport is shifted up
- On Android, `gr_flip()` prepares the next menu viewport before `eglSwapBuffers()` and does not clear that exposed strip afterward
- Content outside the shifted viewport is therefore undefined driver-preserved framebuffer data, which would explain why Shield shows stale bottom-of-screen fragments and inconsistent redraws there

Cheap discriminating check:

- Add a temporary scissor-clear of the bottom `koff` pixels in `gr_flip()` right after the viewport shift and before swap
- If the stale fragments disappear on Shield, the root cause is confirmed without touching the menu layout math

Notes for the actual fix:

- The user asked for a solid fill, ideally close to the average color of the rest of the screen
- The low-risk first fix is a solid fill in the exposed strip, mirrored in D1 and D2
- If an exact average proves too expensive to compute every frame, start with a stable representative color and only add dynamic averaging if the first fix is visually poor
- Avoid per-frame `glReadPixels()` for this on Shield. The code already has Android comments warning that readback on menu frames is risky on Shield-class drivers

### 2. Texture loading already has coarse timing, but not enough to isolate the stall

Relevant anchors:

- `d2/arch/ogl/ogl.c` and `d1/arch/ogl/ogl.c` `ogl_cache_level_textures()`
- `d2/arch/ogl/ogl.c` and `d1/arch/ogl/ogl.c` `ogl_loadbmtexture_f()`
- `android/app/src/main/cpp/shared/pngfile_stb.c` `read_png()` and `read_ktx2_file()`
- `android/app/src/main/cpp/shared/ogl_texture_android.c` `android_ogl_load_dxa_mask()`
- `android/app/src/main/cpp/shared/android_loading_progress.c`
- `android/app/src/main/java/com/dxxredux/app/VideoInfoOverlay.kt`
- `android/app/src/main/cpp/jni_main.c` `nativeGetVideoStats()`

What is already there:

- `ogl_cache_level_textures()` already measures total cache time into `g_cache_time_ms`
- `VideoInfoOverlay` already displays `cacheTimeMs`
- The loading overlay already receives per-item progress updates through `android_loading_progress_*`
- `DLOG_TEXTURE` already exists and is used heavily for Android texture diagnostics

What is missing:

- No split between file lookup time, file read time, decode or parse time, GL upload time, mask load time, and fallback behavior
- No summary of how many textures came from KTX2, PNG or JPG fallback, or stock bitmap upload
- No "top N slowest textures" summary to identify pathological assets or repeated fallback misses

Local hypothesis:

- Shield slowness is likely in one or more of these buckets:
  - repeated PhysFS lookup misses before the successful file variant is found
  - KTX2 container parse cost in `read_ktx2_file()`
  - PNG or JPG fallback decode in `read_png()` via stb_image
  - GL upload cost in `glCompressedTexImage2D()` or `ogl_loadtexture()`
  - extra mask loads for super-transparent textures
- The current `g_cache_time_ms` only tells us that the cache pass is slow, not which bucket dominates on Shield

Cheap discriminating check:

- Add aggregated timers and counts behind a debug gate and compare one Shield level load with hires textures enabled
- The first pass should log totals and top offenders, not one line per texture unless the debug flag is explicitly enabled

Instrumentation shape that fits the existing codebase:

- Reuse `debug_log(DLOG_TEXTURE, ...)` and the exported Android debug log files instead of relying on logcat scrollback
- Keep the first pass aggregate-first:
  - total textures visited
  - KTX2 hit count and miss count
  - PNG or JPG fallback hit count and miss count
  - stock bitmap upload count
  - mask load count
  - total ms and average ms for:
    - file read
    - decode or parse
    - GL upload
    - mask upload
  - top 10 slowest textures with file name, dimensions, path chosen, and stage breakdown
- Gate the detailed profiling behind an explicit Android debug pref so normal startup cost is unchanged

### 3. The brightness request is a small Kotlin-only tweak

Relevant anchors:

- `android/app/src/main/java/com/dxxredux/app/LoadingProgressOverlayView.kt`
- `android/app/src/main/java/com/dxxredux/app/MainActivity.kt` overlay attachment and JNI callbacks

What the code does now:

- The progress bar colors are fixed in `LoadingProgressOverlayView.kt`
- `borderPaint` is currently `Color.argb(224, 0x19, 0x1A, 0x17)`
- `fillPaint` is currently `Color.argb(192, 0x21, 0x1E, 0x19)`

Local hypothesis:

- The user-visible brightness ask can be satisfied by brightening only those two paints, without changing layout, timing, or JNI plumbing

Cheap discriminating check:

- Double the RGB contribution approximately, clamp to 255, keep the same general hue, and verify visually on the loading screen

## Proposed work order

### Phase 1. Shield keyboard-gap fill

- Implement a narrow Android-only bottom-strip fill in `gr_flip()` after computing `koff` and before swap
- Mirror the same change in D1 and D2
- Start with a solid fill and only add dynamic average-color sampling if the plain fill looks wrong
- Keep the viewport math and touch remapping unchanged unless the discriminating check disproves the current hypothesis

### Phase 2. Texture-load profiling

- Add a small shared Android helper for cumulative texture-load stats, or extend the existing shared Android texture-debug helper
- Time these sub-stages separately:
  - file lookup and open
  - file read
  - decode or parse
  - GL upload
  - mask load and upload
- Record counts for KTX2 success, KTX2 skip, PNG fallback success, fallback miss, stock upload, and mask loads
- Emit one end-of-load summary plus a bounded slowest-textures table when the debug flag is enabled
- Keep any D1 or D2 source edits small and mirrored

### Phase 3. Progress-bar brightness

- Brighten `borderPaint` and `fillPaint` in `LoadingProgressOverlayView.kt`
- Leave text paint unchanged unless the brighter fill makes text readability worse

### Phase 4. Validation

- Focused Android native build
- Focused Kotlin test or compile if the overlay tweak touches only Kotlin
- Existing keyboard viewport automation can still cover the no-crash path:
  - `android/game_scripts/test_keyboard_viewport.json5`
- Shield-specific visual validation is still required for the keyboard-gap artifact because the bug depends on that centered-keyboard presentation
- Use `VideoInfoOverlay` for baseline and post-change cache-time comparison, then export texture debug logs for the detailed instrumentation summary

## Expected file touch list

- `d2/arch/ogl/ogl.c`
- `d1/arch/ogl/ogl.c`
- `android/app/src/main/cpp/shared/android_texture_debug.c` or a new nearby shared helper
- `android/app/src/main/cpp/shared/android_texture_debug.h` or a new nearby shared header
- `android/app/src/main/cpp/shared/pngfile_stb.c`
- `android/app/src/main/cpp/shared/ogl_texture_android.c`
- `android/app/src/main/java/com/dxxredux/app/LoadingProgressOverlayView.kt`
- optional: `android/app/src/main/cpp/shared/game_introspect.cpp` or `android/app/src/main/cpp/jni_main.c` if the profiling summary should surface beyond debug logs

## Risks and guardrails

- Do not add per-frame readback for the Shield gap fill. The existing Android code already documents that menu-frame readback is a driver risk on Shield-class devices
- Keep texture profiling behind a debug gate so startup performance is unchanged when the instrumentation is off
- Mirror the OGL fix in both D1 and D2
- Prefer shared Android helper code for the new texture-load summaries so the duplicated D1 and D2 `ogl.c` changes stay small