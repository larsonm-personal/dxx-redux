# Plan: Input Demo Helper Extraction Survey (2026-05-04)

## Goal
- Survey D1/D2 original game files for input-demo-only helper functions that should move into dedicated input-demo files
- Produce a work list that keeps original 1996 game files limited to small call sites and declarations where possible
- Classify candidates as D1-only, D2-only, duplicated D1/D2, shared android input-demo code, or truly engine-local glue

## Scope
- Read-only survey plus work list creation
- Do not move code in this phase
- Prefer fresh files in `d1/` and `d2/` for game-specific input-demo helpers, and `android/` shared files for cross-game or android-specific input-demo helpers

## Execution Plan
- Find `input_demo_*` functions and nearby helper blocks inside existing D1/D2 game files
- Identify which helpers are already in dedicated input-demo files and which are embedded in original files
- Map each embedded helper to a proposed destination and note required call-site changes
- Write the extraction work list into this plan file

## Status
- Phase 1 complete
  - surveyed static and public `input_demo_*` functions in original D1/D2 `main/*.c` files
- Phase 2 complete
  - classified helper clusters by destination and extraction risk
- Phase 3 complete
  - work list below captures the recommended move order and target files
- Phase 4 complete
  - extracted D1/D2 replay metadata validation and replay startup command-line handling out of `inferno.c`
  - added `d1/main/input_demo_start.c` and `d1/main/input_demo_start.h`
  - expanded `d2/main/input_demo_start.c` and `d2/main/input_demo_start.h`
  - moved skip-level-intro input-demo state out of `gameseq.c` and into the dedicated start files for both games
  - validated with `run-windows-build.ps1 -Target both` and one focused D2 headless replay run
- Phase 5 complete
  - added `d1/main/input_demo_hooks.c` and `d1/main/input_demo_hooks.h`
  - added `d2/main/input_demo_hooks.c` and `d2/main/input_demo_hooks.h`
  - moved the top-of-`game.c` replay/result snapshot helpers out of the original D1/D2 `game.c` files and into the new hook files
  - moved D2 result kill-baseline state and its updater into `d2/main/input_demo_hooks.c` with the extracted result-capture helper
  - rewired `newdemo.c`, `game.c`, `game.h`, and both `main/CMakeLists.txt` files to use the dedicated hook headers instead of keeping those helper bodies in original files
  - validated with `run-windows-build.ps1 -Target both` and the focused D2 headless replay smoke test
- Phase 6 complete
  - moved the remaining D1 replay mismatch logging, replay result writing, replay prepare/advance flow, and mine-exit replay finish helper out of `d1/main/game.c` into `d1/main/input_demo_hooks.c`
  - moved the remaining D2 replay mismatch logging, replay result writing, replay prepare/advance flow, and level-exit and mine-exit replay finish helpers out of `d2/main/game.c` into `d2/main/input_demo_hooks.c`
  - intentionally left `input_demo_step_replay_frame` in both original `game.c` files because it still reaches the local `time_paused` state, and left the D2 tail/fire probe helpers in `game.c` because they still directly instrument the local game-loop stages
  - validated with `run-windows-build.ps1 -Target d1`, `run-windows-build.ps1 -Target d2`, and the focused D2 headless replay smoke test
- Phase 7 complete
  - moved `input_demo_step_replay_frame` out of both original `game.c` files and into the per-game `input_demo_hooks.c` files without changing its public signature
  - added small `game_is_time_paused()` accessors in `d1/main/game.c` and `d2/main/game.c` so the extracted wrapper can still honor the local pause state without moving broader pause control into new files
  - kept the D2 tail/fire probe helpers in `d2/main/game.c` because they still directly instrument local game-loop stages
  - validated with `run-windows-build.ps1 -Target d1`, `run-windows-build.ps1 -Target d2`, and the focused D2 headless replay smoke test
- Phase 8 complete
  - moved the extracted replay-helper declarations out of `d1/main/game.h` and `d2/main/game.h` into the per-game `input_demo_hooks.h` files
  - rewired the remaining non-`game.c` consumers to include the hook headers directly, including the SDL event loops, mine-exit and endlevel callers, and the headless replay runner
  - narrowed `input_demo_hooks.h` to opaque forward declarations for result and state-trace types so SDL code can include it without pulling in the shared trace header, and added direct includes only where full struct definitions are needed
  - validated with `run-windows-build.ps1 -Target d1`, `run-windows-build.ps1 -Target d2`, and the focused D2 headless replay smoke test
