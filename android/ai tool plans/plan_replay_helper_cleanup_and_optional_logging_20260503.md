# Plan: Replay Helper Cleanup And Optional Logging (2026-05-03)

## Goal
- Make the non-headless replay helper expose optional robot labels
- Make helper-script numbered prompts choose option 1 when Enter is pressed
- Make replay investigation logging optional and move shared pieces under `android/` where practical

## Completion Status (2026-05-04)
- Phase 1 completed
  - `android/tests/run_input_demo_replay.ps1` now uses a shared numbered-choice helper and blank Enter selects option 1 for replay mode, render profile, and demo selection
  - The same option-1-on-Enter pattern was applied in `android/Run-TestMenu.ps1`, `android/1_build-aab.ps1`, `android/2_deploy-playstore.ps1`, and `android/tests/test_dual_emu.ps1`
- Phase 2 completed
  - `-inputdemo-replay-labels` now enables the replay label overlay in D2 and is accepted-and-ignored in D1 for helper parity
  - The replay frame counter stays tied to the same toggle as the robot labels
  - Validated with visual replay labels off by default and visual replay labels explicitly on, both passing
- Phase 3 completed
  - `android/app/src/main/cpp/shared/input_demo_debug_logging.h/.cpp` now provides a runtime gate instead of compile-time no-op stubs
  - Investigation probe families that were left in the tree are quiet by default and re-enabled by `-inputdemo-debug-log`
  - Validated with quiet default visual replay and explicit debug-log-on visual replay, both passing
- Phase 4 completed
  - Shared replay-debug declarations and helper implementation stayed under `android/app/src/main/cpp/shared/`
- Phase 5 completed
  - Windows host builds passed for D2 and D1
  - Replay smoke checks passed for visual labels-off, visual labels-on, debug-log-on, and headless replay in its supported accelerated mode
  - `android\run-code-quality.ps1 --fix` reported all checks passing in this environment, though it also printed `WARNING: Skipping missing path: --fix`

## Survey Findings

### 1. Replay robot labels are a separate always-on overlay
- `d2/main/render.c` accumulates `g_replay_robot_labels` for replay objects whenever `input_demo_replay_is_loaded()` is true
- `d2/main/gamerend.c` always draws the robot object-number labels and the replay frame counter during replay
- `d2/include/replay_debug_overlay.h` documents the overlay as active whenever replay is loaded
- There is no enable flag, config key, or `-inputdemo-*` command-line argument for this overlay today
- The older texture-label system under `debug_tex_overlay` is separate and should not be reused for the replay robot labels

### 2. Replay helper prompt defaults are inconsistent
- `android/tests/run_input_demo_replay.ps1` has three interactive numbered prompts with no Enter default today:
  - replay mode
  - render profile
  - demo selection
- `android/tests/run_input_demo_headless.ps1` and `android/tests/run_input_demo_regressions.ps1` are wrappers and inherit the replay helper behavior
- `android/run_test.ps1` already falls back to the first option for invalid or blank numbered script-parameter input
- `android/Run-TestMenu.ps1` still exits on blank or invalid test selection instead of defaulting to option 1
- `android/1_build-aab.ps1` already has an explicit Enter default of `[3]`, so it does not match the requested option-1 convention and should be treated as a separate script-specific case

### 3. Existing extracted replay-debug helper is present but mostly unused
- `android/app/src/main/cpp/shared/input_demo_debug_logging.h/.cpp` already exists as a shared seam for replay-debug logging
- That file is compiled into both D1 and D2, but `ENABLE_INPUT_DEMO_DEBUG_LOGGING` is not defined anywhere in the current tree
- Result: the active logging branch in `input_demo_debug_logging.cpp` is disabled, and all helper calls currently compile to no-op stubs
- The recent robot invisibility investigation logs that matter for cleanup are not using this seam. They are direct `con_printf()` blocks in at least:
  - `d2/main/render.c`
  - `d2/main/object.c`
  - `d2/main/ai.c`

