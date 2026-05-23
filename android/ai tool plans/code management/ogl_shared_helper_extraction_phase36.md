# OGL Shared Helper Extraction Phase 36

## Goal

Relocate the remaining Android cached texmerge table and clear/reuse/reserve
wrapper plumbing from D1 and D2 `arch/ogl/ogl.c` into shared native code.

## Scope

- android/app/src/main/cpp/shared/merged_wall_debug.h
- android/app/src/main/cpp/shared/merged_wall_debug.c
- d1/arch/ogl/ogl.c
- d2/arch/ogl/ogl.c

## Work items

- [x] Add shared cache-owned texmerge clear/reuse/reserve wrappers
- [x] Replace duplicated D1 texmerge cache storage and wrapper threading
- [x] Replace duplicated D2 texmerge cache storage and wrapper threading
- [x] Run validation and record the result

## Guardrails

- Keep D1 and D2 mirrored for the cache relocation work
- Keep the cache entry storage in shared code, but continue treating `ogl_texture *`
  as opaque there except through existing shared helpers
- Preserve the existing cache reuse semantics, slot eviction path, and texture-size selection
- Do not widen this tranche into the ETC2/KTX2 upload path

## Result

- added cache-owned Android texmerge clear/reuse/reserve wrappers to
  `shared/merged_wall_debug.{h,c}` so the shared file now owns the remaining
  cached-texmerge table storage instead of threading it through both `ogl.c`
  files
- removed the duplicated D1 and D2 cached-texmerge table/reset helpers and
  tightened each `ogl_android_get_cached_plain_texmerge_bitmap(...)` wrapper to
  call the shared cache-owned entry points directly
- repaired the intermediate D2 patch drift by restoring the validated
  `ogl_bindbmtex()` / texture-stats / anisotropy / MSAA helper structure before
  reapplying the shared-cache wrapper conversion
- validation passed:
  - `android\\gradlew.bat :app:assembleDebug :app:testDebugUnitTest --console=plain`
- phase-36 tracked file churn vs `upstream/main`:
  - `android/app/src/main/cpp/shared/merged_wall_debug.c`: `+6197 -0`
  - `android/app/src/main/cpp/shared/merged_wall_debug.h`: `+253 -0`
  - `d1/arch/ogl/ogl.c`: `+1508 -50`
  - `d2/arch/ogl/ogl.c`: `+1590 -49`
- this completes the planned phase-2 `ogl.c` shrink tranches through phase 36