- Phase 9 complete
  - moved the D2 `aipath.c` path-probe helper block and its private path-probe state into `d2/main/input_demo_hooks.c`
  - kept `d2/main/aipath.c` down to local call sites plus narrow `extern` declarations instead of widening `d2/main/input_demo_hooks.h` with `object` and `point_seg` signatures that no other file needs yet
  - aligned the plan with the already-completed move of the D2 replay tail/fire/energy probe helpers out of `d2/main/game.c`
  - validated with `run-windows-build.ps1 -Target d2`, the focused D2 headless replay smoke test, and `android\gradlew.bat ":app:buildCMakeDebug[arm64-v8a]-2"`
- Phase 10 complete
  - moved the D2 `object.c` robot lifecycle helper block into `d2/main/input_demo_hooks.c`
  - kept `d2/main/object.c` down to local call sites plus narrow `extern` declarations, matching the `aipath.c` extraction pattern
  - validated with `run-windows-build.ps1 -Target d2`, the focused D2 headless replay smoke test, and `android\gradlew.bat ":app:buildCMakeDebug[arm64-v8a]-2"`
- Phase 11 complete
  - moved D2 awareness-source ownership out of `d2/main/ai.c` and into `d2/main/input_demo_hooks.c`
  - moved the public `input_demo_set_awareness_source` declaration out of `d2/main/ai.h` and into `d2/main/input_demo_hooks.h`, then rewired the real callers in `ai.c`, `ai2.c`, `collide.c`, and `laser.c`
  - kept the new awareness-source consume helper local to `ai.c` via a narrow `extern` declaration instead of widening `input_demo_hooks.h` with an internal-only API
  - validated with `run-windows-build.ps1 -Target d2`, the focused D2 headless replay smoke test, and `android\gradlew.bat ":app:buildCMakeDebug[arm64-v8a]-2"`
- Phase 12 complete
  - moved the D2 `ai2.c` robot-fire probe helper block into `d2/main/input_demo_hooks.c`
  - kept `d2/main/ai2.c` down to local call sites plus narrow `extern` declarations, matching the earlier `aipath.c` and `object.c` extraction pattern
  - validated with `run-windows-build.ps1 -Target d2`, the focused D2 headless replay smoke test, and `android\gradlew.bat ":app:buildCMakeDebug[arm64-v8a]-2"`
- Phase 13 complete
  - moved the longest D2 `object.c` robot visual-state logging printers into `d2/main/input_demo_hooks.c` so the legacy file keeps short call sites instead of formatter-expanded `con_printf` argument lists
  - kept the extracted helpers file-local to `object.c` via narrow `extern` declarations instead of growing `d2/main/input_demo_hooks.h`
  - validated with `run-windows-build.ps1 -Target d2`, the focused D2 headless replay smoke test, and `android\gradlew.bat ":app:buildCMakeDebug[arm64-v8a]-2"`
- Phase 14 complete
  - moved the D2 `ai.c` robot pose and view-probe helper block into `d2/main/input_demo_hooks.c`, including the long pose-tracking log bodies that were inflating the legacy file diff
  - kept `d2/main/ai.c` down to a single local `extern` plus the existing `input_demo_trace_tracked_robot_poses()` call site in `init_ai_frame()`
  - validated with `run-windows-build.ps1 -Target d2`, the focused D2 headless replay smoke test, and `android\gradlew.bat ":app:buildCMakeDebug[arm64-v8a]-2"`

## Existing Dedicated Input Demo Files
- Keep using `d2/main/input_demo_start.c` and `d2/main/input_demo_start.h` for D2 replay startup logic
- Keep using `d1/main/input_demo_control_info.h`, `d2/main/input_demo_control_info.h`, and `d2/main/input_demo_energy_trace.h` as existing dedicated input-demo files
- Keep generic codec, replay, recorder, state trace, RNG trace, controls, result, fixture, FP, and debug logging code under `android/app/src/main/cpp/shared/input_demo_*`

## Preferred Destination Shape
- Add one umbrella file per game for game-specific input-demo hooks first:
  - `d1/main/input_demo_hooks.c` and `d1/main/input_demo_hooks.h`
  - `d2/main/input_demo_hooks.c` and `d2/main/input_demo_hooks.h`
