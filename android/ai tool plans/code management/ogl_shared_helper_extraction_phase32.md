# OGL Shared Helper Extraction Phase 32

## Goal

Extract the duplicated Android DXA mask loader and the two small Android-only
logging helpers from D1 and D2 `arch/ogl/ogl.c` into shared native code.

## Scope

- android/app/src/main/cpp/shared/ogl_texture_android.h
- android/app/src/main/cpp/shared/ogl_texture_android.c
- android/app/src/main/cpp/shared/android_texture_debug.h
- android/app/src/main/cpp/shared/android_texture_debug.c
- android/app/src/main/cpp/shared/merged_wall_debug.h
- android/app/src/main/cpp/shared/merged_wall_debug.c
- d1/arch/ogl/ogl.c
- d2/arch/ogl/ogl.c

## Work items

- [x] Add shared Android DXA mask helper
- [x] Add shared Android texture-bind logging helper
- [x] Add shared merged-wall experiment-apply logging helper
- [x] Replace duplicated D1 helper bodies/call sites with shared calls
- [x] Replace duplicated D2 helper bodies/call sites with shared calls
- [x] Run validation and record the result

## Guardrails

- Keep D1 and D2 mirrored
- Do not widen the shared OGL boundary beyond public headers or explicit local
  declarations
- Preserve existing Android logging text and DXA mask behavior
- Avoid touching the larger KTX2/ETC2 upload path in this tranche

## Result

- finished the remaining phase-32 helper wiring from the restored phase-31
  baseline instead of reopening the earlier recovery work
- used the already-present shared DXA helper in
  `shared/ogl_texture_android.{h,c}` and replaced the duplicated local
  `ogl_load_dxa_mask(...)` helper bodies / Android call sites in both games
- replaced the duplicated Android merged-wall experiment-apply block in both
  `ogl_start_frame()` implementations with
  `android_merged_wall_consume_experiment_pending_apply()`
- added shared helper `android_texture_debug_log_render_bind(...)` in
  `shared/android_texture_debug.{h,c}` and replaced the duplicated Android
  `r_etc2_render_log_count` bind-log block in both `g3_draw_tmap()` paths
- validation passed:
  - `./android/run-code-quality.ps1 -Fix`
  - Android `./android/gradlew.bat :app:assembleDebug :app:testDebugUnitTest --console=plain`
  - Windows `./run-windows-build.ps1 -Target both -Preset x86-release -BuildType RelWithDebInfo`
  - `git diff --numstat upstream/main` on the phase-32 scope files
- phase-32 file-specific churn lines:
  - `android/app/src/main/cpp/shared/android_texture_debug.c`: `+451 -0`
  - `android/app/src/main/cpp/shared/android_texture_debug.h`: `+53 -0`
  - `android/app/src/main/cpp/shared/ogl_texture_android.c`: `+195 -0`
  - `android/app/src/main/cpp/shared/ogl_texture_android.h`: `+49 -0`
  - `d1/arch/ogl/ogl.c`: `+1614 -50`
  - `d2/arch/ogl/ogl.c`: `+1696 -49`