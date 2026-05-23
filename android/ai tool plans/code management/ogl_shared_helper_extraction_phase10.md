# OGL shared-helper extraction phase 10

## Scope

This tranche removes the remaining single-use local metl154 wrapper helpers in
`d1/arch/ogl/ogl.c` and `d2/arch/ogl/ogl.c` by inlining their shared-helper
calls at the only call sites.

In scope:

- inline one-shot metl154 wrapper calls in the shared draw path
- remove the dead local helper wrappers after inlining
- keep D1 and D2 mirrored while respecting their existing pointer-type differences
- preserve behavior, log payloads, and conditional Android-only paths

Out of scope:

- changing shared helper APIs
- touching the multi-call palette or alpha-source wrappers
- changing non-Android render behavior

## Required validation

1. `./android/run-code-quality.ps1 -Fix`
2. `./android/gradlew.bat :app:assembleDebug :app:testDebugUnitTest`
3. `./run-windows-build.ps1 -Target both -Preset x86-release -BuildType RelWithDebInfo`
4. `./android/diff_vs_upstream.ps1 -Top 20`

## Work items

- [x] Inline the single-use metl154 wrappers at their call sites in D1 and D2
- [x] Remove the dead local helper wrappers from both `ogl.c` files
- [x] Keep the D1 and D2 Android merge paths mirrored after the inline changes
- [x] Run validation and record the result

## Notes

- Prefer small anchored patches in both `ogl.c` files
- Keep the wid-flag predicate behavior identical when inlining the single-clip guard
- D1 still needs explicit casts for the shared helper pointlist parameters

Validation results:

- `./android/run-code-quality.ps1 -Fix` passed
- `./android/gradlew.bat :app:assembleDebug :app:testDebugUnitTest` passed
- `./run-windows-build.ps1 -Target both -Preset x86-release -BuildType RelWithDebInfo` passed
- `./android/diff_vs_upstream.ps1 -Top 20` passed
- No new warnings were introduced by this tranche
- `merged_wall_debug.c` remained warning-free
- Remaining Windows warnings were the pre-existing `loadgl.h` macro redefinitions and `ogl_get_free_texture` return-path warning
- `diff_vs_upstream` reported `2288 50 d1/arch/ogl/ogl.c` and `2318 49 d2/arch/ogl/ogl.c`