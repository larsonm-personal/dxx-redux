# OGL Shared Helper Extraction Phase 28

## Goal

Extract the duplicated Android texture-label anchor and joined-label helper
logic from D1 and D2 `arch/ogl/ogl.c` into shared texture debug code.

## Scope

- android/app/src/main/cpp/shared/android_texture_debug.h
- android/app/src/main/cpp/shared/android_texture_debug.c
- d1/arch/ogl/ogl.c
- d2/arch/ogl/ogl.c

## Work items

- [x] Add shared texture-label anchor and append helpers
- [x] Replace duplicated D1 label helper bodies and call sites
- [x] Replace duplicated D2 label helper bodies and call sites
- [x] Run validation and record the result

## Guardrails

- Keep D1 and D2 mirrored
- Preserve existing overlay-label and joined-label behavior exactly
- Keep the helper Android-only and local to the texture debug path
- Avoid changing larger merged-wall clip logic in this tranche

## Result

- added shared texture debug helpers:
	- `android_texture_debug_get_label_anchor(...)`
	- `android_texture_debug_add_overlay_label(...)`
	- `android_texture_debug_add_joined_labels(...)`
- removed the duplicated joined-label and label-anchor helper bodies from
	both `ogl.c` files
- replaced two repeated overlay-label append blocks and one joined-label
	call site in each game with shared helper calls
- validation passed:
	- `run-code-quality.ps1 -Fix`
	- Android `:app:assembleDebug :app:testDebugUnitTest`
	- `run-windows-build.ps1 -Target both -Preset x86-release -BuildType RelWithDebInfo`
	- `git diff --numstat upstream/main` on the touched files
	- `diff_vs_upstream.ps1 -Top 20`
- phase-28 file-specific churn lines:
	- `android/app/src/main/cpp/shared/android_texture_debug.c`: `+422 -0`
	- `android/app/src/main/cpp/shared/android_texture_debug.h`: `+49 -0`
	- `d1/arch/ogl/ogl.c`: `+1715 -50 total 1765`
	- `d2/arch/ogl/ogl.c`: `+1741 -49 total 1790`