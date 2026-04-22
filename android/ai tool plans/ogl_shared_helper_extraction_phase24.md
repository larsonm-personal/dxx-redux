# OGL Shared Helper Extraction Phase 24

## Goal

Extract the remaining duplicated cached-texmerge output-texture setup block from
D1 and D2 `arch/ogl/ogl.c` into shared Android helper code.

## Scope

- android/app/src/main/cpp/shared/merged_wall_debug.h
- android/app/src/main/cpp/shared/merged_wall_debug.c
- d1/arch/ogl/ogl.c
- d2/arch/ogl/ogl.c

## Work items

- [x] Add shared helper for cached-texmerge output-texture setup
- [x] Replace duplicated D1 and D2 output-texture setup block with the shared helper call
- [x] Remove now-unused local render-pass constant arrays left behind by phase 23
- [x] Run validation and record the result

## Guardrails

- Keep D1 and D2 mirrored
- Leave `ogl_get_free_texture`, `ogl_init_texture`, `tex_set_size`, and `r_texcount` local
- Preserve texture flags, wrap mode, filter setup, and image allocation behavior

## Result

- added shared helper:
	- `android_merged_wall_cached_texmerge_setup_output_texture(...)`
- replaced duplicated D1/D2 output-texture setup block with a shared helper call
	while keeping these local in `ogl.c`:
	- `ogl_get_free_texture()`
	- `ogl_init_texture(...)`
	- `tex_set_size(...)`
	- `r_texcount++`
- removed now-unused local render-pass constant arrays from both `ogl.c` files
	after phase 23 moved the draw path to shared code
- validation passed:
	- `run-code-quality.ps1 -Fix`
	- Android `:app:assembleDebug :app:testDebugUnitTest`
	- `run-windows-build.ps1 -Target both -Preset x86-release -BuildType RelWithDebInfo`
	- `diff_vs_upstream.ps1 -Top 20`
- latest churn lines:
	- `d1/arch/ogl/ogl.c`: `+1901 -50 total 1951`
	- `d2/arch/ogl/ogl.c`: `+1928 -49 total 1977`
	- overall totals: `+17823 -801` across `199` files
