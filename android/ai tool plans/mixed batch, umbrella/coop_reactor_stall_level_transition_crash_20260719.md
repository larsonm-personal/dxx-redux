# Coop reactor-room stall and level-transition crash

## Scope

- Investigate the severe Android frame-rate drop near the D2 reactor room
- Investigate the host process exit while advancing to the next coop level
- Prefer diagnostics first where the supplied log cannot establish a safe fix

## Plan

- [x] Read repository instructions and preserve unrelated worktree changes
- [x] Correlate the supplied log with level, texture-cache, rendering, and coop transition code
- [x] Add focused Android diagnostics or implement narrowly supported fixes
- [x] Add or extend regression coverage
- [x] Run scoped formatting, tests, and CMake build verification
- [x] Record findings and completed work here

## Findings

- The supplied log covers D2 level 7 and the transition into level 8. It does not contain a native crash report or signal.
- Level 8 completed `LoadLevel()` and the post-load texture-cache reset. The launcher restore offer appeared about three seconds later.
- A failed network level sync deliberately closes the game window and returns to the launcher, but its existing explanation was written only to the Network category. The supplied Coop log therefore could not distinguish that path from a native crash.
- The 2.4 MB bitmap cache did page out during level 7 gameplay, but only once in the supplied log. This can cause a hitch, but it does not establish the reported sustained 1-2 FPS slowdown.
- Enhanced models are not used for the heavy driller or reactor. Both use the legacy polygon-model renderer.
- Existing Profiling logs already record slow-frame CPU/GPU buckets and the most expensive rendered object, including object type, ID, model, and render duration. A reproduction with Profiling enabled can distinguish the suspected reactor and robot from texture, simulation, GPU, and buffer-swap stalls.
- The follow-up native crash report has no signal or backtrace because the xCrash dumper itself exited with status 102. Its recovered breadcrumbs do show D2 cache-full guards at `piggy.c:1249` and `piggy.c:1296` immediately before termination.
- Android `Int3()` only records a breadcrumb, so neither guard directly terminates the process. They nevertheless confirm bitmap cache pressure during the failure.
- D2's post-RLE capacity test double-counted the bitmap just loaded: it advanced `Piggy_bitmap_cache_next` by `zsize`, then tested `Piggy_bitmap_cache_next + zsize`. This caused unnecessary full-cache eviction and reload cycles.

## Changes

- Added Android coop log markers and crash breadcrumbs around D1/D2 level advance, level load, network sync, session-close, and ready phases.
- Kept all new behavior Android-only and duplicated the small upstream hooks in D1 and D2.
- Corrected D2's post-RLE cache test to compare the actual advanced cursor against capacity without adding the bitmap size twice.
- On Android, D2 now sizes the bitmap cache to the complete PIG bitmap payload plus the existing 10 percent margin. The explicit low-memory mode retains its 1.4 MB limit.
- Added an Android automation regression that launches Counterstrike level 7 with `water.pig`, waits through cache warmup, and verifies the game remains active on the requested level.

## Validation

- Passed scoped code-quality checks for all modified source files, the automation script, and this plan.
- Passed Android `:app:assembleDebug` for arm64-v8a, armeabi-v7a, and x86_64.
- Passed Windows CMake configure/build for D1 and D2 through `run-windows-build.ps1 -Target both`.
- Passed Android `:app:testDebugUnitTest`.
- Passed `test_d2_level7_bitmap_cache.json5` on the Android emulator: 21 of 21 steps, level 7 active after cache warmup.
- CTest reported no registered tests in the Windows release build directories.
