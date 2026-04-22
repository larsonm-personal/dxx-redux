# OGL Shared Helper Extraction Phase 33

## Goal

Extract the duplicated Android TexFilt live-apply loop from D1 and D2
`arch/ogl/ogl.c` into shared native code.

## Scope

- android/app/src/main/cpp/shared/ogl_texture_android.h
- android/app/src/main/cpp/shared/ogl_texture_android.c
- d1/arch/ogl/ogl.c
- d2/arch/ogl/ogl.c

## Work items

- [x] Confirm the shared Android TexFilt live-apply state and helper surface
- [x] Wire the D1 TexFilt live-apply path to the shared call
- [x] Wire the D2 TexFilt live-apply path to the shared call
- [x] Run validation and record the result

## Guardrails

- Keep D1 and D2 mirrored
- Reuse the existing shared texture-list helper surface instead of creating a
  new OGL subsystem
- Preserve in-place mip generation, font/NOCOLOR skip, and bind-cache reset
- Keep the helper Android-only and limited to the texture-list iteration

## Result

- phase 33 started from a branch state where the shared TexFilt helper already
  existed in `shared/ogl_texture_android.{h,c}`, but neither `ogl_start_frame()`
  path consumed `g_texfilt_pending_apply`
- added the missing `ogl_texture_android.h` include in D2 so both games can call
  the shared helper through the same surface
- wired both `ogl_start_frame()` implementations to call
  `android_ogl_apply_texfilt_all(...)` when `g_texfilt_pending_apply` is set,
  using `g_texfilt_level` as the requested overlay value and `GameCfg.TexFilt`
  as the applied runtime config value
- validation passed:
  - `android\\gradlew.bat :app:assembleDebug :app:testDebugUnitTest --console=plain`
- phase-33 file-specific churn lines:
  - `android/app/src/main/cpp/shared/ogl_texture_android.c`: `+195 -0`
  - `android/app/src/main/cpp/shared/ogl_texture_android.h`: `+49 -0`
  - `d1/arch/ogl/ogl.c`: `+1625 -50`
  - `d2/arch/ogl/ogl.c`: `+1708 -49`