# OGL shared-helper extraction phase 4

## Scope

This tranche extracts the duplicated merged-wall GL-state inspection helpers
from `d1/arch/ogl/ogl.c` and `d2/arch/ogl/ogl.c` into
`android/app/src/main/cpp/shared/merged_wall_debug.{h,c}`.

In scope:

- move the texture-filter inspection helper into shared code
- move the draw-state inspection helper into shared code
- move the general GL-state snapshot helper into shared code
- keep D1 and D2 call sites mirrored, with thin local wrappers if that keeps
  the diagnostic block stable

Out of scope:

- moving the larger metl154 diagnostic log payload builder
- changing any log payloads or render behavior
- renaming the remaining metl154 investigation surface

## Required validation

1. `./android/run-code-quality.ps1 -Fix`
2. `./android/gradlew.bat :app:assembleDebug :app:testDebugUnitTest`
3. `./android/diff_vs_upstream.ps1 -Top 20`

## Work items

- [x] Add shared declarations and implementations for the GL-state helper cluster
- [x] Remove duplicated helper bodies from both `ogl.c` files
- [x] Keep D1 and D2 call sites mirrored after extraction
- [x] Run validation and record the result

## Notes

- Prefer small anchored patches in both `ogl.c` files
- Keep the shared header lightweight by using a forward declaration for the texture struct
- Keep local wrappers only where they avoid a larger call-site rewrite inside the diagnostic block
- Validation passed:
  1. `./android/run-code-quality.ps1 -Fix`
  2. `./android/gradlew.bat :app:assembleDebug :app:testDebugUnitTest`
  3. `./android/diff_vs_upstream.ps1 -Top 20`
- The build still reports the pre-existing unused helper warnings in `android/app/src/main/cpp/shared/game_introspect.cpp`
- The current working tree still reflects only the intended phase-4 files after validation