- Use sections inside those files for path, AI, collision, physics, render, recorder, frame/replay, and direct command helpers
- Split later only if include dependencies or file size become painful; the main goal is to get helper bodies out of original files
- Add both new `.c` files to `d1/main/CMakeLists.txt` and `d2/main/CMakeLists.txt`
- Add `android/app/src/main/cpp/shared/input_demo_probe_utils.c` and `.h` only for truly generic helpers shared by D1/D2 or by multiple D2 hook sections

## Android Shared Utility Candidates
- `input_demo_path_probe_hash_add`, `input_demo_path_probe_hash_text`, `input_demo_trace_hash_label`, and similar state-key hashing helpers
  - Move as generic `input_demo_probe_hash_*` helpers if more than one extraction cluster uses them
  - Otherwise keep them in the D2 umbrella file to avoid a fake abstraction
- The repeated D2 `input_demo_record_frame_event_json` helper in `collide.c`, `laser.c`, and `gauges.c`
  - Best target: a shared helper that wraps `input_demo_recorder_append_frame_event_json` and one-shot error logging
  - If `con_printf` include coupling makes `android/` awkward, put the helper in `d2/main/input_demo_hooks.c` first
- Debug activity helpers are already in `android/app/src/main/cpp/shared/input_demo_debug_logging.*`; do not duplicate them back into D1/D2
- Do not move object, AI, segment, wall, weapon, or player inspection code into `android/`; those helpers know too much about each game's engine structs

## D1 Work List

### D1 replay and result frame helpers
- Current file: `d1/main/game.c`
- Move to: `d1/main/input_demo_hooks.c`, frame/replay section
- Status: mostly completed across the second through fourth implementation tranches
- Functions:
  - `input_demo_record_game_frame`
  - `input_demo_update_rng_trace_context`
  - `input_demo_count_live_objects_of_type`
  - `input_demo_capture_state_trace_diag`
  - `input_demo_capture_current_result`
  - `input_demo_write_replay_frame_state_trace`
  - `input_demo_log_replay_frame_state_mismatch`
  - `input_demo_log_current_replay_frame_state_mismatch`
  - `input_demo_write_replay_result`
  - `input_demo_finish_replay_without_close`
  - `input_demo_stop_replay`
  - `input_demo_delay_replay_frame`
  - `input_demo_prepare_replay_frame`
  - `input_demo_advance_replay_frame`
  - `input_demo_step_replay_frame`
  - `input_demo_finish_replay_from_mine_exit`
- Original file should keep only small calls from the game loop and exit paths
- Public declarations now live in `d1/main/input_demo_hooks.h`, with original callers including that header where needed
- Completed in this tranche:
  - `input_demo_record_game_frame`
  - `input_demo_update_rng_trace_context`
  - `input_demo_count_live_objects_of_type`
  - `input_demo_capture_state_trace_diag`
  - `input_demo_capture_current_result`
- Completed in the next tranche:
  - `input_demo_write_replay_frame_state_trace`
  - `input_demo_log_replay_frame_state_mismatch`
  - `input_demo_log_current_replay_frame_state_mismatch`
  - `input_demo_write_replay_result`
  - `input_demo_finish_replay_without_close`
  - `input_demo_stop_replay`
  - `input_demo_delay_replay_frame`
  - `input_demo_prepare_replay_frame`
  - `input_demo_advance_replay_frame`
  - `input_demo_finish_replay_from_mine_exit`
- Completed in the latest tranche:
  - `input_demo_step_replay_frame`
- Remaining in `d1/main/game.c` for now:
  - `game_is_time_paused`

### D1 replay startup and metadata helpers
- Current file: `d1/main/inferno.c`
- Move to: `d1/main/input_demo_hooks.c`, startup section, or a new `d1/main/input_demo_start.c` if matching D2 is preferred
- Status: completed in the first implementation tranche using new `d1/main/input_demo_start.c` and `d1/main/input_demo_start.h`
- Functions:
  - `maybe_validate_input_demo_metadata`
  - `input_demo_replay_hash_u8_sequence`
  - `input_demo_apply_replay_player_cfg`
  - `input_demo_maybe_start_replay_from_cmdline`
- Original `inferno.c` should keep only command dispatch calls
- The hash helper is a candidate for `android/app/src/main/cpp/shared/input_demo_probe_utils.*` if shared with D2 start logic

