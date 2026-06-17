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
5. [completed] Replace the diagnostic start-from-level path with a fuller D1 object stream translation that restores object slots, segment links, player object physics, robot objects, weapons, fireballs, and powerups.
6. [completed] Add D1 wall, door, trigger, fuelcen/matcen, and control-center translation.
7. [completed] Add D1 AI static/local state translation and RNG state alignment, then rerun the D1-in-D2 replay without the start-from-level escape hatch.

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
- Player physics restore build passed and the partial replay reached frame 2284 before the 120 second timeout. Comparing the partial trace showed the frame-1 position drift was fixed; after ignoring the known D1 `robots_killed` summary mismatch, the next frame-0 mismatch was missing saved object state: expected one fireball/live-object allocation already present in the D1 checkpoint.
- Object-stream slice now in progress: parse D1 saved object slots from the checkpoint in `d1_save_translate.c`, translate movement/control/render unions field-by-field into D2 objects, rebuild segment object links, and reset the object allocator with `special_reset_objects()`. Wall/door/trigger/fuelcen and AI-local sections remain separate follow-up work.
- Object-stream translation build passed and the replay completed all 2737 frames. Frame 0 now matches the D1 trace for live-object hash, fireball count, segment object list, robot count, and object allocator count. The first selected-field divergence became an extra fireball at frame 1, caused by `StartNewGame` leaving `Do_appearance_effect` armed; native D1/D2 restore paths clear that flag before resuming mid-level, so the D1 translator now clears it too.
- Runtime-state translation now restores the D1 checkpoint tail for weapon timers, RNG state, d-tick state, object allocator/free list/signature seed, homer counters, laser runtime state, and AI path runtime. Frame 0 now matches the D1 trace for the full runtime hash. The RNG path still diverges at D1 frame 7 vs D2 frame 28, so the next slice restores D1's saved AI-local/path memory instead of merely skipping over the AI state block.
- AI-local/path translation now restores D1 `Ai_local_info`, `Point_segs`, cloak slots, boss timers, awareness events, and believed-player position into the D2 runtime layout. The D1-in-D2 replay completes all 2737 frames.
- World-state translation now restores D1 walls, active doors, triggers, segment side textures and wall links, robot centers, fuel centers, control-center state, and countdown state. The D1-in-D2 replay still completes all 2737 frames.
- Latest save-translation replay:
  `.\android\tests\run_input_demo_replay.ps1 -DemoPath C:\local\dxx-redux\android\regression_demos\d1_descent_level4_20260616_121645.dximdemo -Game d2 -D1InD2 -D1InD2StartFromLevel -Mode accelerated -Runner fast -StateLogPath temp\d1_level4_121645_d1_in_d2_save_translate_world_state.jsonl -RngLogPath temp\d1_level4_121645_d1_in_d2_save_translate_world_rngtrace.jsonl -ReplayDebugLog -SkipExpectedChecks -AllowMissingActualResult -KeepSandbox -TimeoutSeconds 180`
- Remaining selected-state divergence is no longer a frame-0 restore failure. The first selected mismatch is frame 134, where D1 has one additional weapon/fireball/live object, and D1 awards the first 200 score at frame 156 while D1-in-D2 awards it at frame 158. That points to projectile spawn/collision timing.
- D1-in-D2 is already loading D1 `Weapon_info`; frame-134 D2 laser creation logs show laser ID 0 velocity magnitude at D1 speed. The next likely compatibility gap is D1 player-ship data, especially `Player_ship->gun_points`, which D1-in-D2 currently seeks past but does not apply.

Projectile semantics phase:

1. [completed] Apply D1 `player_ship` data under the D1-in-D2 asset overlay, saving and restoring the D2 ship when the overlay deactivates.
2. [completed] Rebuild D2 and rerun `d1_descent_level4_20260616_121645.dximdemo` with state and RNG logs.
3. [completed] Compare the first selected mismatch and first score-award frame against the D1 trace.
4. [in progress] Align D1-in-D2 robot fire/cooldown semantics before deeper projectile collision work.
5. [in progress] If the robot-fire object/fireball mismatch remains, add scoped D1/D2 robot-fire instrumentation and fix the next engine semantic difference it exposes.

