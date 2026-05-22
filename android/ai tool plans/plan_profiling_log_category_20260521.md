# Profiling log category and Shield performance plan

## Status

- [x] Survey current logging, frame, graphics, texture, storage, and overlay paths
- [x] Draft implementation plan for a low-volume profiling category
- [x] Add the profiling category and sampled log sink
- [x] Add initial native frame wickets
- [ ] Add texture, storage, and syscall wickets
- [ ] Add Kotlin overlay timing
- [ ] Add parser tooling and validate on Shield

Validation so far:

- `cd android; .\gradlew.bat assembleDebug`
- `cd android; .\gradlew.bat testDebugUnitTest`

## Goal

Add a new debug log category named `Profiling` that can run on Shield without flooding storage. The first implementation should answer these questions from one bad-scene capture:

1. Is the lost time in simulation, rendering, frame flip, GPU wait, texture loading, storage I/O, or Kotlin overlays
2. Which named functions or syscall-like boundaries dominate the sampled time
3. Which texture names or storage operations dominate level load and runtime page-in stalls
4. Whether the result points to an engine fix, a graphics setting/default issue, a texture-pack pipeline issue, or overlay/UI work

## Core design

Use two layers rather than one giant text log:

1. A low-overhead in-app wicket profiler that is controlled by `DLOG_PROFILING`
2. Optional external simpleperf or Perfetto captures when the in-app wickets show a large unexplained bucket

The in-app profiler should aggregate in native and Kotlin code, then write compact machine-parseable lines. No per-function `debug_log()` calls inside hot loops. The profiler should default to sampling for 1000 ms every 10000 ms while the category is enabled.

## Logging category integration

Files:

- `android/app/src/main/cpp/shared/debug_log_categories.h`
- `android/app/src/main/cpp/shared/android_log.c`
- `android/app/src/main/java/com/dxxredux/app/DebugLogCategory.kt`
- `android/app/src/main/java/com/dxxredux/app/DebugLog.kt`
- `android/app/src/main/java/com/dxxredux/app/AdvancedSettingsPage.kt`
- `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt`
- `android/app/src/main/java/com/dxxredux/app/MainActivity.kt`

Plan:

- Add `DLOG_PROFILING` after `DLOG_LAUNCHER` and bump `DLOG_COUNT`
- Add `DebugLogCategory.PROFILING`, label `Profiling`, and include it in existing category UI loops
- Update setup introspection debug prefs to include `profiling_log_enabled`
- Update automation `write_probe_debug_prefs` only if we explicitly want probe mode to enable profiling. I would start with disabled there so existing graphics probes do not suddenly collect frame profiles
- Keep category default off
- Because `DebugLog.writeLine()` flushes after every line, add a profiling-specific batch path or deferred flush path. Otherwise the profiler measures slow Shield storage as part of the game thread

Preferred log sink shape:

- Native profiler accumulates lines for a sample window in a small fixed buffer
- At sample end, native sends a batch to Kotlin
- Kotlin writes each profiling line with the same timestamp/tag format but flushes once per batch
- If batching is too much for tranche 1, fall back to one `debug_log(DLOG_PROFILING, ...)` per sampled frame, but keep this as temporary because per-line flush can distort Shield results

## Native profiler helper

New files:

- `android/app/src/main/cpp/shared/android_profile.h`
- `android/app/src/main/cpp/shared/android_profile.c`

Build wiring:

- Add the helper to both D1 and D2 Android target source lists in `android/app/src/main/cpp/CMakeLists.txt`
- All public macros must compile away to no-ops on non-Android builds
- Every use in D1 and D2 code must be guarded by the helper macros, not by direct references to Android-only globals

Runtime model:

- Fast disabled path: one check of `debug_log_enabled[DLOG_PROFILING]`
- Clock: `clock_gettime(CLOCK_MONOTONIC)` in microseconds, reusing the current Android timing style in `d1/d2/arch/ogl/ogl.c`
- Fixed bucket enum and fixed label table, no allocation in a frame
- Per sampled frame:
  - non-overlapping top-level wicket marks
  - additive duration buckets for nested or repeated work
  - call counts, max call time, and total time per bucket
  - existing counters copied in at frame end, such as texture binds, polygons, shader switches, mask draws, GPU timer, swap time, resolve time, and GL error-drain time
- Per sample window:
  - aggregate totals, averages, max, and top buckets across all sampled frames
  - optional folded-stack lines for flame-graph generation

Sampling policy:

- Default period: 10000 ms
- Default active window: 1000 ms
- Start with a sample boundary line so logs can be correlated with adb, simpleperf, or Perfetto captures
- Add a JNI or debug-flag setter later for `window_ms`, `period_ms`, and `deep_mode`; do not build a large UI for tranche 1

## Log format

