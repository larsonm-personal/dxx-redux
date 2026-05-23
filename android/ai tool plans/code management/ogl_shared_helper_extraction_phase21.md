# OGL Shared Helper Extraction Phase 21

## Goal

Extract the duplicated cached-texmerge cache-slot selection logic from D1 and D2
`arch/ogl/ogl.c` into shared Android helper code.

## Scope

- android/app/src/main/cpp/shared/merged_wall_debug.h
- android/app/src/main/cpp/shared/merged_wall_debug.c
- d1/arch/ogl/ogl.c
- d2/arch/ogl/ogl.c

## Work items

- [x] Add shared helper declaration/implementation to choose cache slot
- [x] Replace duplicated D1 and D2 slot-selection logic with the shared helper call
- [x] Keep existing LRU/fallback behavior identical
- [x] Run validation and record the result

## Guardrails

- Keep D1 and D2 mirrored
- Do not change cache size, reuse matching, or texture creation behavior
- Keep Android-only behavior behind existing compile guards

## Result

- added shared helper:
	- `android_merged_wall_cached_texmerge_choose_slot(...)`
- replaced duplicated D1/D2 cache-slot selection logic with the shared helper
	call
- preserved existing behavior:
	- first prefer free/invalid slots
	- otherwise evict the least-recently-used slot
- validation passed:
	- `run-code-quality.ps1 -Fix`
	- Android `:app:assembleDebug :app:testDebugUnitTest`
	- `run-windows-build.ps1 -Target both -Preset x86-release -BuildType RelWithDebInfo`
	- `diff_vs_upstream.ps1 -Top 20`
- latest churn lines:
	- `d1/arch/ogl/ogl.c`: `+2052 -50 total 2102`
	- `d2/arch/ogl/ogl.c`: `+2079 -49 total 2128`
	- overall totals: `+18125 -801` across `199` files
