# OGL Shared Helper Extraction Phase 31

## Goal

Extract the duplicated Android texture memory stats and anisotropy reapply
helpers from D1 and D2 `arch/ogl/ogl.c` into shared native code.

## Scope

- android/app/src/main/cpp/shared/ogl_texture_android.h
- android/app/src/main/cpp/shared/ogl_texture_android.c
- android/app/src/main/cpp/CMakeLists.txt
- d1/arch/ogl/ogl.c
- d2/arch/ogl/ogl.c

## Work items

- [x] Add shared Android texture helper declarations and implementation
- [x] Replace duplicated D1 helper bodies with thin wrappers
- [x] Replace duplicated D2 helper bodies with thin wrappers
- [x] Run validation and record the result

## Guardrails

- Keep D1 and D2 mirrored
- Keep the shared helper Android-only and limited to texture-list iteration
- Preserve logging, anisotropy level clamping, and bind-cache invalidation
- Avoid widening the shared surface beyond the minimum state needed

## Result

- added shared Android texture helpers in `shared/ogl_texture_android.{h,c}` for:
	- texture-list byte counting
	- anisotropy reapply across the texture list
	- shared bind-cache and `GL_TEXTURE_2D` runtime-state helpers used by the
		cached-texmerge shared path
- replaced the duplicated `ogl_get_texture_bytes()` and
	`ogl_apply_anisotropy_all()` bodies in both `ogl.c` files with thin wrappers
- fixed adjacent Android-only shared OGL build gaps exposed by validation:
	- replaced `merged_wall_debug.c`'s dependence on private `ogl.c` macros with
		explicit runtime state passed from the D1/D2 call site
	- replaced the non-portable `oglprog.h` include in `merged_wall_debug.c`
		with local declarations matching the shared shader API surface
	- added the already-used `shared/ogl_msaa_android.c` source to the D2 Android
		target in `CMakeLists.txt`
- validation passed:
	- `run-code-quality.ps1 -Fix`
	- Android `:app:assembleDebug :app:testDebugUnitTest`
	- `run-windows-build.ps1 -Target both -Preset x86-release -BuildType RelWithDebInfo`
	- `git diff --numstat upstream/main`
	- `diff_vs_upstream.ps1 -Top 20`
- phase-31 tracked file-specific churn lines:
	- `android/app/src/main/cpp/CMakeLists.txt`: `+731 -0`
	- `android/app/src/main/cpp/shared/merged_wall_debug.c`: `+6106 -0`
	- `android/app/src/main/cpp/shared/merged_wall_debug.h`: `+242 -0`
	- `d1/arch/ogl/ogl.c`: `+1672 -50 total 1722`
	- `d2/arch/ogl/ogl.c`: `+1696 -49 total 1745`
- phase-31 OGL delta versus phase 30:
	- `d1/arch/ogl/ogl.c`: shrank from `+1673 -50` to `+1672 -50`
	- `d2/arch/ogl/ogl.c`: shrank from `+1697 -49` to `+1696 -49`
