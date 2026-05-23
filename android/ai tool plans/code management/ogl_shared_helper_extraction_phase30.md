# OGL Shared Helper Extraction Phase 30

## Goal

Move the remaining duplicated Android texture-debug and render-context global
definitions out of D1 and D2 `arch/ogl/ogl.c` into shared texture debug code.

## Scope

- android/app/src/main/cpp/shared/android_texture_debug.h
- android/app/src/main/cpp/shared/android_texture_debug.c
- d1/arch/ogl/ogl.c
- d2/arch/ogl/ogl.c

## Work items

- [x] Move duplicated texture-debug globals into shared code
- [x] Remove the duplicate D1 global definitions
- [x] Remove the duplicate D2 global definitions
- [x] Run validation and record the result

## Guardrails

- Keep D1 and D2 mirrored
- Preserve symbol names and storage duration exactly
- Avoid changing any call sites or runtime behavior in this tranche

## Result

- moved these duplicated Android globals from both `ogl.c` files into
	`android_texture_debug.c`:
	- `g_debug_tex_labels`
	- `g_debug_tex_label_count`
	- `g_debug_tex_overlay_active`
	- `g_font_rgb_override`
	- `g_ogl_render_context`
- added shared extern declarations for `g_font_rgb_override` and
	`g_ogl_render_context` in `android_texture_debug.h`
- validation passed:
	- `run-code-quality.ps1 -Fix`
	- Android `:app:assembleDebug :app:testDebugUnitTest`
	- `run-windows-build.ps1 -Target both -Preset x86-release -BuildType RelWithDebInfo`
	- `git diff --numstat upstream/main` on the touched files
	- `diff_vs_upstream.ps1 -Top 20`
- phase-30 file-specific churn lines:
	- `android/app/src/main/cpp/shared/android_texture_debug.c`: `+429 -0`
	- `android/app/src/main/cpp/shared/android_texture_debug.h`: `+51 -0`
	- `d1/arch/ogl/ogl.c`: `+1673 -50 total 1723`
	- `d2/arch/ogl/ogl.c`: `+1697 -49 total 1746`