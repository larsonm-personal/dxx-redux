# OGL shared-helper extraction phase 18

## Scope

This tranche extracts the duplicated cached-texmerge cache-entry reset helper
from `d1/arch/ogl/ogl.c` and `d2/arch/ogl/ogl.c` into
`android/app/src/main/cpp/shared/merged_wall_debug.{h,c}`.

In scope:

- define a shared cached-texmerge entry struct in shared headers
- add a shared cache-entry reset helper implementation
- replace D1 and D2 local reset helper usage with the shared helper
- remove dead local reset helper definitions from both `ogl.c` files

Out of scope:

- full cache-clear and slot-replacement policy extraction
- cached-texmerge render path logic
- behavior changes to cache invalidation timing

## Required validation

1. `./android/run-code-quality.ps1 -Fix`
2. `./android/gradlew.bat :app:assembleDebug :app:testDebugUnitTest`
3. `./run-windows-build.ps1 -Target both -Preset x86-release -BuildType RelWithDebInfo`
4. `./android/diff_vs_upstream.ps1 -Top 20`

## Work items

- [x] Add shared cached-texmerge entry struct and reset helper
- [x] Replace D1 and D2 local reset helper calls with shared helper calls
- [x] Remove dead local reset helper definitions in both `ogl.c` files
- [x] Run validation and record the result

## Notes

- Keep reset field values unchanged (`slot=-1`, `orient=-1`, `last_time_used=-1`)
- Keep D1 and D2 cache behavior mirrored

## Result

- added shared cached-texmerge entry struct and reset API:
	- `struct merged_wall_cached_texmerge_entry`
	- `android_merged_wall_cached_texmerge_reset_entry(...)`
- switched D1 and D2 cache entry typedefs to the shared struct
- removed duplicated local `ogl_android_texmerge_reset_entry(...)` from both
	`d1/arch/ogl/ogl.c` and `d2/arch/ogl/ogl.c`
- updated all local reset call sites in both games to use shared helper
- validation passed:
	- `run-code-quality.ps1 -Fix`
	- Android `:app:assembleDebug :app:testDebugUnitTest`
	- `run-windows-build.ps1 -Target both -Preset x86-release -BuildType RelWithDebInfo`
	- `diff_vs_upstream.ps1 -Top 20`
- latest churn lines:
	- `d1/arch/ogl/ogl.c`: `+2077 -50 total 2127`
	- `d2/arch/ogl/ogl.c`: `+2104 -49 total 2153`
	- overall totals: `+18175 -801` across `199` files
