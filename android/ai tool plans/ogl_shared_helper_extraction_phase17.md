# OGL shared-helper extraction phase 17

## Scope

This tranche extracts the duplicated cached-texmerge UV builder helper from
`d1/arch/ogl/ogl.c` and `d2/arch/ogl/ogl.c` into
`android/app/src/main/cpp/shared/merged_wall_debug.{h,c}`.

In scope:

- add a shared API for cached-texmerge UV generation
- replace local calls in D1 and D2 with the shared helper
- remove dead local UV-builder helper definitions from both `ogl.c` files

Out of scope:

- cache slot policy and cache entry lifecycle helpers
- cached texmerge render path logic or GL state changes
- behavior changes to orient mapping or UV axis handling

## Required validation

1. `./android/run-code-quality.ps1 -Fix`
2. `./android/gradlew.bat :app:assembleDebug :app:testDebugUnitTest`
3. `./run-windows-build.ps1 -Target both -Preset x86-release -BuildType RelWithDebInfo`
4. `./android/diff_vs_upstream.ps1 -Top 20`

## Work items

- [x] Add shared cached-texmerge UV-builder API/implementation
- [x] Replace D1 and D2 local call sites with shared helper calls
- [x] Remove dead local helper definitions in both `ogl.c` files
- [x] Run validation and record the result

## Notes

- Preserve existing orient switch behavior exactly
- Preserve the existing base-v inversion needed for GLES FBO orientation

## Result

- moved duplicated cached-texmerge UV builder into shared helper:
	- `android_merged_wall_cached_texmerge_build_uvs(...)`
	- added to `merged_wall_debug.h` and `merged_wall_debug.c`
- removed duplicated `ogl_android_texmerge_build_uvs(...)` from both
	`d1/arch/ogl/ogl.c` and `d2/arch/ogl/ogl.c`
- D1 and D2 cached texmerge UV setup now calls the shared helper directly
- validation passed:
	- `run-code-quality.ps1 -Fix`
	- Android `:app:assembleDebug :app:testDebugUnitTest`
	- `run-windows-build.ps1 -Target both -Preset x86-release -BuildType RelWithDebInfo`
	- `diff_vs_upstream.ps1 -Top 20`
- `diff_vs_upstream.ps1 -Top 20` reported:
	- `d1/arch/ogl/ogl.c`: `+2100 -50 total 2150`
	- `d2/arch/ogl/ogl.c`: `+2127 -49 total 2176`
	- summary totals: `+18221 -801` across `199` files
