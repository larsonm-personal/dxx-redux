# OGL shared-helper extraction phase 8

## Scope

This tranche extracts the duplicated metl154 draw-state log emitter from
`d1/arch/ogl/ogl.c` and `d2/arch/ogl/ogl.c` into
`android/app/src/main/cpp/shared/merged_wall_debug.{h,c}`.

In scope:

- move the metl154 state log emitter into shared code
- keep the GL-state gathering wrappers local for now
- keep the `g_merged_wall_last_draw_state` update behavior unchanged
- keep D1 and D2 call sites mirrored with a thin local wrapper

Out of scope:

- moving the secondary-unit experiment helpers
- changing the state log payload or snapshot semantics
- renaming the remaining metl154 investigation surface

## Required validation

1. `./android/run-code-quality.ps1 -Fix`
2. `./android/gradlew.bat :app:assembleDebug :app:testDebugUnitTest`
3. `./run-windows-build.ps1 -Target both -Preset x86-release -BuildType RelWithDebInfo`
4. `./android/diff_vs_upstream.ps1 -Top 20`

## Work items

- [x] Add a shared declaration and implementation for the state log emitter
- [x] Replace duplicated D1/D2 state logger bodies with wrappers
- [x] Keep D1 and D2 call sites mirrored after extraction
- [x] Run validation and record the result

## Notes

- Prefer small anchored patches in both `ogl.c` files
- Reuse the existing submit-context route string instead of duplicating route tracking
- Leave GL-state sampling local to avoid widening this tranche beyond a pure log-emitter move

Validation results:

- `./android/run-code-quality.ps1 -Fix` passed
- `./android/gradlew.bat :app:assembleDebug :app:testDebugUnitTest` passed
- `./run-windows-build.ps1 -Target both -Preset x86-release -BuildType RelWithDebInfo` passed
- `./android/diff_vs_upstream.ps1 -Top 20` passed
- No new warnings were introduced by the state logger extraction
- `merged_wall_debug.c` remained warning-free in the Android build
- Remaining Windows warnings were the pre-existing `loadgl.h` macro redefinitions and `ogl_get_free_texture` return-path warning