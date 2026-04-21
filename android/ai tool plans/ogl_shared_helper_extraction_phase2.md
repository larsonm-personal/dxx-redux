# OGL shared-helper extraction phase 2

## Scope

This tranche extracts another narrow set of duplicated merged-wall OGL helpers
from `d1/arch/ogl/ogl.c` and `d2/arch/ogl/ogl.c` into
`android/app/src/main/cpp/shared/merged_wall_debug.{h,c}`.

In scope:

- export the shared merged-wall experiment-name helper
- move the duplicated tmap2 route logging helper into shared code
- move the duplicated upload logging helper into shared code
- remove the matching local helper bodies from both `ogl.c` files
- keep D1 and D2 call sites mirrored

Out of scope:

- moving the larger metl154 diagnostic helper family
- changing log payload formats or draw behavior
- broad renaming of the remaining metl154 investigation surface

## Required validation

1. `./android/run-code-quality.ps1 -Fix`
2. `./android/gradlew.bat :app:assembleDebug :app:testDebugUnitTest`
3. `./android/diff_vs_upstream.ps1 -Top 20`

## Work items

- [x] Add shared declarations and implementations for the experiment-name and log helpers
- [x] Remove duplicated local helper bodies from both `ogl.c` files
- [x] Keep D1 and D2 call sites mirrored after extraction
- [x] Run validation and record the result

## Notes

- Prefer small anchored patches in both `ogl.c` files
- Keep the existing metl154 log strings unchanged in this phase
- Moved the upload sequence counter into shared code because `upload_id` was only consumed by the upload logger and submit context
- Validation passed:
	1. `./android/run-code-quality.ps1 -Fix`
	2. `./android/gradlew.bat :app:assembleDebug :app:testDebugUnitTest`
	3. `./android/diff_vs_upstream.ps1 -Top 20`
- The Android unit-test build still reports the pre-existing `game_introspect.cpp` unused-function warnings during native compilation