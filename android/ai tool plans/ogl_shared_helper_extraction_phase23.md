# OGL Shared Helper Extraction Phase 23

## Goal

Extract the duplicated cached-texmerge FBO render pass from D1 and D2
`arch/ogl/ogl.c` into shared Android helper code.

## Scope

- android/app/src/main/cpp/shared/merged_wall_debug.h
- android/app/src/main/cpp/shared/merged_wall_debug.c
- d1/arch/ogl/ogl.c
- d2/arch/ogl/ogl.c

## Work items

- [x] Add shared helper to render a cached texmerge into an output texture
- [x] Replace duplicated D1 and D2 FBO render-pass block with the shared helper call
- [x] Keep GL state save/restore, wrap handling, and shader draw ordering unchanged
- [x] Run validation and record the result

## Guardrails

- Keep D1 and D2 mirrored
- Do not change cache semantics or texture creation flow
- Preserve TexFilt and anisotropy behavior
- Preserve wrapstate tracking on source textures

## Result

- added shared helper:
	- `android_merged_wall_cached_texmerge_render_to_texture(...)`
- added internal shared wrap helper used by the render path to preserve
	source-texture wrapstate tracking inside shared code
- replaced duplicated D1/D2 cached-texmerge FBO render-pass block with the
	shared helper call
- preserved:
	- GL state save/restore behavior
	- source texture wrap transitions (`CLAMP_TO_EDGE` then `REPEAT`)
	- shader setup and draw ordering
	- post-render filter/aniso finalization behavior
- validation passed:
	- `run-code-quality.ps1 -Fix`
	- Android `:app:assembleDebug :app:testDebugUnitTest`
	- `run-windows-build.ps1 -Target both -Preset x86-release -BuildType RelWithDebInfo`
	- `diff_vs_upstream.ps1 -Top 20`
- latest churn lines:
	- `d1/arch/ogl/ogl.c`: `+1933 -50 total 1983`
	- `d2/arch/ogl/ogl.c`: `+1960 -49 total 2009`
	- overall totals: `+17887 -801` across `199` files
