# OGL Shared Helper Extraction Phase 26

## Goal

Extract the remaining duplicated cached-texmerge slot-reserve and eviction block
from D1 and D2 `arch/ogl/ogl.c` into shared Android helper code.

## Scope

- android/app/src/main/cpp/shared/merged_wall_debug.h
- android/app/src/main/cpp/shared/merged_wall_debug.c
- d1/arch/ogl/ogl.c
- d2/arch/ogl/ogl.c

## Work items

- [x] Add shared helper for cached-texmerge slot reservation and eviction
- [x] Replace the duplicated D1 slot-reserve block
- [x] Replace the duplicated D2 slot-reserve block
- [x] Run validation and record the result

## Guardrails

- Keep D1 and D2 mirrored
- Keep texture allocation, `tex_set_size`, and `r_texcount` local
- Use the local `ogl_freetexture(...)` only as the eviction callback
- Preserve slot numbering and cache replacement behavior exactly

## Result

- added shared helper:
	- `android_merged_wall_cached_texmerge_reserve_entry(...)`
- replaced the duplicated D1 and D2 slot-select, eviction, and reset block
	with one shared helper call
- internalized the now-local slot chooser inside `merged_wall_debug.c`
	because it is no longer called directly from `ogl.c`
- kept texture allocation, `tex_set_size(...)`, and `r_texcount++` local
	in both game OGL files
- validation passed:
	- `run-code-quality.ps1 -Fix`
	- Android `:app:assembleDebug :app:testDebugUnitTest`
	- `run-windows-build.ps1 -Target both -Preset x86-release -BuildType RelWithDebInfo`
	- `diff_vs_upstream.ps1 -Top 20`
- latest churn lines:
	- `d1/arch/ogl/ogl.c`: `+1887 -50 total 1937`
	- `d2/arch/ogl/ogl.c`: `+1914 -49 total 1963`
	- overall totals: `+17795 -801` across `199` files
