# OGL Shared Helper Extraction Phase 22

## Goal

Extract larger remaining duplicated cached-texmerge helper chunks from D1 and D2
`arch/ogl/ogl.c` into shared Android helper code.

## Scope

- android/app/src/main/cpp/shared/merged_wall_debug.h
- android/app/src/main/cpp/shared/merged_wall_debug.c
- d1/arch/ogl/ogl.c
- d2/arch/ogl/ogl.c

## Work items

- [x] Add shared helper for cached-texmerge reuse lookup
- [x] Add shared helper for cached-texmerge entry commit metadata
- [x] Add shared helpers for render/final texture filter setup
- [x] Replace duplicated D1 and D2 helper blocks with shared calls
- [x] Run validation and record the result

## Guardrails

- Keep D1 and D2 mirrored
- Do not change cache semantics or GL call ordering around FBO work
- Keep behavior identical for TexFilt/anisotropy handling

## Result

- added shared helpers:
	- `android_merged_wall_cached_texmerge_try_reuse(...)`
	- `android_merged_wall_cached_texmerge_commit_entry(...)`
	- `android_merged_wall_cached_texmerge_set_render_filters(...)`
	- `android_merged_wall_cached_texmerge_finalize_filters(...)`
- replaced duplicated D1/D2 blocks in `ogl_android_get_cached_plain_texmerge_bitmap(...)`:
	- reuse-scan loop
	- pre-render texture filter setup
	- post-render mip/anisotropy/final filter setup
	- entry metadata commit block
- fixed follow-up Android compile issue by adding `#include "timer.h"` in
	`merged_wall_debug.c`
- validation passed:
	- `run-code-quality.ps1 -Fix`
	- Android `:app:assembleDebug :app:testDebugUnitTest`
	- `run-windows-build.ps1 -Target both -Preset x86-release -BuildType RelWithDebInfo`
	- `diff_vs_upstream.ps1 -Top 20`
- latest churn lines:
	- `d1/arch/ogl/ogl.c`: `+2052 -50 total 2102`
	- `d2/arch/ogl/ogl.c`: `+2079 -49 total 2128`
	- overall totals: `+18125 -801` across `199` files
