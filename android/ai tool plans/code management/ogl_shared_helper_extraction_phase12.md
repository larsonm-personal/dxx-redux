# OGL shared-helper extraction phase 12

## Scope

This tranche removes the localized submit-context wrapper helpers in
`d1/arch/ogl/ogl.c` and `d2/arch/ogl/ogl.c` by inlining their shared-helper
calls at the clipped-submit call sites.

In scope:

- inline the local reset/set/input-code/point-summary wrappers in the
  `g3_draw_tmap_2` and clipped-submit path
- remove the dead local submit-context wrappers after inlining
- keep D1 and D2 mirrored while preserving their pointlist-type differences

Out of scope:

- the higher-fanout logging-target wrapper
- the multi-call palette-source wrapper
- any shared helper API changes

## Required validation

1. `./android/run-code-quality.ps1 -Fix`
2. `./android/gradlew.bat :app:assembleDebug :app:testDebugUnitTest`
3. `./run-windows-build.ps1 -Target both -Preset x86-release -BuildType RelWithDebInfo`
4. `./android/diff_vs_upstream.ps1 -Top 20`

## Work items

- [x] Inline the localized submit-context wrapper calls in D1 and D2
- [x] Remove the dead local submit-context wrappers from both `ogl.c` files
- [x] Keep the D1 and D2 Android merge paths mirrored after the inline changes
- [x] Run validation and record the result

## Notes

- Prefer small anchored patches in both `ogl.c` files
- D1 still needs explicit pointlist casts when calling shared helpers
- Keep the existing route, input-behind, temp-point, and clip-applied values unchanged

Validation results:

- `./android/run-code-quality.ps1 -Fix` passed
- `./android/gradlew.bat :app:assembleDebug :app:testDebugUnitTest` passed
- `./run-windows-build.ps1 -Target both -Preset x86-release -BuildType RelWithDebInfo` passed
- `./android/diff_vs_upstream.ps1 -Top 20` passed
- The Android and Windows pass set stayed intact after inlining the submit-context helpers
- No new warnings were introduced beyond the pre-existing `loadgl.h` macro redefinitions and `ogl_get_free_texture` return-path warning
- `diff_vs_upstream` reported `2229 50 d1/arch/ogl/ogl.c` and `2256 49 d2/arch/ogl/ogl.c`