### D1 quick recording and recorder setup
- Current file: `d1/main/newdemo.c`
- Move to: `d1/main/input_demo_hooks.c`, recorder section
- Functions:
  - `input_demo_quick_record_mission_name`
  - `input_demo_clear_quick_recording`
  - `input_demo_release_recorder_settings`
  - `input_demo_capture_recorder_checkpoint`
  - `input_demo_is_mid_level_record_start`
  - `input_demo_ascii_equal_ignore_case`
  - `input_demo_fill_recorder_player_cfg`
  - `input_demo_strip_hog_extension`
  - `input_demo_sanitize_slug`
  - `input_demo_prepare_recorder_settings`
  - `input_demo_build_quick_record_name`
  - `input_demo_build_classic_demo_path`
  - `input_demo_delete_classic_demo_sidecar`
  - `input_demo_trim_new_recordings`
  - `maybe_start_input_demo_recording`
  - `maybe_flush_input_demo_recording`
- Original `newdemo.c` should keep only calls at record start/stop points

### D1 collision, physics, fireball, and FVI probes
- Current files: `d1/main/collide.c`, `d1/main/physics.c`, `d1/main/fireball.c`, `d1/main/fvi.c`
- Move to: `d1/main/input_demo_hooks.c`, probe section
- Candidate helpers:
  - collision: `input_demo_trace_collision_pose_active`, `input_demo_trace_collision_frame_index`, `input_demo_trace_collision_mode_name`, `input_demo_trace_player_bump_active`, `input_demo_log_player_bump_probe`, `input_demo_log_player_robot_contact_probe`, `input_demo_trace_local_player_weapon_robot_active`, `input_demo_log_weapon_robot_accept_seq`
  - physics: `input_demo_trace_motion_probe_active`, `input_demo_trace_motion_frame_index`, `input_demo_trace_motion_mode_name`, `input_demo_player_robot_hit_object_probe_active`, `input_demo_log_player_robot_hit_object_probe`
  - fireball: `input_demo_trace_explosion_probe_active`, `input_demo_trace_explosion_frame_index`, `input_demo_trace_explosion_mode_name`, `input_demo_exploding_robot_probe_active`, `input_demo_log_exploding_object_probe`
  - FVI: `fvi_input_demo_frame_index`, `fvi_input_demo_mode_name`, `input_demo_log_fvi_weapon_robot_check`
- Original files should keep only calls like `input_demo_log_*` near the instrumented engine operation

### D1 level intro control
- Current file: `d1/main/gameseq.c`
- Move to: `d1/main/input_demo_hooks.c`, startup/control section
- Status: startup skip-intro state and setter completed in the first implementation tranche via `d1/main/input_demo_start.c`
- Function:
  - `input_demo_set_skip_level_intro`
- Keep the call site in level startup code small

## D2 Work List

### D2 replay startup and command-line glue
- Current file: `d2/main/inferno.c`
- Move to: existing `d2/main/input_demo_start.c` and `d2/main/input_demo_start.h`
- Status: completed in the first implementation tranche
- Functions:
  - `maybe_validate_input_demo_metadata`
  - `input_demo_maybe_start_replay_from_cmdline`
- Original `inferno.c` should keep only the high-level startup call
- Keep existing `input_demo_load_replay_from_path` and `input_demo_start_loaded_replay` in `input_demo_start.c`

### D2 replay and result frame helpers
- Current file: `d2/main/game.c`
- Move to: `d2/main/input_demo_hooks.c`, frame/replay section
- Status: mostly completed across the second through fourth implementation tranches
- Functions:
  - `input_demo_record_game_frame`
  - `input_demo_update_rng_trace_context`
  - `input_demo_count_live_objects_of_type`
  - `input_demo_count_live_player_weapons`
  - `input_demo_capture_state_trace_diag`
  - `input_demo_capture_current_result`
  - result kill baseline state and helpers
  - `input_demo_replay_tail_probe_frame_active`
  - `input_demo_log_current_replay_frame_state_mismatch`
  - `input_demo_replay_fire_probe_active`
  - `input_demo_log_replay_energy_stage`
  - `input_demo_write_replay_result`
  - `input_demo_finish_replay_from_level_exit`
  - `input_demo_finish_replay_from_mine_exit`
  - `input_demo_stop_replay`
  - `input_demo_delay_replay_frame`
  - `input_demo_prepare_replay_frame`
  - `input_demo_advance_replay_frame`
  - `input_demo_finish_replay_without_close`
  - `input_demo_step_replay_frame`
