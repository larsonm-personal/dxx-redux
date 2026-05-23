# OGL Shared Helper Extraction Phase 34

## Goal

Extract the duplicated Android GPU timer triple-buffer query rotation from D1 and D2
`arch/ogl/ogl.c` into shared native code.

## Scope

- android/app/src/main/cpp/shared/ogl_gpu_timer_android.h
- android/app/src/main/cpp/shared/ogl_gpu_timer_android.c
- android/app/src/main/cpp/CMakeLists.txt
- d1/arch/ogl/ogl.c
- d2/arch/ogl/ogl.c

## Work items

- [x] Add shared Android GPU timer state and helper surface
- [x] Add the shared GPU timer helper to both Android native targets
- [x] Replace duplicated D1 GPU timer begin/end blocks with shared calls
- [x] Replace duplicated D2 GPU timer begin/end blocks with shared calls
- [x] Run validation and record the result

## Guardrails

- Keep D1 and D2 mirrored for the extracted GPU timer logic
- Limit the shared boundary to the query array and explicit runtime-state pointers
- Preserve the existing oldest-ready query read, blocking fallback, and disjoint guard
- Keep the helper Android-only and avoid reaching into ogl.c file-local macros or globals

## Result

- added `shared/ogl_gpu_timer_android.{h,c}` with a small runtime-state struct that
	carries the query array, ring-buffer counters, and GPU time output pointer
- kept the extraction boundary narrow: the shared helper owns the begin/end query
	rotation while D1/D2 keep their file-local query storage and availability gate
- wired both `ogl_start_frame()` call sites to
	`android_ogl_gpu_timer_begin_frame(...)`
- wired both `gr_flip()` call sites to `android_ogl_gpu_timer_end_frame(...)`
- updated both Android native targets in `android/app/src/main/cpp/CMakeLists.txt`
	to compile the new shared helper
- validation passed:
	- `android\\gradlew.bat :app:assembleDebug :app:testDebugUnitTest --console=plain`
- phase-34 tracked file churn vs `upstream/main`:
	- `android/app/src/main/cpp/CMakeLists.txt`: `+733 -0`
	- `d1/arch/ogl/ogl.c`: `+1606 -50`
	- `d2/arch/ogl/ogl.c`: `+1689 -49`
- new phase-34 helper files:
	- `android/app/src/main/cpp/shared/ogl_gpu_timer_android.h`: `+16 -0` (new file)
	- `android/app/src/main/cpp/shared/ogl_gpu_timer_android.c`: `+62 -0` (new file)