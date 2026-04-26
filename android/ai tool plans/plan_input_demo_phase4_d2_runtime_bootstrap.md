# Input Demo Phase 4 D2 Runtime Bootstrap Tranche

## Goal

Wire the first D2 runtime replay path into the existing engine using the shared
replay-session helper.

This tranche should:

- add `-inputdemo-replay <demo-file>` to D2 startup
- validate `rng_mode` before allocating replay state
- load the shared replay session and auto-start the requested mission and level
- drive `FrameTime`, `Controls`, and RNG from replay frames inside the D2 game loop

This tranche does not yet add D1 runtime replay, checkpoint restore, or final
result comparison.

## Constraints

- Keep replay parsing and sparse-stream policy in the shared Android-side code
- Use the normal D2 mission and new-game startup path rather than inventing a
  custom entry path
- Support `start_mode: "new_level"` only for this slice and fail fast on other
  fixture modes
- Validate with focused D2 executable checks before wider Android validation

## Planned Steps

- [x] Wire shared replay helper into D2 desktop and Android build graphs
- [x] Add D2 `-inputdemo-replay <demo-file>` startup handling in `inferno.c`
- [x] Add D2 game-loop replay frame injection in `game.c`
- [x] Run focused D2 desktop validation
- [x] Run Android native validation

## Exit Criteria

- D2 accepts `-inputdemo-replay <demo-file>` and starts the requested mission and level
- While replay is active, D2 uses recorded `FrameTime`, controls, and RNG state per frame
- D2 desktop build and Android native build still pass

## Progress on 2026-04-26

- added D2 runtime replay startup in `inferno.c` by extending the existing validation path with `-inputdemo-replay <demo-file>`
- fail-fast checks currently reject non-D2 fixtures and any `start_mode` other than `new_level`
- replay bootstrap now loads the requested mission, applies recorded difficulty, and enters `StartNewGame(level)` through the normal D2 path
- D2 `game.c` now skips idle control polling while replay is active and substitutes recorded `FrameTime`, `Controls`, and RNG state from the shared replay session before each gameplay frame
- replay state unloads cleanly on end-of-stream, replay load failure, frame-expansion failure, or window close
- initial Android validation exposed a local build-graph defect because `input_demo_replay.cpp` had been added to the Android D1 source list instead of the D2 list; fixing that and rerunning produced a clean native build
- validation passed with `run-windows-build.ps1 -Target d2`, `buildd2\maths\test_input_demo_replay.exe`, and `android\gradlew.bat :app:externalNativeBuildDebug --no-daemon`
