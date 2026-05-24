# Fix Branch-Touched Warnings 2026-04-28

## Goal

Reduce warning noise only for D1/D2 warnings attributable to code changed on this branch, while keeping the merge diff small.

## Steps

- [x] Identify warning sites that are tied to branch-touched D1/D2 lines.
- [x] Fix only the concrete branch-owned warning sites.
- [x] Re-run a warning-producing Android build and confirm the targeted warnings are gone.

## Result

- Removed the branch-leftover unused `objnum` local from `d2/main/object.c`.
- Re-ran `:app:buildCMakeDebug[arm64-v8a]-2` and confirmed the `d2/main/object.c` warning no longer appears.
- Left the remaining `d2/main/ai.c` and `d2/main/physics.c` warnings alone because the same warning sites are already present on `origin/cmake`.

## 2026-05-24 Follow-up

- [x] Removed the branch-owned `d2/main/dxa_metadata_patch.cpp` packed-member warnings by replacing pointer-return helpers with by-value helpers and explicit indexed writes.
- [x] Re-ran `./run-linux-build.sh --target d2 --clean` and confirmed the `dxa_metadata_patch.cpp` warnings are gone.
- [x] Left the remaining desktop `d2` warnings alone because `git blame` showed they are upstream-owned in `d2/misc/strutil.c`, `d2/xmodel/xmodel.cpp`, `d2/xmodel/aseread.cpp`, `d2/main/kconfig.c`, `d2/main/menu.c`, and `d2/main/multi.c`.
- [x] Removed the branch-owned extract test `-Wformat-truncation` warnings in `test_cue_iso.c`, `iso9660_reader.c`, and `fingerprint_audio.c`.
- [x] Re-ran `timeout 60s bash ./android/run_cue_iso_tests.sh` and confirmed `100% tests passed, 0 tests failed out of 6` with no remaining compile warnings in the captured log.