Projectile semantics notes:

- D1 player-ship asset overlay is now active while D1 robot/weapon assets are active. It reads the D1 `player_ship` record from `descent.pig`, applies the D1 gun points/control constants/model reference, updates live player objects, and restores the D2 ship when the overlay deactivates.
- Verification after the player-ship overlay:
  `.\android\tests\run_input_demo_replay.ps1 -DemoPath C:\local\dxx-redux\android\regression_demos\d1_descent_level4_20260616_121645.dximdemo -Game d2 -D1InD2 -D1InD2StartFromLevel -Mode accelerated -Runner fast -StateLogPath temp\d1_level4_121645_d1_in_d2_player_ship_state.jsonl -RngLogPath temp\d1_level4_121645_d1_in_d2_player_ship_rngtrace.jsonl -ReplayDebugLog -SkipExpectedChecks -AllowMissingActualResult -KeepSandbox -TimeoutSeconds 180`
- The ship overlay is correct asset compatibility work but did not move the first mismatch. Compact frame comparison showed the frame-134 difference is not the player laser pair: both D1 and D1-in-D2 still have `player_weapon_count=10` at frame 134. D1 has one extra non-player weapon and one fireball (`weapon_object_count=11`, `fireball_object_count=1`), while D1-in-D2 still has `10` and `0`.
- The next focused change is D1 robot-fire cooldown semantics. D2 has secondary-weapon and snipe/free-shot cooldown handling; D1 has one fire timer and always advances `rapidfire_count`. D1-in-D2 now uses D1's `set_next_fire_time` behavior when running D1 missions.
- D1 cooldown semantics did not move the frame-134 mismatch. Added a D1-side robot-fire probe so native D1 replay logs can be compared against the existing D2 robot-fire probe around frames 132-136.
- Follow-up trace comparison moved the root cause earlier than the frame-134 projectile symptom. D1 and D1-in-D2 match robot/live-object hashes through frame 33, then D1 starts changing robot state at frame 34 while D1-in-D2 keeps the saved robot state until much later. The likely semantic gap is D2's continuous AI time-slice gate versus D1's older stepped distance thresholds, so the next implementation branch should use D1 AI scheduling thresholds when D1-in-D2 is active.
- The D1 AI time-slice thresholds improve the final replay result substantially but do not move the first robot hash mismatch. New shared state-trace fields identify D1 object 58, robot id 1 in segment 20, as the first changing robot. Its position stays fixed while `rotvel` decays, which points to D1 AI/physics rotating a robot that D1-in-D2 leaves inert. The next concrete D1/D2 AI difference is robotmaker handling: D1 always calls `ai_follow_path` for robots in robotmaker segments before the time-slice gate, while D2 only does so when the D2 `Station` record is enabled.

D1-in-D2 gameplay semantics phase:

1. [completed] Add a mission-scoped D1 gameplay helper in `d2/main/d1_in_d2.*` and use it for D1-in-D2-only behavior branches.
2. [completed] Match D1 rotational drag for D1-in-D2 physics simulation.
3. [completed] Match D1 AI path creation semantics for random side-order refresh, outside-point mutation, station-path polishing, and center-point insertion/pruning.
4. [completed] Match D1 AI idle/visibility wake behavior for station and still robots.
5. [completed] Match D1 robot-vs-robot FVI filtering for attack robots in D1-in-D2.
6. [completed] Match D1 weapon-fire awareness behavior by suppressing D2's player-laser-fire awareness event in D1-in-D2.
7. [completed] Match D1 visibility ray acceptance more closely by using `FQ_CHECK_OBJS` and accepting player-object hits for D1-in-D2.
8. [completed] Suppress D2-only blind-fire logic for D1-in-D2.
9. [in progress] Resolve the remaining object-14 frame-132 visibility/turning split.
10. [completed] Add matching D1/D2 AI visibility diagnostics before recording the next fresh demo.

