# OGL shared-helper extraction phase 6

## Scope

This tranche extracts the duplicated metl154 submit and split diagnostic log
emitters from `d1/arch/ogl/ogl.c` and `d2/arch/ogl/ogl.c` into
`android/app/src/main/cpp/shared/merged_wall_debug.{h,c}`.

In scope:

- move the metl154 submit diagnostic log emitter into shared code
- move the metl154 split diagnostic log emitter into shared code
- remove the now-dead local triangle-area wrappers from both `ogl.c` files
- keep D1 and D2 call sites mirrored, with thin local wrappers where that keeps
  the surrounding diagnostic block stable

Out of scope:

- moving the larger metl154 diag log emitter
- changing any metl154 log payloads or runtime behavior
- renaming the remaining metl154 investigation surface

## Required validation

1. `./android/run-code-quality.ps1 -Fix`
2. `./android/gradlew.bat :app:assembleDebug :app:testDebugUnitTest`
3. `./android/diff_vs_upstream.ps1 -Top 20`

## Work items

- [x] Add shared declarations and implementations for the submit/split log helpers
- [x] Remove duplicated helper bodies from both `ogl.c` files
- [x] Keep D1 and D2 call sites mirrored after extraction
- [x] Run validation and record the result

## Notes

- Prefer small anchored patches in both `ogl.c` files
- Pass the existing `merged_wall_tmap2_submit_context` into shared code instead of re-encoding route/orig metadata at each call site
- Remove any dead local helper wrappers created by the extraction in the same tranche
- Validation passed:
  1. `./android/run-code-quality.ps1 -Fix`
  2. `./android/gradlew.bat :app:assembleDebug :app:testDebugUnitTest`
  3. `./run-windows-build.ps1 -Target both -Preset x86-release -BuildType RelWithDebInfo`
  4. `./android/diff_vs_upstream.ps1 -Top 20`
- The Android build warning set remains pre-existing and outside this tranche, including `game_introspect.cpp`, `jni_fingerprint.c`, third-party `lzma_sdk`, and assorted legacy D1/D2 warnings
- The local `ogl_get_metl154_triangle_area` wrappers are gone from both `ogl.c` files, and the submit/split emitters now call shared code through thin local wrappers