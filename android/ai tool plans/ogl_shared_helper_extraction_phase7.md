# OGL shared-helper extraction phase 7

## Scope

This tranche extracts the duplicated metl154 diagnostic log emitter from
`d1/arch/ogl/ogl.c` and `d2/arch/ogl/ogl.c` into
`android/app/src/main/cpp/shared/merged_wall_debug.{h,c}`.

In scope:

- move the metl154 diag log emitter into shared code
- move the local first-handle tracking state with that emitter
- remove the now-dead local diag-only prep wrappers and local constants from both `ogl.c` files
- keep D1 and D2 call sites mirrored, with a thin wrapper if that keeps the draw path stable

Out of scope:

- changing the metl154 log payloads or runtime behavior
- moving unrelated state logging or draw-face snapshot code
- renaming the remaining metl154 investigation surface

## Required validation

1. `./android/run-code-quality.ps1 -Fix`
2. `./android/gradlew.bat :app:assembleDebug :app:testDebugUnitTest`
3. `./run-windows-build.ps1 -Target both -Preset x86-release -BuildType RelWithDebInfo`
4. `./android/diff_vs_upstream.ps1 -Top 20`

## Work items

- [x] Add a shared declaration and implementation for the diag log emitter
- [x] Remove duplicated diag-only helper bodies, constants, and state from both `ogl.c` files
- [x] Keep D1 and D2 call sites mirrored after extraction
- [x] Run validation and record the result

## Notes

- Prefer small anchored patches in both `ogl.c` files
- Keep only the wrappers that are still needed by other logging paths after the diag emitter moves
- Pass dynamic render inputs explicitly instead of widening shared dependencies unless the dependency is already present in the shared include surface
- Validation passed:
	1. `./android/run-code-quality.ps1 -Fix`
	2. `./android/gradlew.bat :app:assembleDebug :app:testDebugUnitTest`
	3. `./run-windows-build.ps1 -Target both -Preset x86-release -BuildType RelWithDebInfo`
	4. `./android/diff_vs_upstream.ps1 -Top 20`
- The `ogl_get_metl154_source_filter_sample` and `ogl_get_metl154_filter_state` warnings introduced during the first phase-7 pass were removed and are now gone from the Android build output
- The current Android build reports no warnings for `merged_wall_debug.c`
- Remaining warnings are outside this tranche, including the long-standing `loadgl.h` macro redefinition noise and the existing `ogl_get_free_texture` return-path warning