# OGL Shared Helper Extraction Phase 35

## Goal

Extract the duplicated Android framebuffer snapshot sampler from D1 and D2
`arch/ogl/ogl.c` into shared native code.

## Scope

- android/app/src/main/cpp/shared/merged_wall_debug.h
- android/app/src/main/cpp/shared/merged_wall_debug.c
- d1/arch/ogl/ogl.c
- d2/arch/ogl/ogl.c

## Work items

- [x] Add shared Android framebuffer snapshot sampler helper
- [x] Replace duplicated D1 framebuffer snapshot block with a shared call
- [x] Replace duplicated D2 framebuffer snapshot block with a shared call
- [x] Run validation and record the result

## Guardrails

- Keep D1 and D2 mirrored for the extracted snapshot logic
- Preserve the center-pixel sample, 4x4 grid average, and existing snapshot-finish call
- Keep the helper Android-only and limited to framebuffer readback plus the shared snapshot handoff
- Do not change the exported framebuffer sample globals or the snapshot result format

## Result

- added `android_merged_wall_sample_snapshot_framebuffer(...)` to
  `shared/merged_wall_debug.{h,c}` so the framebuffer readback and snapshot-finish
  handoff live in one shared Android helper
- replaced the duplicated D1 and D2 `gr_flip()` framebuffer sampling blocks with a
  single shared call that passes the existing exported sample and average globals
- kept the old behavior where `android_merged_wall_finish_snapshot(...)` still runs
  even when the framebuffer dimensions are not positive, using the current stored
  sample values
- validation passed:
  - `android\\gradlew.bat :app:assembleDebug :app:testDebugUnitTest --console=plain`
- phase-35 tracked file churn vs `upstream/main`:
  - `android/app/src/main/cpp/shared/merged_wall_debug.c`: `+6162 -0`
  - `android/app/src/main/cpp/shared/merged_wall_debug.h`: `+246 -0`
  - `d1/arch/ogl/ogl.c`: `+1586 -50`
  - `d2/arch/ogl/ogl.c`: `+1668 -49`