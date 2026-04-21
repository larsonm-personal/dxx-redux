# OGL shared-helper extraction phase 11

## Scope

This tranche removes a few remaining one- or two-use local wrapper helpers in
`d1/arch/ogl/ogl.c` and `d2/arch/ogl/ogl.c` by inlining their shared-helper
calls directly at the call sites.

In scope:

- inline the local wrappers for draw-order, face tracking, cover logging,
  snapshot finishing, and metl154 alpha-source logging
- remove the dead local wrappers after inlining
- keep D1 and D2 mirrored while preserving their pointlist-type differences

Out of scope:

- multi-call wrappers such as the palette-source or submit-context helpers
- any new shared helper API changes
- non-Android rendering behavior

## Required validation

1. `./android/run-code-quality.ps1 -Fix`
2. `./android/gradlew.bat :app:assembleDebug :app:testDebugUnitTest`
3. `./run-windows-build.ps1 -Target both -Preset x86-release -BuildType RelWithDebInfo`
4. `./android/diff_vs_upstream.ps1 -Top 20`

## Work items

- [x] Inline the targeted one- and two-use wrapper calls in D1 and D2
- [x] Remove the dead local wrappers from both `ogl.c` files
- [x] Keep the D1 and D2 Android-only paths mirrored after the inline changes
- [x] Run validation and record the result

## Notes

- Prefer small anchored patches in both `ogl.c` files
- D1 still needs explicit pointlist casts for shared helper calls
- Keep the cover and track-face metadata arguments unchanged when inlining

Validation results:

- `./android/run-code-quality.ps1 -Fix` passed
- `./android/gradlew.bat :app:assembleDebug :app:testDebugUnitTest` passed
- `./run-windows-build.ps1 -Target both -Preset x86-release -BuildType RelWithDebInfo` passed
- `./android/diff_vs_upstream.ps1 -Top 20` passed
- A temporary phase-11 regression broke the top-of-file include/preprocessor block; this was fixed by restoring the branch-specific `OGLES`/`ANDROID` include split before validation was finalized
- The earlier Android `GL/glew.h` failure and Windows `C1070` mismatch error are gone on the current tree
- No new warnings were introduced beyond the pre-existing `loadgl.h` macro redefinitions and `ogl_get_free_texture` return-path warning
- `diff_vs_upstream` reported `2254 50 d1/arch/ogl/ogl.c` and `2282 49 d2/arch/ogl/ogl.c`