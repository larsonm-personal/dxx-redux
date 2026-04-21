# OGL shared-helper extraction phase 5

## Scope

This tranche extracts another narrow duplicated helper cluster from
`d1/arch/ogl/ogl.c` and `d2/arch/ogl/ogl.c` into
`android/app/src/main/cpp/shared/merged_wall_debug.{h,c}`.

In scope:

- move the signed triangle-area helper into shared code
- move the metl154 UV-point capture helper into shared code
- move the metl154 source-log gate and palette/alpha source log helpers into shared code
- keep D1 and D2 call sites mirrored, with thin local wrappers where that keeps
  the surrounding diagnostic block stable

Out of scope:

- moving the larger metl154 submit, split, or diag log emitters
- changing any metl154 log payloads or runtime behavior
- renaming the remaining metl154 investigation surface

## Required validation

1. `./android/run-code-quality.ps1 -Fix`
2. `./android/gradlew.bat :app:assembleDebug :app:testDebugUnitTest`
3. `./android/diff_vs_upstream.ps1 -Top 20`

## Work items

- [x] Add shared declarations and implementations for the geometry and source-log helper cluster
- [x] Remove duplicated helper bodies from both `ogl.c` files
- [x] Keep D1 and D2 call sites mirrored after extraction
- [x] Run validation and record the result

## Notes

- Prefer small anchored patches in both `ogl.c` files
- Move the source-log mask state into shared code with the helper bodies that consume it
- Keep local wrappers where they avoid wider call-site churn in the texture loading paths
- Validation passed:
  1. `./android/run-code-quality.ps1 -Fix`
  2. `./android/gradlew.bat :app:assembleDebug :app:testDebugUnitTest`
  3. `./android/diff_vs_upstream.ps1 -Top 20`
- The shared `merged_wall_debug.c` UV helper uses `f2fl`, and the current Android build reports no `f2glf` or `merged_wall_debug.c` warnings
- The local `ogl_get_metl154_alpha_class` wrappers are gone from both `ogl.c` files, and the current Android build reports no warnings for that symbol
- Remaining Android build warnings are pre-existing groups outside this tranche, including legacy unused-function, enum-conversion, packed-member, and array-bounds warnings