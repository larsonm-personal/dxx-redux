# OGL Shared Helper Extraction Phase 29

## Goal

Replace the remaining duplicated Android screen-space overlay label append
block in D1 and D2 `arch/ogl/ogl.c` with the shared texture debug helper.

## Scope

- d1/arch/ogl/ogl.c
- d2/arch/ogl/ogl.c

## Work items

- [x] Replace the duplicated D1 screen-space label block
- [x] Replace the duplicated D2 screen-space label block
- [x] Run validation and record the result

## Guardrails

- Keep D1 and D2 mirrored
- Reuse the phase-28 shared helper instead of adding new shared code
- Preserve the existing `draw_tmap` and merged-wall skip conditions

## Result

- replaced the remaining duplicated screen-space overlay label append block in
	both `ogl.c` files with `android_texture_debug_add_overlay_label(...)`
- validation initially exposed a local Android build defect in the shared
	anchor helper (`Canv_w2` / `Canv_h2` declaration); fixed in-place and
	reran the same validation slice successfully
- validation passed:
	- `run-code-quality.ps1 -Fix`
	- Android `:app:assembleDebug :app:testDebugUnitTest`
	- `run-windows-build.ps1 -Target both -Preset x86-release -BuildType RelWithDebInfo`
	- `git diff --numstat upstream/main` on the touched files
	- `diff_vs_upstream.ps1 -Top 20`
- phase-29 file-specific churn lines:
	- `android/app/src/main/cpp/shared/android_texture_debug.c`: `+424 -0`
	- `android/app/src/main/cpp/shared/android_texture_debug.h`: `+49 -0`
	- `d1/arch/ogl/ogl.c`: `+1681 -50 total 1731`
	- `d2/arch/ogl/ogl.c`: `+1706 -49 total 1755`