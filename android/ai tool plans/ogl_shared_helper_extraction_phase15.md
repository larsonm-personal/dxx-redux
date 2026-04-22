# OGL shared-helper extraction phase 15

## Scope

This tranche extracts the duplicated `ogl_android_texmerge_log` helper from
`d1/arch/ogl/ogl.c` and `d2/arch/ogl/ogl.c` into
`android/app/src/main/cpp/shared/merged_wall_debug.{h,c}`.

In scope:

- add a shared cached-texmerge log helper API in `merged_wall_debug`
- replace local helper calls in D1 and D2 with the shared helper
- remove the dead local helper definition from both `ogl.c` files

Out of scope:

- texmerge cache entry lifecycle helpers (`reset_entry`, `cache_clear`)
- UV construction or cached draw path refactors
- any behavior change to logging masks, fields, or route labels

## Required validation

1. `./android/run-code-quality.ps1 -Fix`
2. `./android/gradlew.bat :app:assembleDebug :app:testDebugUnitTest`
3. `./run-windows-build.ps1 -Target both -Preset x86-release -BuildType RelWithDebInfo`
4. `./android/diff_vs_upstream.ps1 -Top 20`

## Work items

- [x] Add shared cached-texmerge log API/implementation
- [x] Replace D1 and D2 local call sites with shared helper calls
- [x] Remove dead local helper definitions in both `ogl.c` files
- [x] Run validation and record the result

## Notes

- Keep log message format and fields byte-for-byte equivalent
- Preserve existing event values (`reuse`, `create`) and route tags

## Result

- moved cached texmerge logging into shared helper:
	- `android_merged_wall_log_cached_texmerge(...)` added to
		`merged_wall_debug.h` and `merged_wall_debug.c`
- removed duplicated `ogl_android_texmerge_log(...)` from both `ogl.c` files
- D1 and D2 cached-texmerge reuse/create log sites now call the shared helper
- validation passed:
	- `run-code-quality.ps1 -Fix`
	- Android `:app:assembleDebug :app:testDebugUnitTest`
	- `run-windows-build.ps1 -Target both -Preset x86-release -BuildType RelWithDebInfo`
	- `diff_vs_upstream.ps1 -Top 20`
- `diff_vs_upstream.ps1 -Top 20` churn lines:
	- `d1/arch/ogl/ogl.c`: `+2173 -50 total 2223`
	- `d2/arch/ogl/ogl.c`: `+2200 -49 total 2249`
- warnings remained non-blocking:
	- existing `loadgl.h` macro redefinition warnings
	- existing `ogl_get_free_texture` not-all-control-paths-return warning
	- Android build also reports `unused function` warnings in `game_introspect.cpp`