Use stable key-value text so PowerShell, Python, awk, or a spreadsheet can parse it. Avoid prose inside profiling lines.

Example frame line:

```text
prof_v=1 type=frame game=d2 sample=17 frame=12345 mode=game level=1 total_us=30142 wait_us=0 sim_us=4810 render_us=12654 flip_us=11922 swap_us=8031 gpu_us=22100 tex_load_us=0 io_us=0 ui_poll_us=180 ui_draw_us=0 binds=842 bind_reuse=611 polys=1843 shaders=12 masks=3 top=swap/8031/1,render_mine/6120/1,do_ai_frame_all/2100/1,render_gauges/1740/1
```

Example sample summary:

```text
prof_v=1 type=summary game=d2 sample=17 frames=25 total_avg_us=30280 total_p95_us=41800 total_max_us=51220 top=swap/181000/25,render_mine/154000/25,do_ai_frame_all/49000/25,ogl_bindbmtex/32000/1811 tex_load_count=2 tex_load_us=42000 io_read_us=33000
```

Example texture summary:

```text
prof_v=1 type=texture_sample sample=17 loads=2 total_us=42000 read_us=33000 decode_us=5100 upload_us=3900 pagein_us=0 top=door45#0/26000/ktx2_read,metl154/16000/png_decode
```

Optional folded stack lines for flame graph tools:

```text
prof_v=1 type=folded sample=17 stack=frame;render;render_frame;render_mine us=154000 calls=25
prof_v=1 type=folded sample=17 stack=frame;flip;eglSwapBuffers us=181000 calls=25
```

Notes:

- `total_us` is wall time for the game frame from the game-thread frame start to after `gr_flip()`
- `wait_us` is frame pacing and should not be blamed as engine work
- `swap_us` can be GPU or vsync blocking, not just CPU work
- `top` values are sorted descending by total bucket time inside the frame or sample
- Bucket values may be inclusive, so the parser should separate top-level non-overlapping wickets from nested blame buckets

## Native frame wickets

Top-level wickets should be mirrored in D1 and D2 with minimal edits.

Primary anchors:

- `d1/main/game.c` and `d2/main/game.c`, `game_handler(EVENT_WINDOW_DRAW)`
- `d1/main/game.c` and `d2/main/game.c`, `GameProcessFrame()`
- `d1/main/gamerend.c` and `d2/main/gamerend.c`, `game_render_frame()` and `game_render_frame_mono()`
- `d1/main/render.c` and `d2/main/render.c`, `render_frame()`, `render_mine()`, `do_render_object()`
- `d1/main/object.c` and `d2/main/object.c`, `object_move_all()`
- `d1/arch/ogl/ogl.c` and `d2/arch/ogl/ogl.c`, `ogl_start_frame()`, `ogl_end_frame()`, `gr_flip()`, texture functions
- `d1/arch/ogl/gr.c` and `d2/arch/ogl/gr.c`, `ogl_swap_buffers_internal()`

Frame-level wickets:

- `frame_total`: start of `EVENT_WINDOW_DRAW` to after render or replay step
- `frame_wait`: time spent inside frame pacing in `calc_frame_time()`
- `sim_total`: `calc_game_time()` plus `GameProcessFrame()`
- `render_total`: `game_render_frame()`
- `flip_total`: `gr_flip()`
- `swap_total`: actual `eglSwapBuffers()` boundary
- `gpu_frame`: existing EXT_disjoint_timer_query result when available

Simulation sub-buckets:

- `multi_do_frame`
- `object_move_all`
- `do_ai_frame_all`
- `digi_sync_sounds`
- `do_special_effects`
- `wall_frame_process`
- `triggers_frame_process`
- `FireLaser` and `do_laser_firing_player`
- `coop_autosave`
- `do_endlevel_frame`

Render sub-buckets:

- `render_frame`
- `g3_start_frame` and `g3_end_frame`
- `start_lighting_frame`
- `find_point_seg`
- `build_segment_list`
- `build_object_lists`
- `set_dynamic_light`
- `render_mine`
- `render_segment`
- `render_side`
- `render_face` or `g3_draw_tmap` if a clean local anchor exists
- `do_render_object`
- `render_object`
- `render_gauges`
- `game_draw_hud_stuff`
- `coop_indicator_lines_render`

OpenGL and flip sub-buckets:

- `ogl_start_frame`
- `ogl_start_pending_aniso`
- `ogl_start_pending_texfilt`
- `ogl_start_msaa_fbo`
- `glClear`
- `ogl_end_frame`
- `glGetError_drain`
- `ogl_do_palfx`
- `gpu_timer_end`
- `msaa_resolve`
- `fb_sample_readpixels`
- `keyboard_gap_fill`
- `glViewport_menu_offset`
- `egl_recreate_surface`
- `eglMakeCurrent`
- `eglCreateWindowSurface`
- `eglSwapBuffers`

