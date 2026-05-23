# Plan: headless console demo runner

Date: 2026-04-30

Goal: create a true console input-demo regression runner that does not initialize SDL video, create a window, or run rendering, and that replays `.dximdemo` files as fast as possible with minimal output and final result reporting.

## Phases

| phase | task | status |
|---|---|---|
| 1 | Inventory current replay, engine startup, build targets, SDL/render dependencies, and existing regression scripts. | completed |
| 2 | Identify the smallest reusable game-logic entry point needed to load a checkpoint, run input frames, compare final state, and exit. | completed |
| 3 | Design a separate console target with its own `main.cpp` and minimal platform stubs for video, audio, input polling, and frame presentation. | completed |
| 4 | Add a first compile-only host target that links the replay parser/result code and documents missing engine dependencies. | completed |
| 5 | Extend the target to execute a single D2 `.dximdemo` without SDL video and print only PASS/FAIL plus result details. | completed |
| 6 | Update regression scripts to prefer the console runner when available and keep the windowed runner as a fallback. | completed |
| 7 | Add timing output and validation so large demo batches can track speed and deterministic final state. | completed |

## Notes

- The existing `-inputdemo-norender` path is not the final goal. It still initializes SDL/OpenGL and keeps render traversal for determinism.
- A prior experiment showed that simply skipping `game_render_frame()` changes simulation because render traversal feeds rendered-object data used by robot wake-up logic.
- The console runner should either preserve required non-visual render side effects explicitly or move those side effects out of rendering before removing rendering entirely.
- Default output should be quiet: one final PASS/FAIL line, elapsed time, replay frames, and the result JSON path or mismatch summary.

## Initial inventory findings

- `android/tests/run_input_demo_headless.ps1` now prefers the D2 console runner through the shared replay wrapper and falls back to the older no-present path when the console runner is not eligible.
- `android/tests/run_input_demo_regressions.ps1` now scans `android/regression_demos` and passes `-PreferHeadlessConsole` into the shared replay wrapper, so D2 checkpoint demos use the console runner when it exists while the existing wrapper remains the fallback path.
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
- Replay startup has now been extracted again inside D2 into `input_demo_start.c`/`input_demo_start.h`, with `input_demo_load_replay_from_path()` and `input_demo_start_loaded_replay()`. The normal `inferno.c` command-line replay path now calls the shared helper, and the console target reuses the same load helper.
- The console target now has a minimal non-window runtime probe in `android/app/src/main/cpp/headless/input_demo_headless_main.cpp`. It initializes memory, PhysFS, config, text, gamedata, piggy, and `init_game()`, disables nice-fps throttling, then attempts to run checkpoint-start demos through `input_demo_start_loaded_replay()` and `input_demo_step_replay_frame()`.
- In the current workspace, the runtime probe stops before replay start because no full Descent 2 desktop data set is available on the host search path. The validated failure is: `HEADLESS-RUN FAIL init could not find descent2.hog or d2demo.hog; pass -hogdir <dir> with Descent 2 data files`.

## Candidate first implementation slice

1. Extract D2 replay startup from `inferno.c` into a small exported helper that takes a demo path and returns a status after starting the level or restoring the checkpoint. Keep the existing `-inputdemo-replay` code as a caller of that helper.
2. Extract D2 replay stepping/result writing from `game.c` into exported helpers, for example `input_demo_replay_run_frame()` and `input_demo_replay_finish_without_window()`, while leaving `game_handler()` behavior unchanged.
3. Add a D2-only compile target first, in a dedicated non-OGL host configure if needed, with `android/app/src/main/cpp/headless/input_demo_headless_main.cpp` and a new headless arch backend.
4. Have the first runnable version execute only checkpoint-start demos, because that avoids menu/title/pilot selection paths and matches the regression fixture already in `android/regression_demos`.
5. After D2 passes the existing fixture, duplicate the minimal exported API and target shape for D1.

## Current progress

- Steps 1 and 2 are done in both D2 and D1. Replay startup and replay stepping now have exported helpers that the normal windowed path already uses.
- Step 3 is now real, not provisional. `buildd2/main/dxx-redux-d2-headless.exe` starts through the shared replay helpers, keeps gameplay and audio logic alive, suppresses window creation and rendering, and uses dummy SDL audio so the logic path stays intact without audible output.
- Step 5 is also done for the current D2 regression fixture. The headless runner replays `android/regression_demos/d2_descent2_level2_20260430_135527.dximdemo` to completion with real data via `-hogdir`, writes the `.actual.json` result, and prints the final `HEADLESS-RUN OK` summary line.
- Step 6 is now done. The shared replay wrapper can launch `buildd2/main/dxx-redux-d2-headless.exe` for D2 `save_checkpoint` demos in accelerated mode without reimplementing result comparison, timeout handling, or data-dir resolution in a second script.
- The batch regression wrapper now prefers the console runner and was revalidated on the current fixture. It selected `Runner: headless-console`, launched `dxx-redux-d2-headless.exe`, and still failed on the same shared `player0.shields expected 158, actual 154` mismatch as before.
- Step 7 is now done. `android/tests/run_input_demo_replay.ps1` prints per-run elapsed time and effective replay FPS after the result comparison, and `android/tests/run_input_demo_regressions.ps1` prints total batch elapsed time at the end.
- The timing pass was revalidated on the current fixture without changing replay outcome. The console-preferred regression run still selected `Runner: headless-console`, printed `Elapsed: 7.567s replay_fps=98.85` for the replay, and the batch summary still exited with the same known shield mismatch.
- The render-side parity issue that blocked deterministic host replay is resolved for the headless slice. Skipping rendering dropped robot `danger_laser_num` updates that normally come from render traversal, so the headless path now performs the required non-visual viewer-space update when the player fires.
- The current remaining replay mismatch is not headless-specific. The normal desktop replay and the headless replay now stop on the same final state for the current fixture: `energy=81 shields=154 score=33200 lives=3 seg=79`. The embedded expected trailer still says `shields=158`, so any further investigation belongs to the broader replay determinism track rather than the headless parity track.
- The remaining 4-shield gap is now localized to two late robot-weapon hits, not a broad end-state drift. Frame 542 is a valid immediate hit from robot 109 via weapon `41/sig 4391`, created at frame 540 and landing two frames later.
- The frame 584 hit is downstream of an earlier late shot, not a fresh late spawn. Weapon `42/sig 4379` is created by robot 99 at frame 536, robot 99 then dies on frame 537 from three player spreadfire hits, and that already-spawned projectile lands on the player at frame 584 after its parent slot is gone.
- The next determinism step should treat frame 542 as the likely primary late divergence. The frame 584 hit still matters, but current evidence suggests it may be a cascade from earlier motion or collision drift rather than the first bad decision point.

## Follow-up 2026-05-02: temp_game_logs 231831 parity

- Target demo: `android/temp_game_logs/d2_descent2_level2_20260501_231831.dximdemo`
- Current headless loop now calls `timer_update()` before `input_demo_step_replay_frame()`
- Headless replay is still deterministically shifted versus windowed replay, with first state mismatch at frame 419 and RNG metadata shift showing same call counts but one-frame-earlier timing in headless
- Attempting to call `game_render_frame()` directly in the console loop, or forcing `SysInputDemoNoRender=0`, currently crashes the headless runner on host build
- Working hypothesis remains that render traversal side effects are required for strict parity, and that true parity will likely require either a safe no-present windowed path or additional headless-side replacement for render-dependent side effects