# OGL Shared Helper Extraction Phase 20

## Goal

Extract the duplicated cached-texmerge size selection and bounds validation block
from D1 and D2 `arch/ogl/ogl.c` into shared Android helper code.

## Scope

- `android/app/src/main/cpp/shared/merged_wall_debug.h`
- `android/app/src/main/cpp/shared/merged_wall_debug.c`
- `d1/arch/ogl/ogl.c`
- `d2/arch/ogl/ogl.c`

## Work items

- [x] Add shared helper declaration/implementation to compute cached texmerge size
- [x] Replace duplicated D1 and D2 size/bounds logic with the shared helper call
- [x] Keep existing behavior for fallbacks and texture-size limit checks
- [x] Run validation and record the result

## Guardrails

- Do not change cache policy or slot selection behavior
- Keep D1 and D2 mirrored
- Keep Android-only behavior behind existing compile guards

## Result

- added shared helper:
	- `android_merged_wall_cached_texmerge_choose_size(...)`
- replaced duplicated size/fallback/limit logic in both `ogl.c` files with a
	shared helper call
- preserved existing behavior for fallback to base texture dimensions and
	rejection when dimensions are invalid or exceed `ogl_max_texture_size`
- validation passed:
	- `run-code-quality.ps1 -Fix`
	- Android `:app:assembleDebug :app:testDebugUnitTest`
	- `run-windows-build.ps1 -Target both -Preset x86-release -BuildType RelWithDebInfo`
	- `diff_vs_upstream.ps1 -Top 20`
- latest churn lines:
	- `d1/arch/ogl/ogl.c`: `+2062 -50 total 2112`
	- `d2/arch/ogl/ogl.c`: `+2089 -49 total 2138`
	- overall totals: `+18145 -801` across `199` files