Render deep mode:

- Default mode should avoid timing every face and every texture bind if it changes performance too much
- Deep mode can add `render_side`, `render_face`, `ogl_bindbmtex`, `glBindTexture`, `glTexParameteri`, and merged-wall helper buckets
- Deep mode should still be sampled and should be used only after coarse profiling says render is dominant

## Texture and storage wickets

Texture loading is a first-class part of this plan because Shield storage may be the root cause.

Anchors:

- `d1/arch/ogl/ogl.c` and `d2/arch/ogl/ogl.c`, `ogl_cache_level_textures()`
- `d1/arch/ogl/ogl.c` and `d2/arch/ogl/ogl.c`, `ogl_loadbmtexture_f()`
- `d1/arch/ogl/ogl.c` and `d2/arch/ogl/ogl.c`, `ogl_loadtexture()`
- `android/app/src/main/cpp/shared/pngfile_stb.c`, `read_png()` and `read_ktx2_file()`
- `d1/main/piggy.c` and `d2/main/piggy.c`, `piggy_bitmap_page_in()`
- `d1/main/paging.c` and `d2/main/paging.c`, `paging_touch_all()`
- `android/app/src/main/cpp/shared/physfs_archiver_saf.c`, `safio_read()`, `SAF_openRead()`, `saf_open_file()` path
- `android/app/src/main/cpp/shared/android_loading_progress.c`, progress JNI calls during cache pass

Cache and load buckets:

- `cache_level_textures_total`
- `cache_loop`
- `texture_load_total`
- `texture_ktx2_open_read`
- `texture_ktx2_parse`
- `texture_png_open_read`
- `texture_png_decode`
- `texture_upload_total`
- `texture_glCompressedTexImage2D`
- `texture_glTexImage2D`
- `texture_glGenerateMipmap`
- `texture_mask_load_upload`
- `piggy_page_in`
- `paging_touch_all`
- `loading_progress_jni`

Storage/syscall buckets:

- `saf_open_file_jni`
- `saf_pread`
- `saf_dup`
- `physfs_open_read`
- `physfs_file_length`
- `physfs_read_bytes`
- `physfs_seek`
- `asset_open`
- `asset_get_buffer`

Texture reporting rules:

- Do not log every texture as a line during cache. Aggregate totals and emit one summary line per cache pass or sample window
- Keep a fixed top-N slow texture table, likely 8 entries, storing texture name, path type, total time, read time, decode time, upload time, and result path such as ktx2, png, stock, mask
- Emit detailed per-texture lines only for textures over a threshold such as 10 ms, and only while `DLOG_PROFILING` is enabled
- Include runtime page-ins separately from startup cache pass, since runtime page-in stalls are the most likely way slow storage becomes bad gameplay

## Kotlin overlay wickets

The native frame line alone cannot prove whether Android views are stealing time. Kotlin/UI profiling should use the same category but its own line type because UI frames are not synchronized to game frames.

New helper:

- `android/app/src/main/java/com/dxxredux/app/ProfilingLog.kt` or a small object inside a new file

Suggested buckets:

- `main_overlay_poll_total`, from `MainActivity.startOverlayPolling()`
- individual expensive JNI calls inside that poller, grouped as `native_is_state_calls`
- `pollTrackLabel`
- `TouchOverlayView.onDraw`
- `TouchOverlayView.onTouchEvent`
- `VideoInfoOverlay.poll`
- `VideoInfoOverlay.onDraw`
- `CoopStatsOverlay.poll` and `onDraw`
- `MultiplayerStatsOverlay.poll` and `onDraw`
- `NetworkEventsOverlay.poll` and `onDraw`
- `LoadingProgressOverlayView.onDraw`
- `SkipButtonView`, `ExitButtonView`, `StartGameButtonView`, `AcceptJoinButtonView`, and `WarpButtonOverlay` draw cost if visible
- Optional `Choreographer` UI frame callback during sample windows for UI frame interval and missed-frame count

UI log format:

```text
prof_v=1 type=ui_summary sample=17 polls=10 poll_us=2140 draw_us=3320 ui_frames=60 ui_frame_max_us=19000 visible=touch,video top=TouchOverlayView.onDraw/2100/18,MainActivity.overlayPoll/2140/10,VideoInfoOverlay.poll/620/2
```

Rules:

- Kotlin profiling should use `SystemClock.elapsedRealtimeNanos()`
- Do not call into native for every UI bucket
- Emit one UI summary per sample window, with optional `ui_frame` lines only in deep mode
- Make the UI helper check `DebugLog.isCategoryEnabled(context, PROFILING)` or a cached atomic state so disabled overhead is very small

## External profiler correlation

The in-app profiler will not discover arbitrary functions that were not instrumented. If a sample shows a large unexplained bucket, use external sampling for the next capture.

