# OGL shared-helper extraction phase 19

## Scope

This tranche extracts the duplicated cached-texmerge cache-clear helper from
`d1/arch/ogl/ogl.c` and `d2/arch/ogl/ogl.c` into
`android/app/src/main/cpp/shared/merged_wall_debug.{h,c}`.

In scope:

- add a shared helper that clears an array of cached-texmerge entries
- replace D1 and D2 local cache-clear loop bodies with shared helper calls
- keep cache initialization behavior unchanged in both games

Out of scope:

- cache slot selection policy
- cached-texmerge rendering/upload logic
- moving the entire cached-texmerge function out of `ogl.c`

## Required validation

1. `./android/run-code-quality.ps1 -Fix`
2. `./android/gradlew.bat :app:assembleDebug :app:testDebugUnitTest`
3. `./run-windows-build.ps1 -Target both -Preset x86-release -BuildType RelWithDebInfo`
4. `./android/diff_vs_upstream.ps1 -Top 20`

## Work items

- [x] Add shared cached-texmerge cache-clear API/implementation
- [x] Replace D1 and D2 local cache-clear loop bodies with shared helper calls
- [x] Keep D1 and D2 cache-init behavior mirrored after the replacement
- [x] Run validation and record the result

## Notes

- Preserve the existing `ogl_android_texmerge_cache_initialized = 1` behavior
- Preserve `OGL_ANDROID_TEXMERGE_CACHE_SIZE` usage at call sites

## Result

- added shared cache-clear helper:
	- `android_merged_wall_cached_texmerge_clear(...)`
- replaced duplicated local cache-clear loop bodies in both `ogl.c` files with
	shared helper calls
- preserved existing `ogl_android_texmerge_cache_initialized = 1` behavior in
	both games
- validation passed:
	- `run-code-quality.ps1 -Fix`
	- Android `:app:assembleDebug :app:testDebugUnitTest`
	- `run-windows-build.ps1 -Target both -Preset x86-release -BuildType RelWithDebInfo`
	- `diff_vs_upstream.ps1 -Top 20`
- latest churn lines:
	- `d1/arch/ogl/ogl.c`: `+2075 -50 total 2125`
	- `d2/arch/ogl/ogl.c`: `+2102 -49 total 2151`
	- overall totals: `+18171 -801` across `199` files
