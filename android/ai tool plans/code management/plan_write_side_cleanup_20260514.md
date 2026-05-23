# Plan: Write-Side Cleanup 2026-05-14

## Goal

- Reduce duplicated Android save write-side logic across D1/D2 where it is behavior-neutral to do so
- Remove or tighten nearby warning-prone code without reopening validated behavior changes

## Steps

- [x] Identify the smallest shared helper that can replace duplicated Android save write-side code in D1/D2
- [x] Fix the remaining local warning-prone site in `d2/main/gamecntl.c` if a minimal change exists
- [x] Rebuild Android debug after the cleanup edits
- [x] Run a focused validation for the touched Android save/resume path

## Outcome

- The shared Android save metadata layer now owns the common PHYSFS trailer append path via `android_save_meta_write_physfs()`, which removes another duplicated build-and-write block from both `d1/main/state.c` and `d2/main/state.c`.
- `d2/main/gamecntl.c` now reads RNG probe state through the correct pointer API before logging, which removes the local clang warning without changing the probe output shape.
- Validation passed after formatting on the final on-disk state: `android/gradlew.bat assembleDebug` succeeded and `android/run_test.ps1 -ScriptName test_autosave_resume_unified.json5 -Game d2` finished with `PASS (file-based, 30/29 steps, 371ms)` and `EXIT: 0`.