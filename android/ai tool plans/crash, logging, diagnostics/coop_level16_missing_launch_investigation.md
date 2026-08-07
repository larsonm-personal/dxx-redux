# Coop level 16 missing launch investigation

- [x] Correlate the supplied log with multiplayer host and restore state transitions
- [x] Trace the `gameseg.c` illegal side-type failure and identify inputs that can produce it
- [x] Check recent relevant changes and existing tests for a regression mechanism
- [x] Record findings, confidence, and the next diagnostic evidence needed

## Findings

- Build `1ba93942` contains dormancy refactor `b3cd3143`
- The central overlay poll never starts on the first activity resume because
  `uiWorkSuspended` begins false and `resumeUiWork()` returns immediately
- The supplied `central_polls=0` diagnostic confirms the failed startup path
- The start button depends on that poll calling `nativeIsHostSelectingPlayers()`
- Native player selection remains functional because the built-in `OK` item bypasses the overlay
- The `Auto L0` save is a separate lobby autosave bug: coop mode is active before a level is loaded,
  and `coop_autosave()` does not reject `Current_level_num == 0`
- Saving at level zero reaches uninitialized segment geometry and produces the side type zero popup
- Restore slot 6 was selected while the bad rotating autosave targeted slot 5, so the selected save is not implicated

## Suggested fixes

- Always start the central overlay poll on resume while keeping independent overlay resume calls conditional
- Reject coop autosaves unless an active level and game window are present
- Add coverage for initial activity resume and for autosave requests in the pre-level coop lobby

## Implementation

- [x] Restore central overlay polling on the initial activity resume
- [x] Reject coop autosaves before active level gameplay
- [x] Add or extend focused regression coverage
- [x] Run scoped formatting, tests, and build verification

## Verification

- Scoped code-quality pass completed for the changed Kotlin, C, and JSON5 files
- `:app:testDebugUnitTest` passed
- `:app:assembleDebug` passed for arm64-v8a, armeabi-v7a, and x86_64
- D2 `test_launch_to_automap.json5` passed all 87 steps on emulator-5554
