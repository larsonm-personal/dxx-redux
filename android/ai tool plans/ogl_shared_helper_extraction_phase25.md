# OGL Shared Helper Extraction Phase 25

## Goal

Extract the remaining duplicated cached-texmerge finalize-orchestration block from
D1 and D2 `arch/ogl/ogl.c` into shared Android helper code.

## Scope

- android/app/src/main/cpp/shared/merged_wall_debug.h
- android/app/src/main/cpp/shared/merged_wall_debug.c
- d1/arch/ogl/ogl.c
- d2/arch/ogl/ogl.c

## Work items

- [x] Add shared helper for cached-texmerge render failure rollback and finalize flow
- [x] Replace the duplicated D1 finalize-orchestration block
- [x] Replace the duplicated D2 finalize-orchestration block
- [x] Run validation and record the result

## Guardrails

- Keep D1 and D2 mirrored
- Keep `ogl_get_free_texture`, `ogl_init_texture`, `tex_set_size`, and `r_texcount` local
- Pass only the local texture-free callback needed for the failure path
- Preserve commit, bitmap init, out-slot, and create-log behavior exactly

## Result

- added shared helper:
	- `android_merged_wall_cached_texmerge_finalize_entry(...)`
- replaced the duplicated D1 and D2 cached-texmerge finalize-orchestration block
	with one shared helper call
- kept the local-only pieces in `ogl.c` limited to:
	- `ogl_get_free_texture()`
	- `ogl_init_texture(...)`
	- `tex_set_size(...)`
	- `r_texcount++`
	- `ogl_freetexture(...)` passed as the failure-path callback
- validation passed:
	- `run-code-quality.ps1 -Fix`
	- Android `:app:assembleDebug :app:testDebugUnitTest`
	- `run-windows-build.ps1 -Target both -Preset x86-release -BuildType RelWithDebInfo`
	- `diff_vs_upstream.ps1 -Top 20`
- latest churn lines:
	- `d1/arch/ogl/ogl.c`: `+1891 -50 total 1941`
	- `d2/arch/ogl/ogl.c`: `+1918 -49 total 1967`
	- overall totals: `+17803 -801` across `199` files
