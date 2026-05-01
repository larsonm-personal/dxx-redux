# Plan: headless console demo runner

Date: 2026-04-30

Goal: create a true console input-demo regression runner that does not initialize SDL video, create a window, or run rendering, and that replays `.dximdemo` files as fast as possible with minimal output and final result reporting.

## Phases

| phase | task | status |
|---|---|---|
| 1 | Inventory current replay, engine startup, build targets, SDL/render dependencies, and existing regression scripts. | completed |
| 2 | Identify the smallest reusable game-logic entry point needed to load a checkpoint, run input frames, compare final state, and exit. | completed |
| 3 | Design a separate console target with its own `main.cpp` and minimal platform stubs for video, audio, input polling, and frame presentation. | in_progress |
| 4 | Add a first compile-only host target that links the replay parser/result code and documents missing engine dependencies. | completed |
| 5 | Extend the target to execute a single D2 `.dximdemo` without SDL video and print only PASS/FAIL plus result details. | not-started |
| 6 | Update regression scripts to prefer the console runner when available and keep the windowed runner as a fallback. | not-started |
| 7 | Add timing output and validation so large demo batches can track speed and deterministic final state. | not-started |

## Notes

- The existing `-inputdemo-norender` path is not the final goal. It still initializes SDL/OpenGL and keeps render traversal for determinism.
- A prior experiment showed that simply skipping `game_render_frame()` changes simulation because render traversal feeds rendered-object data used by robot wake-up logic.
- The console runner should either preserve required non-visual render side effects explicitly or move those side effects out of rendering before removing rendering entirely.
- Default output should be quiet: one final PASS/FAIL line, elapsed time, replay frames, and the result JSON path or mismatch summary.

## Initial inventory findings

- `android/tests/run_input_demo_headless.ps1` is currently only a no-present wrapper over `run_input_demo_replay.ps1 -NoRender`. It is useful as a temporary staged check, but it is not the final console runner.
- `android/tests/run_input_demo_regressions.ps1` already scans `android/regression_demos` and delegates each file to the windowed wrapper. Later it should prefer the console runner when the executable exists and keep the current wrapper as fallback.
- `d1/main/CMakeLists.txt` and `d2/main/CMakeLists.txt` build monolithic `dxx-redux-d1` and `dxx-redux-d2` targets from source lists that include `inferno.c`. A custom `main.cpp` will need the game logic list split from the interactive main source, or a new object/static library built from the same list minus `inferno.c`.
- The normal host build globally defines `OGL` when `OPENGL` is on in `d1/CMakeLists.txt` and `d2/CMakeLists.txt`. A headless target in the same configure cannot simply opt out of OpenGL with target-local sources while that global definition is present. The clean choices are a dedicated non-OGL headless configure first, or a later CMake cleanup that makes graphics definitions target-local.
- `run-windows-build.ps1` does not currently accept pass-through CMake options, so a first non-OGL headless configure would need either a small helper/script or a manual CMake invocation into a separate build directory such as `buildd2_headless`.
- `d2/main/inferno.c` owns input-demo startup in static `maybe_start_input_demo_replay()`. It loads and validates the `.dximdemo`, applies player config, then either calls `StartNewGame()` for `new_level` or writes/restores the embedded checkpoint with `state_restore_all_sub()` for `save_checkpoint`.
- `d2/main/game.c` owns replay stepping in static `input_demo_apply_replay_frame()`, `input_demo_finish_replay_frame()`, `input_demo_stop_replay()`, and `input_demo_write_replay_result()`. The current window-driven loop reaches these through `game_handler(EVENT_WINDOW_DRAW)`.
- The current frame order is replay frame apply, `ReadControlsReplayFrame()`, `calc_game_time()`, `GameProcessFrame()`, replay frame finish, then `game_render_frame()`. A console loop should preserve this order except for visual drawing, and must still write/compare the embedded result trailer.
- `d2/arch/sdl/window.c` is an internal engine window list and does not use SDL directly. It can probably remain available to reduce churn, as long as the console runner does not create an OS window.
- SDL/OpenGL dependencies to replace for a true console target are `arch/sdl/init.c`, `event.c`, `timer.c`, `key.c`, `mouse.c`, `joy.c`, audio files, redbook/jukebox files, and the graphics backend from `arch/sdl/gr.c` or `arch/ogl/*`.
- A small `arch/headless` backend should provide console error/warning output, no-op input/audio, a deterministic fast timer path, and a `gr_init()`/`gr_set_mode()` implementation that allocates a software canvas for code that expects `grd_curscreen`, without presenting or creating an SDL window.
- Render traversal side effects are the hard part. `render.c` fills `Window_rendered_data` via `do_render_object()`, and consumers include `laser.c` and `object.c`/`wake_up_rendered_objects()`. The console runner needs a non-visual visibility/update pass or those side effects must be moved out of drawing before `game_render_frame()` is skipped.
- A first D2 console scaffold now builds in the existing host configure as `buildd2/main/dxx-redux-d2-headless.exe` by reusing the full D2 game source list minus `inferno.c`, then adding a tiny runtime stub for `LeaveEvents`, `Quitting`, `Screen_mode`, and the critical-error globals.
- The host scaffold still needs `net_udp.c` and `shared/net/net_udp_android.c` on the target when `USE_UDP` is enabled, because menu and multiplayer objects keep those references live even when the console `main.cpp` only parses a demo file.
- The current scaffold does not run the engine yet. It validates RNG metadata, loads a `.dximdemo`, and prints a single summary line without opening a window. This is enough to keep phase 5 focused on actual replay execution instead of target setup.

## Candidate first implementation slice

1. Extract D2 replay startup from `inferno.c` into a small exported helper that takes a demo path and returns a status after starting the level or restoring the checkpoint. Keep the existing `-inputdemo-replay` code as a caller of that helper.
2. Extract D2 replay stepping/result writing from `game.c` into exported helpers, for example `input_demo_replay_run_frame()` and `input_demo_replay_finish_without_window()`, while leaving `game_handler()` behavior unchanged.
3. Add a D2-only compile target first, in a dedicated non-OGL host configure if needed, with `android/app/src/main/cpp/headless/input_demo_headless_main.cpp` and a new headless arch backend.
4. Have the first runnable version execute only checkpoint-start demos, because that avoids menu/title/pilot selection paths and matches the regression fixture already in `android/regression_demos`.
5. After D2 passes the existing fixture, duplicate the minimal exported API and target shape for D1.

## Current progress

- Steps 1 and 2 are done in both D2 and D1. Replay startup and replay stepping now have exported helpers that the normal windowed path already uses.
- Step 3 has started with a D2-only scaffold target in the existing host build. It currently proves the separate-console-target wiring and demo metadata loading path.
- The next implementation slice is to move from metadata-only loading to checkpoint restore and per-frame replay execution inside the new console target without calling the interactive SDL startup path.