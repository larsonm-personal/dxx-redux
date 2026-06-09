# Level metadata crash repro and fix

## Goal
- Reproduce and fix the launcher crash/noisy crash report when opening level metadata for base D2 and mission ZIP packages such as obsidian.zip.
- Prefer an emulator test that exercises the launcher metadata button or the same analyzer path.

## Plan
- [x] Inspect setup-screen automation support for opening file and mod detail dialogs.
- [x] Add a focused test hook or script for base D2 and a mission ZIP metadata analysis path.
- [x] Fix the worker lifecycle or native analyzer failure source.
- [x] Improve diagnostics if the worker exits without a normal result.
- [x] Run scoped code quality, Android Kotlin/native builds, and the focused test when available.

## Notes
- The reported xCrash text says the dumper child terminated with exit status 102, without a useful native stack.
- Current implementation explicitly exits the `:levelmeta` worker process after writing a result; this may produce crash-report noise even for successful analyses.
- Replaced the single explicit-exit worker with persistent D1/D2-specific worker processes.
- Added launcher automation action `analyze_level_metadata` and script `test_level_metadata_launcher_d2_obsidian.json5`.
- Because workers now persist, staged ZIP directories are mounted for each request instead of only during first native runtime initialization.
- Reproduced the base D2 crash in emulator, then fixed Android PhysFS bootstrap by passing a real Android `Context` into native metadata analysis.
- Focused emulator run passed for base D2 and Obsidian ZIP metadata analysis.
