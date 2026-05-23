# OGL shared-helper extraction phase 3

## Scope

This tranche extracts a narrow set of duplicated merged-wall source-sampling
helpers from `d1/arch/ogl/ogl.c` and `d2/arch/ogl/ogl.c` into
`android/app/src/main/cpp/shared/merged_wall_debug.{h,c}`.

In scope:

- move source palette-count sampling into shared code
- move source alpha-class and alpha-value helpers into shared code
- move average source UV sampling into shared code
- move bilinear source-filter sampling into shared code
- move source vertical-slice sampling into shared code
- keep D1 and D2 call sites mirrored, with thin local wrappers if that keeps
  the diagnostic block stable

Out of scope:

- moving the GL-state inspection helpers
- moving the larger metl154 diagnostic log payload builder
- changing any log payloads or render behavior

## Required validation

1. `./android/run-code-quality.ps1 -Fix`
2. `./android/gradlew.bat :app:assembleDebug :app:testDebugUnitTest`
3. `./android/diff_vs_upstream.ps1 -Top 20`

## Work items

- [x] Add shared declarations and implementations for the source-sampling helper cluster
- [x] Remove duplicated helper bodies from both `ogl.c` files
- [x] Keep D1 and D2 call sites mirrored after extraction
- [x] Run validation and record the result

## Notes

- Prefer small anchored patches in both `ogl.c` files
- Reuse the existing shared `merged_wall_get_source_bitmap()` helper instead of creating another source-bitmap implementation
- Keep local OGL wrappers only where they avoid a large call-site rewrite inside the diagnostic block
- Validation passed:
  1. `./android/run-code-quality.ps1 -Fix`
  2. `./android/gradlew.bat :app:assembleDebug :app:testDebugUnitTest`
  3. `./android/diff_vs_upstream.ps1 -Top 20`
- Follow-up cleanup removed the now-unused local `ogl_get_metl154_alpha_value` wrappers from both `ogl.c` files, and the Android build no longer reports that warning
- The final validation rerun only reported the existing Git CRLF line-ending warnings during the diff script