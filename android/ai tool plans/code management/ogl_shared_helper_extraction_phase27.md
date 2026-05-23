# OGL Shared Helper Extraction Phase 27

## Goal

Extract the duplicated Android MSAA FBO create and destroy helpers from
D1 and D2 `arch/ogl/ogl.c` into shared Android helper code.

## Scope

- android/app/src/main/cpp/shared/ogl_msaa_android.h
- android/app/src/main/cpp/shared/ogl_msaa_android.c
- android/app/src/main/cpp/CMakeLists.txt
- d1/arch/ogl/ogl.c
- d2/arch/ogl/ogl.c

## Work items

- [x] Add shared Android MSAA state and helper functions
- [x] Wire the shared helper file into the Android native build
- [x] Replace duplicated D1 and D2 MSAA helper bodies with thin wrappers
- [x] Run validation and record the result

## Guardrails

- Keep D1 and D2 mirrored
- Preserve the existing MSAA lifecycle and log messages exactly
- Keep `ogl_msaa_max_samples` and `g_msaa_fbo_bound` as local game state
- Avoid touching MSAA resolve or frame-depth behavior in this tranche

## Result

- added shared Android MSAA helper files:
	- `shared/ogl_msaa_android.h`
	- `shared/ogl_msaa_android.c`
- moved the duplicated Android MSAA FBO create and destroy bodies out of
	both `ogl.c` files into shared helpers:
	- `android_ogl_msaa_destroy_fbo(...)`
	- `android_ogl_msaa_create_fbo(...)`
- kept `ogl_msaa_max_samples` and `g_msaa_fbo_bound` local in each game,
	with thin local wrappers preserving the existing lifecycle
- validation passed:
	- `run-code-quality.ps1 -Fix`
	- Android `:app:assembleDebug :app:testDebugUnitTest`
	- `run-windows-build.ps1 -Target both -Preset x86-release -BuildType RelWithDebInfo`
	- `diff_vs_upstream.ps1 -Top 20`
- latest file-specific churn lines:
	- `d1/arch/ogl/ogl.c`: `+1818 -50 total 1868`
	- `d2/arch/ogl/ogl.c`: `+1845 -49 total 1894`
	- `android/app/src/main/cpp/shared/ogl_msaa_android.c`: `101` new lines
	- `android/app/src/main/cpp/shared/ogl_msaa_android.h`: `22` new lines
- note: the current workspace-wide `diff_vs_upstream.ps1` total is inflated by
	unrelated untracked files, so only per-file MSAA/OGL churn is recorded here