Plan:

- Emit `type=sample_start` and `type=sample_end` lines with monotonic time and wall-clock time
- On debug builds, optionally call `ATrace_beginSection()` and `ATrace_endSection()` for the same major native and Kotlin buckets so Perfetto can show the same names
- Use simpleperf on Shield when available to sample native call stacks during a bad scene
- Use Perfetto or Android Studio System Trace to confirm UI thread, render thread, Binder, scheduler, and storage stalls if the in-app data points outside the game thread
- Treat external tracing as second-pass evidence. The first pass should be usable from the exportable debug log alone

## Parser and analysis tooling

Add a small parser after the first log format lands.

Suggested file:

- `android/profile-log-summary.ps1`

Output:

- total sampled frames and time
- worst frames by `total_us`
- average, p95, and max frame time
- top buckets by total time and percent of sampled wall time
- top buckets by max single call
- top texture loads by total, read, decode, and upload time
- top storage/syscall buckets
- UI summary totals and max frame interval
- optional folded-stack output file compatible with FlameGraph input

The parser should accept an exported debug log and write CSV summaries under `temp/` by default.

## Implementation phases

### Phase 1. Category and low-volume sink

- Add `Profiling` category in C and Kotlin
- Add the profiling batch or deferred-flush path
- Add sample start/end and no-op frame lines from the native helper
- Verify toggling from Advanced settings enables native category state in `MainActivity.syncDebugLogPrefs()`

### Phase 2. Coarse native frame profile

- Add frame begin/end around `EVENT_WINDOW_DRAW`
- Add top-level buckets for wait, sim, render, flip, swap, and GPU time
- Reuse existing OGL timings where present
- Emit one frame line during sample windows and one summary line at sample end

### Phase 3. Renderer and simulation blame buckets

- Add moderate-cost function buckets in `GameProcessFrame()`, `game_render_frame_mono()`, `render_frame()`, and `render_mine()`
- Add deep-mode hooks for high-frequency render paths only after coarse output proves render is dominant
- Include render counters already shown by `VideoInfoOverlay`

### Phase 4. Texture and storage profile

- Convert existing cache-stage totals into profiling-category summaries
- Add slow texture top-N table
- Split `read_png()` and `read_ktx2_file()` into open, read, decode or parse stages
- Add `piggy_bitmap_page_in()` and SAF `pread()` buckets
- Add runtime page-in markers so a single bad frame can show texture loading as the reason

### Phase 5. Kotlin overlay profile

- Add the Kotlin profiling helper
- Instrument the main overlay poller and visible overlay draw/poll methods
- Emit one `ui_summary` per sample window
- Add optional Choreographer UI frame timing only if overlay cost remains unclear

### Phase 6. Parser and capture workflow

- Add `android/profile-log-summary.ps1`
- Document a Shield capture flow in this plan after implementation
- Make parser output both human-readable console tables and CSV files under `temp/`

### Phase 7. Validation

- Run `android\stop-stale-formatters.ps1` before formatting or validation if another formatter may be active
- Run `android\run-code-quality.ps1 --fix` after code edits
- Build with `cd android; $env:JAVA_HOME='c:\local\jdk-21'; .\gradlew.bat :app:assembleDebug --console=plain`
- On Shield, enable only `Profiling`, reproduce the bad scene for at least 30 seconds, export the debug log, and run the parser
- If the top bucket is still too broad, enable deep mode for one short capture

## First likely findings to confirm or falsify

1. `eglSwapBuffers` or GPU timer dominates. This points to GPU load, vsync blocking, or driver stalls
2. `render_mine`, `render_segment`, merged-wall helpers, or texture binds dominate. This points to renderer CPU work
3. `texture_load_total`, `piggy_page_in`, `physfs_read_bytes`, or `saf_pread` appears inside bad frames. This points to storage or texture-cache behavior
4. `cache_level_textures_total` is huge but gameplay frames are normal. This points to startup/cache work, not steady-state rendering
5. `ui_summary` shows high UI draw or poll time. This points to Kotlin overlay cost or too many JNI polls
6. `frame_wait` dominates. This is frame pacing and should not be treated as a slowdown bug

## Guardrails

- Keep all new profiler code Android-only or no-op off Android
- Mirror D1 and D2 hooks where the same gameplay or renderer path exists
- Prefer shared helper code in `android/app/src/main/cpp/shared/` to reduce D1/D2 duplication
- Do not leave per-frame text logging in existing Graphics or Texture categories for this problem
- Do not emit per-texture or per-face lines by default
- Keep profiling off by default and cheap when disabled
- Treat `eglSwapBuffers` as a wait boundary, not automatically as CPU work
- Keep the first implementation focused on measurement. Fixes should come only after one Shield capture identifies the dominant bucket