- Original `game.c` should keep only game-loop and exit-path calls
- Public declarations now live in `d2/main/input_demo_hooks.h`
- Completed in this tranche:
  - `input_demo_record_game_frame`
  - `input_demo_update_rng_trace_context`
  - `input_demo_count_live_objects_of_type`
  - `input_demo_capture_state_trace_diag`
  - `input_demo_capture_current_result`
  - result kill baseline state and its updater
- Completed in the next tranche:
  - `input_demo_log_current_replay_frame_state_mismatch`
  - `input_demo_write_replay_result`
  - `input_demo_finish_replay_from_level_exit`
  - `input_demo_finish_replay_from_mine_exit`
  - `input_demo_stop_replay`
  - `input_demo_delay_replay_frame`
  - `input_demo_prepare_replay_frame`
  - `input_demo_advance_replay_frame`
  - `input_demo_finish_replay_without_close`
- Completed in the latest tranche:
  - `input_demo_step_replay_frame`
- Completed after the latest tranche:
  - `input_demo_count_live_player_weapons`
  - `input_demo_replay_tail_probe_frame_active`
  - `input_demo_replay_fire_probe_active`
  - `input_demo_log_replay_energy_stage`
- Remaining in `d2/main/game.c` for now:
  - `game_is_time_paused`

### D2 path and guidebot path probes
- Current file: `d2/main/aipath.c`
- Move to: `d2/main/input_demo_hooks.c`, path section
- Status: completed in the current tranche
- Functions and state:
  - `input_demo_should_match_android_companion_velocity`
  - `input_demo_trace_frame_index`
  - `input_demo_trace_path_active`
  - `input_demo_replay_path_probe_active`
  - `input_demo_replay_follow_probe_active`
  - `input_demo_replay_path_request_probe_active`
  - `input_demo_path_probe_state`
  - `g_input_demo_last_path_detail`
  - `g_input_demo_last_path_points`
  - `input_demo_path_probe_hash_add`
  - `input_demo_path_probe_hash_text`
  - `input_demo_log_path_robot_state`
  - `input_demo_log_path_request`
  - `input_demo_log_path_probe`
  - `input_demo_log_path_detail`
  - `input_demo_log_path_points`
- Original `aipath.c` now keeps only calls at path creation, follow, detail, and velocity adjustment points, plus narrow local `extern` declarations for the extracted helpers
- Exact examples from the user belong in this cluster

### D2 robot lifecycle and object probes
- Current file: `d2/main/object.c`
- Move to: `d2/main/input_demo_hooks.c`, robot/object section
- Status: partially completed for lifecycle helpers plus the longest visual-state logging printers
- Functions:
  - `input_demo_trace_ai_rng_active`
  - `input_demo_robot_lifecycle_probe_active`
  - `input_demo_robot_visual_probe_active`
  - `input_demo_robot_lifecycle_is_target`
- Original `object.c` now keeps the local call sites and only the shorter remaining visual/debug logging, while the lifecycle predicate helpers and longest visual-state printers live in `d2/main/input_demo_hooks.c`
- Prefer replacing `input_demo_robot_lifecycle_is_target` call sites with a single `input_demo_log_robot_lifecycle_delete(objnum, obj)` helper so `object.c` does not keep target-selection details

### D2 AI and awareness probes
- Current files: `d2/main/ai.c`, `d2/main/ai2.c`
- Move to: `d2/main/input_demo_hooks.c`, AI section
- Status: partially completed for awareness-source ownership, the `ai2.c` robot-fire probe helpers, and the `ai.c` robot pose/view probe block
- Functions and state:
  - `Input_demo_awareness_source_tag`
  - `Input_demo_awareness_source_objnum`
  - `Input_demo_awareness_aux_objnum`
  - `input_demo_trace_ai_active`
  - `input_demo_trace_robot_pose_active`
  - `input_demo_trace_robot_pose_mode_name`
  - `input_demo_trace_frame_index`
  - `input_demo_trace_view_probe` struct
  - `input_demo_trace_robot_view_probe`
  - `input_demo_trace_robot_is_in_view`
  - `input_demo_trace_tracked_robot_poses`
  - `input_demo_replay_awareness_probe_active`
  - `input_demo_awareness_probe_mode_name`
  - `input_demo_set_awareness_source`
  - `input_demo_trace_ai_robot_active`
  - `input_demo_log_ai_robot_state`
  - `ai2.c` robot fire helpers: `input_demo_trace_robot_fire_active`, `input_demo_log_robot_fire_state`
