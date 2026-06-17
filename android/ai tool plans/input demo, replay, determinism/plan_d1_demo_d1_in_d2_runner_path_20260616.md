# D1 input demo under D2 runner path

Goal: add an explicit replay path for running D1 input-demo recordings under the D2 executable so D1-in-D2 gameplay desyncs can be measured.

Plan:

1. [completed] Inspect replay metadata gating and runner selection for the current D1-recording forced-D2 failure.
2. [completed] Add an explicit wrapper option that selects the D2 executable while preserving D1 demo metadata expectations.
3. [completed] Allow the D2 input-demo loader to accept D1 recordings when the explicit D1-in-D2 path is requested.
4. [completed] Format/build D2 and run the new path on `d1_descent_level4_20260616_121645.dximdemo`.
5. [completed] Update notes with the first gameplay or restore failure exposed by the new path.

Notes:

- Current forced D2 run exits before gameplay with `Input demo replay currently supports D2 demos only`.
- The default D2 `-Runner fast` selection uses the dedicated headless-console executable for accelerated checkpoint demos. The D1-in-D2 path should use the full D2 executable first, because this is a gameplay compatibility path rather than a D2 headless regression path.
- Added `run_input_demo_replay.ps1 -D1InD2`, which requires a D1 recording, launches the D2 executable, requires both D2 and D1 core data files, disables headless-console selection, and passes `-inputdemo-d1-in-d2` to the engine.
- Added a flag-gated D2 loader path that accepts D1 recordings only when `-inputdemo-d1-in-d2` is present. Normal D1 and D2 replay loading remains strict.
- Verified D2 build with `.\run-windows-build.ps1 -Target d2`; build completed with existing `d2/main/weapon.c` C4715 warnings.
- New path run:
  `.\android\tests\run_input_demo_replay.ps1 -DemoPath C:\local\dxx-redux\android\regression_demos\d1_descent_level4_20260616_121645.dximdemo -Game d2 -D1InD2 -Mode accelerated -Runner fast -StateLogPath temp\d1_level4_121645_d1_in_d2_actual_state.jsonl -RngLogPath temp\d1_level4_121645_d1_in_d2_actual_rngtrace.jsonl -ReplayDebugLog -SkipExpectedChecks -AllowMissingActualResult -KeepSandbox -TimeoutSeconds 120`
- The D2 executable now accepts the D1 recording and gets to checkpoint restore. The first exposed blocker is `restore: unsupported save version 15 in 'inputdemo_start.dgss'` in `temp\input_demo_runtime_wrapper\d2\d1_descent_level4_20260616_121645\gamelog.txt`.
- Because restore stops before frame 0, the D1-in-D2 actual state trace only contains its metadata record and the RNG trace only contains its metadata record.

Follow-up plan:

1. [completed] Compare D1 and D2 checkpoint save headers and restore entry points around save version 15.
2. [completed] Add the smallest explicit D1-in-D2 diagnostic path needed to get past the incompatible checkpoint.
3. [completed] Rebuild D2 and rerun the D1 Level 4 replay path with state/RNG trace output.
4. [completed] Record the next exposed failure or first desync location.

Follow-up notes:

- A faithful D1 checkpoint reader inside D2 is not a small version-gate fix. D1 and D2 save streams differ in player/object/wall/trigger/matcen/AI serialized structs, and D2 inserts exploding-wall, cloaking-wall, marker, afterburner, palette flash, light, omega, and other sections that D1 version 15 checkpoints do not contain.
- Added an explicit diagnostic fallback: `run_input_demo_replay.ps1 -D1InD2 -D1InD2StartFromLevel`, which passes `-inputdemo-d1-in-d2-start-from-level`. This starts the recorded D1 level under the D2 executable instead of restoring the incompatible D1 checkpoint, then offsets `GameTime64` by the recording checkpoint start time.
- Diagnostic run completed all 2737 frames:
  `.\android\tests\run_input_demo_replay.ps1 -DemoPath C:\local\dxx-redux\android\regression_demos\d1_descent_level4_20260616_121645.dximdemo -Game d2 -D1InD2 -D1InD2StartFromLevel -Mode accelerated -Runner fast -StateLogPath temp\d1_level4_121645_d1_in_d2_start_from_level_state.jsonl -RngLogPath temp\d1_level4_121645_d1_in_d2_start_from_level_rngtrace.jsonl -ReplayDebugLog -SkipExpectedChecks -AllowMissingActualResult -KeepSandbox -TimeoutSeconds 120`
