# Ending movie and mission isolation failures

- [x] Declare the ending movie library required by the D2 single-player ending test
- [x] Correct the mission isolation owner timeout and clean up its staged fixtures
- [x] Run both focused integration tests and scoped lint

Preserve the existing uncommitted metadata timeout and cleanup changes

## Findings

- The D2 ending test expected `end.mve` without declaring the `intro-h.mvl` library containing it. No movie libraries were installed on the emulator
- Once installed, that library exposed `skip_intro` sending ordinary taps to movie windows, which deliberately ignore those taps. The helper now uses the same generation-tagged Skip request as the UI for movie windows, preserving explicit controller input and static title taps
- The mission isolation owner allowed its child 600 seconds but inherited the suite's 120-second outer timeout. Its budget is now 900 seconds including staging and cleanup
- Mission isolation now releases old generated projections before staging and removes its fixture archives, manifest, publications, and mount references in `finally`

## Validation

- APK build, scoped lint, and automation catalog validation passed
- D2 ending test passed all 31 steps with real ending movie playback
- Mission isolation passed all 95 steps and its seven campaign-owner/sound-bank checks; cleanup left about 1.1 GB free
- Intro input regression passed for both D1 and D2
- Dependency resolution regenerated the local game-data index to include the declared movie library
- Logs: `temp/suite_endgame_20260918/`
- Full suite not run