- Original AI files should keep only probe/log calls near the AI transitions
- Completed in the current tranches:
  - awareness-source state ownership and setter declaration move
  - `ai2.c` robot fire helpers
  - `ai.c` robot pose/view probe helpers and tracked-pose logging block
- Remaining in `ai.c` and `ai2.c` for now:
  - awareness probe mode/activity helpers
  - AI robot trace/log helpers

### D2 weapon, collision, score, and frame-event recording
- Current files: `d2/main/laser.c`, `d2/main/collide.c`, `d2/main/gauges.c`, `d2/main/fireball.c`, `d2/main/fvi.c`
- Move to: `d2/main/input_demo_hooks.c`, combat/event section, plus `android` utility if shared event append is extracted
- Functions:
  - `laser.c`: spreadfire and weapon lifetime/focus/create helpers, suspect spreadfire tracking, `input_demo_log_weapon_lifetime`, `input_demo_record_weapon_create_event`, `input_demo_record_player_shot_event`, `input_demo_record_spreadfire_emit_event`
  - `collide.c`: wall impact, robot damage, robot impact, player damage, collision pose/player bump helpers, powerup probe helper
  - `gauges.c`: `input_demo_record_score_event`
  - `fireball.c`: explosion probe helpers
  - `fvi.c`: FVI weapon/robot check helpers
- Consolidate repeated `input_demo_record_frame_event_json` bodies during this move

### D2 movement, controls, render, UI RNG, and escort probes
- Current files: `d2/main/physics.c`, `d2/main/controls.c`, `d2/main/render.c`, `d2/main/gamerend.c`, `d2/main/escort.c`
- Move to: `d2/main/input_demo_hooks.c`, movement/render/escort sections
- Functions:
  - `physics.c`: motion probe helpers, player weapon threat, drag/detail state-key helpers, physics fate helpers, player/robot hit-object helpers
  - `controls.c`: player control and wiggle probe helpers
  - `render.c`: render probe active/list/skip helpers and skipped robot logging state
  - `gamerend.c`: preserved UI RNG helpers and logging
  - `escort.c`: escort active/frame/hash/fix-bucket/snapshot helpers, escort RNG/path/state/snipe/thief detail logging, escort state probe reset, and related snapshot globals
- Original files should retain the actual engine branch plus one or two `input_demo_*` calls where the observation happens

### D2 direct commands and replayed actions
- Current files: `d2/main/gamecntl.c`, `d2/main/automap.c`, `d2/main/escort.c`, `d2/main/gameseq.c`, `d2/main/newdemo.c`
- Move to: `d2/main/input_demo_hooks.c`, commands/recorder section
- Status: the `gameseq.c` skip-level-intro piece is already completed in the first implementation tranche via `d2/main/input_demo_start.c`
- Functions:
  - `gamecntl.c`: direct command record helpers, direct command replay apply/abort helpers
  - `automap.c`: `input_demo_apply_recorded_marker_drop`
  - `escort.c`: `input_demo_apply_recorded_guidebot_goal`
  - `gameseq.c`: `input_demo_set_skip_level_intro`
  - `newdemo.c`: quick recording helpers matching the D1 list
- Original files should keep domain actions like `DropMarker`, `set_escort_special_goal`, and record start/stop hooks, but not input-demo parsing or staging details

## Suggested Move Order
- First: add D1/D2 umbrella files, headers, and CMake entries with no behavior change
- Second: move low-risk duplicated utility helpers, especially event append and hash/state-key helpers
- Third: move replay startup from D1/D2 `inferno.c`, using existing D2 `input_demo_start.c` and a new D1 equivalent or the D1 umbrella file
- Fourth: move frame/replay/result helpers out of `game.c` in both games; this gives the biggest diff reduction in core loop files
- Fifth: move recorder helpers out of `newdemo.c` in both games
- Sixth: move D2 probe-heavy clusters in this order: path/object, combat/event, AI/awareness, movement/controls, escort/render/UI RNG
- Seventh: move D1 probe clusters in this order: collision/physics/fireball/FVI

## Validation Plan For Implementation Phase
- Run `android/run-code-quality.ps1 --fix` after code movement
- Run `run-windows-build.ps1 -Target d1` and `run-windows-build.ps1 -Target d2`
- Run one focused input-demo replay wrapper for D2 after each high-risk cluster move
- For D1/D2 frame/replay helper moves, also run the host runtime smoke script if its unrelated state-trace assertion has been fixed or is explicitly skipped