D1-in-D2 gameplay semantics notes:

- Verified after each slice with scoped `.\android\run-code-quality.ps1 -Fix -Paths ...` and `.\run-windows-build.ps1 -Target d2`; shared trace selector changes also rebuilt D1.
- Native D1 verification with object-14 focus:
  `.\android\tests\run_input_demo_replay.ps1 -DemoPath C:\local\dxx-redux\android\regression_demos\d1_descent_level4_20260616_121645.dximdemo -Game d1 -Mode accelerated -Runner fast -StateLogPath temp\d1_level4_121645_d1_obj14_state.jsonl -RngLogPath temp\d1_level4_121645_d1_obj14_rngtrace.jsonl -ReplayDebugLog -SkipExpectedChecks -AllowMissingActualResult -KeepSandbox -TimeoutSeconds 180`
- Latest D1-in-D2 verification:
  `.\android\tests\run_input_demo_replay.ps1 -DemoPath C:\local\dxx-redux\android\regression_demos\d1_descent_level4_20260616_121645.dximdemo -Game d2 -D1InD2 -D1InD2StartFromLevel -Mode accelerated -Runner fast -StateLogPath temp\d1_level4_121645_d1_in_d2_obj14_no_blind_correct_state.jsonl -RngLogPath temp\d1_level4_121645_d1_in_d2_obj14_no_blind_correct_rngtrace.jsonl -ReplayDebugLog -SkipExpectedChecks -AllowMissingActualResult -KeepSandbox -TimeoutSeconds 180`
- Latest D1-in-D2 final state is still wrong but route is closer in position segment: energy 29, shields 29, score 2400, segment 71, robots_alive 58, robots_killed 6, powerups 19. Native D1 final remains energy 76, shields 97, score 7700, segment 71, robots_alive 45, robots_killed 64, powerups 16.
- Confirmed fixed frontiers:
  object 58 rotational drag mismatch at frame 34; object 59 path-randomization mismatch; object 29 station path/retry chain mismatch; object 7 station-to-normal wake and player-fire awareness mismatch.
- Current first focused mismatch is frame 132, focus slot/object 14. D1 object 14 has nonzero small `rotvel` from the AIM_STILL random-turn path; D1-in-D2 has zero `rotvel`. D2 trace shows object 14 visibility: frame 131 raw/final 0, then frame 132 raw/final 2, so it skips the D1 random-turn roll and immediately reacquires/fire-gates. Native D1 lacks the equivalent rich visibility hit trace, so the next best step is to add a small shared visibility/FVI hit diagnostic for D1 and D2 or extend the existing D2 hook to D1.
- Temporary diagnostic focus is currently hardcoded to object 14 in `android/app/src/main/cpp/shared/input_demo_hooks_shared.c`; clean this up or make it configurable before finalizing the change set.
- Diagnostic slice completed: mirrored D2's high-level `compute_vis_and_vec` visibility trace into D1 and added a focused object-14 `player_is_visible_from_object` FVI probe on both sides. The next fresh demo should tell us whether object 14 differs because the ray result differs (`Hit_type`, `Hit_data.hit_object`, hit segment/point, flags/startseg), because the field-of-view dot gate differs, or because the later AI visibility/awareness state differs.
- Verification after the diagnostic slice:
  `.\android\run-code-quality.ps1 -Fix -Paths @('d1\main\ai.c','d1\main\input_demo_hooks.c','d1\main\input_demo_hooks.h','d2\main\ai2.c','d2\main\input_demo_hooks.c','d2\main\input_demo_hooks.h','android\ai tool plans\input demo, replay, determinism\plan_d1_demo_d1_in_d2_runner_path_20260616.md')`
  plus `.\run-windows-build.ps1 -Target d1` and `.\run-windows-build.ps1 -Target d2` passed.
