# OGL shared-helper extraction phase 9

## Scope

This tranche extracts the duplicated metl154 secondary-unit single-clear
experiment helpers from `d1/arch/ogl/ogl.c` and `d2/arch/ogl/ogl.c` into
`android/app/src/main/cpp/shared/merged_wall_debug.{h,c}`.

In scope:

- move the single-clear experiment logic into shared code
- remove the now-local-only draw-state wrapper if this tranche makes it dead
- keep the D1 and D2 call sites mirrored with a thin local wrapper
- preserve the existing debug log payload and GL texture-unit reset behavior

Out of scope:

- changing experiment semantics or log fields
- moving unrelated metl154 helpers
- changing the actual draw path beyond this targeted experiment hook

## Required validation

1. `./android/run-code-quality.ps1 -Fix`
2. `./android/gradlew.bat :app:assembleDebug :app:testDebugUnitTest`
3. `./run-windows-build.ps1 -Target both -Preset x86-release -BuildType RelWithDebInfo`
4. `./android/diff_vs_upstream.ps1 -Top 20`

## Work items

- [x] Add a shared declaration and implementation for the single-clear helper
- [x] Replace duplicated D1/D2 single-clear helpers with wrappers
- [x] Remove any wrapper that becomes dead in both `ogl.c` files
- [x] Run validation and record the result

## Notes

- Prefer small anchored patches in both `ogl.c` files
- Keep the face-context gating logic behavior identical
- Call the shared draw-state helper directly from shared code instead of adding another exported wrapper

Validation results:

- `./android/run-code-quality.ps1 -Fix` passed
- `./android/gradlew.bat :app:assembleDebug :app:testDebugUnitTest` passed
- `./run-windows-build.ps1 -Target both -Preset x86-release -BuildType RelWithDebInfo` passed
- live-tree `git diff --numstat upstream/main -- d1/arch/ogl/ogl.c d2/arch/ogl/ogl.c` reported `2355 50 d1/arch/ogl/ogl.c` and `2385 49 d2/arch/ogl/ogl.c`
- No warnings were reported for `merged_wall_debug.c` in the Android build
- Remaining Windows warnings were the pre-existing `loadgl.h` macro redefinitions and `ogl_get_free_texture` return-path warning
- A stale terminal snapshot briefly showed the earlier phase-8 churn totals, so the final validation above was rechecked against the live tree before closing this tranche