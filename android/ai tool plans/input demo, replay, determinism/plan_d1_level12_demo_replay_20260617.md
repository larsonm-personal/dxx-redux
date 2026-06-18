# D1 level 12 demo replay plan

Goal: verify the new D1 level 12 input demo in plain D1 first, then use it to drive d1-in-d2 compatibility fixes.

Demo files:

- `android/regression_demos/d1_descent_level12_20260617_204120.dem`
- `android/regression_demos/d1_descent_level12_20260617_204120.dximdemo`
- `android/regression_demos/d1_descent_level12_20260617_204120.dximdemo.rngtrace.jsonl`

Plan:

1. [done] Inspect demo metadata and trace availability.
2. [done] Run the `.dximdemo` in plain D1.
3. [done] If plain D1 fails, rerun with state/RNG tracing and fix D1 nondeterminism first.
4. [done] Once plain D1 is passing or understood, run as d1-in-d2.
5. [done] Fix d1-in-d2 desyncs in engine/save-translation logic, keeping demo-specific handling out unless it is purely diagnostic.
6. [done] Run scoped formatting/build/test validation for touched files.

Notes:

- Keep changes centralized in shared Android/input-demo helpers or dedicated D1-save-translation files where possible.
- Avoid hardcoding level 12 object numbers; use the trace fields to identify general engine state splits.
- The provided RNG trace sidecar currently contains only `{"type":"meta","version":1,"events":0,"truncated":false}`, so use embedded state/result checks first.
- First plain D1 attempt reached the post-death high-score screen and waited for input. Added a replay-only high-score bypass in D1 and D2 `scores_maybe_add_player()` so automated replay can finish after final death.
- Second plain D1 attempt got past high scores but reached pilot select. The real fix is to finalize replay at terminal game over, before front-end menus take over. Added D1/D2 `DoGameOver()` replay handling that writes the actual result, unloads replay, closes `Game_wind`, and returns.
- The terminal game-over hook now uses `input_demo_hooks.h` instead of local C-file prototypes. D2's hook header was made self-contained for the `vms_vector` forward declaration.
- With terminal replay completion in place, plain D1 now exits cleanly. The final result passes, while `-TraceState` still reports a diagnostic-only mismatch at frame 2602 in `robot_ai_static`.
- Added a semantic `input_demo_finish_replay_from_game_over()` hook in D1 and D2. D1 game-over replay completion now writes the expected terminal frame count instead of ending one frame short.
- Plain D1 accelerated replay without state tracing now passes the embedded result for `d1_descent_level12_20260617_204120.dximdemo`.
- D1-in-D2 accelerated replay now passes the embedded result for the same demo.
- Remaining caveat: `-TraceState` in plain D1 still reports a diagnostic-only `robot_ai_static` mismatch at frame 2602 while final result reproduction passes. Treat that as a separate trace fidelity issue unless it starts correlating with final-result drift.