- Diagnostic result: PASS as a run path, with actual result written to `temp\input_demo_runtime_wrapper\d2\d1_descent_level4_20260616_121645\results\result.actual.json`.
- First state mismatch in diagnostic fallback is frame 0: `state.position.y` expected `-3533164`, actual `-3611615`; `state.level_summary.robots_killed` expected `64`, actual `0`. This reflects missing checkpoint state rather than a clean gameplay semantic desync.
- First RNG mismatch in diagnostic fallback is the first comparable RNG line: expected D1 `d1/main/aipath.c:create_random_xlate` at frame 7, actual D2 `d2/main/ai.c:do_ai_frame` at frame 61. This is also affected by starting from level state instead of the D1 checkpoint.

Save translation phase:

1. [completed] Move D1 checkpoint transition/parsing code out of shared replay startup and into new D2-specific save translation files.
2. [completed] Parse the D1 checkpoint header, mission, level, game time, player stats, selected weapons, difficulty, and player object pose from the in-memory checkpoint blob.
3. [completed] Use the parsed D1 checkpoint metadata for the diagnostic D1-in-D2 start-from-level path instead of ignoring the checkpoint entirely.
4. [completed] Rebuild and rerun the Level 4 D1-in-D2 diagnostic path, then compare the new frame-0 mismatch against the previous fallback.
5. [in progress] Replace the diagnostic start-from-level path with a fuller D1 object stream translation that restores object slots, segment links, player object physics, robot objects, weapons, fireballs, and powerups.
6. [pending] Add D1 wall, door, trigger, fuelcen/matcen, and control-center translation.
7. [pending] Add D1 AI static/local state translation and RNG state alignment, then rerun the D1-in-D2 replay without the start-from-level escape hatch.

Save translation notes:

- Keep D1 save transition code segregated under new files specific to D1 save translation. `input_demo_start_shared.c` should only dispatch to this code and continue handling replay lifecycle concerns.
- Added `d2/main/d1_save_translate.c` and `d2/main/d1_save_translate.h`. The shared replay starter now delegates D1 checkpoint metadata/player/object-0 pose extraction to that D2-only translation module.
- The recorded D1 checkpoint's mission field is blank, so the D1-in-D2 diagnostic branch falls back to the replay metadata mission name, which is already mapped to the D1-in-D2 mission filename.
- The first translation slice is intentionally partial. It now restores D1 header/player fields and the saved player object pose, but it still starts from the D2-loaded level's object set.
- Verification:
  `.\android\run-code-quality.ps1 -Fix -Paths @('android\app\src\main\cpp\shared\input_demo_start_shared.c','d2\main\d1_save_translate.c','d2\main\d1_save_translate.h')`
  and `.\run-windows-build.ps1 -Target d2` passed.
- Latest diagnostic run:
  `.\android\tests\run_input_demo_replay.ps1 -DemoPath C:\local\dxx-redux\android\regression_demos\d1_descent_level4_20260616_121645.dximdemo -Game d2 -D1InD2 -D1InD2StartFromLevel -Mode accelerated -Runner fast -StateLogPath temp\d1_level4_121645_d1_in_d2_save_translate_pose2_state.jsonl -RngLogPath temp\d1_level4_121645_d1_in_d2_save_translate_pose2_rngtrace.jsonl -ReplayDebugLog -SkipExpectedChecks -AllowMissingActualResult -KeepSandbox -TimeoutSeconds 120`
- Latest diagnostic result completed all 2737 frames and wrote `temp\input_demo_runtime_wrapper\d2\d1_descent_level4_20260616_121645\results\result.actual.json`.
- State compare now shows frame 0 position and forward vector match D1. Remaining frame 0 mismatch is `state.level_summary.robots_killed`: expected `64`, actual `0`. Frame 1 begins diverging in `position.y`, expected `-3534899`, actual `-3534605`, which points to missing D1 object physics/state beyond the player pose.
- RNG compare still fails at the first comparable RNG line: expected D1 `d1/main/aipath.c:create_random_xlate` at frame 7, actual D2 `d2/main/ai.c:do_ai_frame` at frame 61. This remains blocked by incomplete D1 object/AI state restore.
- Next slice in progress: extend the D1 object-0 parser to restore the saved player object's type/control/render fields, size, contents/lifeleft, and physics block. This is a narrow bridge toward the full object-stream translator and should specifically test whether the frame-1 motion drift was caused by missing velocity/thrust/drag state.