### 4. Android launcher-visible debug logging is a different system
- `android/app/src/main/cpp/shared/android_log.h/.c` provides `debug_log(category, fmt, ...)`
- That path is Android-only and is intended for launcher-exportable debug files
- It is not a direct replacement for desktop replay-helper console logging
- It is still useful for Android-only diagnostics that we want exportable from the launcher UI

## Proposed Execution Plan

### Phase 1: Replay helper prompt cleanup
- Add a small numbered-choice helper inside `android/tests/run_input_demo_replay.ps1`
- The helper should:
  - display numbered options
  - treat Enter as option 1
  - retry on any other invalid input
- Convert the replay mode, render profile, and demo selection prompts to use that helper
- Reuse the same helper pattern in other interactive test/menu scripts that should obey the option-1-on-Enter rule, starting with `android/Run-TestMenu.ps1`
- Leave non-choice prompts such as `Press Enter to exit` unchanged

### Phase 2: Replay robot label toggle
- Add a replay-specific overlay enable flag instead of reusing `g_debug_tex_overlay_active`
- Preferred control surface:
  - a replay-specific command-line flag parsed beside the existing `-inputdemo-*` options
  - a non-headless helper prompt that asks whether to show robot labels
- Recommended prompt ordering for cleanup:
  - `[1] No`
  - `[2] Yes`
  - Enter chooses clean replay output by default
- Minimal engine wiring:
  - accept the new replay flag in both `d1/main/inferno.c` and `d2/main/inferno.c` so the helper surface stays symmetric
  - add a small replay overlay state variable alongside the replay overlay declarations
  - guard label accumulation in `d2/main/render.c`
  - guard label and frame-counter draw in `d2/main/gamerend.c`
- If D1 does not draw replay robot labels today, still accept and ignore the flag there so script behavior remains consistent across games

### Phase 3: Replay debug logging cleanup
- Split current replay/debug logs into three buckets before deleting or moving anything:
  - keep: durable replay diagnostics that support regression testing
  - gate: investigation-only probes that are still occasionally useful
  - remove: one-off probes that only served the invisibility investigation
- Convert the gated bucket away from direct always-on `con_printf()` blocks where practical
- Preferred extraction target is the existing shared helper under `android/app/src/main/cpp/shared/input_demo_debug_logging.h/.cpp`
- Extend that helper only for probes we intend to keep, grouped by domain such as:
  - replay_motion
  - replay_collision
  - replay_robot_lifecycle
  - replay_robot_visual
  - replay_render_visibility
- Keep the default build quiet:
  - shared helper functions should no-op unless an explicit toggle is enabled
  - remaining direct always-on investigation blocks in `d2/main/object.c`, `d2/main/render.c`, and `d2/main/ai.c` should be converted or removed

### Phase 4: File placement cleanup
- Keep cross-platform declarations in shared headers under `android/app/src/main/cpp/shared/` when both games can consume them
- Move replay-investigation helper implementations out of gameplay files where practical
- Only keep local probe-window selection code in `d2/main/*.c` when it depends directly on local engine state and would become awkward if moved
- Do not move core replay or gameplay logic into PowerShell or Kotlin

### Phase 5: Validation
- Windows host build via `run-windows-build.ps1 -Target d2 -ErrorLimit 10`
- Replay smoke checks:
  - visual replay with labels off by default
  - visual replay with labels explicitly on
  - headless replay path unchanged
- Prompt behavior checks:
  - pressing Enter at each numbered replay-helper prompt selects option 1
  - blank numbered input in `Run-TestMenu.ps1` selects option 1 where applicable
- After the actual cleanup implementation, run `android\run-code-quality.ps1 --fix`

## Proposed Implementation Order
- Add the replay-helper numbered-choice helper and Enter-default behavior
- Add the replay robot-label flag and non-headless helper prompt
- Remove or gate the robot invisibility investigation logs
- Expand the shared replay-debug helper only for the probes we intend to keep
- Build and replay-smoke the cleaned-up flow

## Open Decisions
- Resolved: the replay frame counter is controlled by the same toggle as the robot labels
- Resolved: `android/Run-TestMenu.ps1` was included in the first cleanup pass