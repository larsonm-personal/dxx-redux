# OGL shared-helper extraction phase 1

## Scope

This tranche extracts a narrow set of duplicated merged-wall OGL helpers from
`d1/arch/ogl/ogl.c` and `d2/arch/ogl/ogl.c` into
`android/app/src/main/cpp/shared/merged_wall_debug.{h,c}`.

In scope:

- move the shared `merged_wall_tmap2_submit_context` struct definition into the
  shared header
- move the reset/set helpers for that context into shared code
- move the pure point/code summary helpers into shared code
- move the pure screen-area helper into shared code
- keep D1 and D2 call sites mirrored, with only signature-cast differences
  where required

Out of scope:

- moving the metl154-named diagnostic/logging helpers
- changing route selection or merged-wall behavior
- changing snapshot, probe, or cover logging payloads
- broad renaming of the remaining metl154 investigation surface

## Required validation

1. `./android/run-code-quality.ps1 -Fix`
2. `./android/gradlew.bat :app:assembleDebug :app:testDebugUnitTest`
3. `./android/diff_vs_upstream.ps1 -Top 20`

## Work items

- [x] Add shared declarations/definitions to `merged_wall_debug.{h,c}`
- [x] Remove duplicated helper bodies from both `ogl.c` files
- [x] Keep D1 and D2 call sites mirrored after extraction
- [x] Run validation and record the result

## Notes

- Prefer small anchored patches in both `ogl.c` files
- Avoid touching the large metl154 diagnostic block in this phase
- If the shared helper signature needs const-correctness, use the shared
  version as the stricter type and cast at the D1 call site only
- Kept the existing local helper names in `d1/arch/ogl/ogl.c` and
  `d2/arch/ogl/ogl.c` as thin wrappers so the draw-path call sites stayed
  stable in this phase while the duplicated logic moved into shared code
- Validation passed:
  1. `./android/run-code-quality.ps1 -Fix`
  2. `./android/gradlew.bat :app:assembleDebug :app:testDebugUnitTest`
  3. `./android/diff_vs_upstream.ps1 -Top 20`
- The Android unit-test build still reports the pre-existing
  `game_introspect.cpp` unused-function warnings during native compilation