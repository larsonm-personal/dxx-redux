# Test suite runtime survey 2026-06-20

## Goal
- Load `temp/test_reports/report_20260620_192211.md`.
- Survey passing tests for speedups, sensible combinations, obsolete/debug tests, and cleanup tasks.
- Avoid changing tests in this survey pass unless an obviously mechanical metadata note is needed.

## Plan
- [x] Read repo instructions and load the passing report.
- [x] Rank tests by elapsed time and setup/log footprint.
- [x] Inspect high-cost clusters and nearby scripts.
- [x] Identify combine/delete/cleanup candidates.
- [x] Record recommendations with expected impact and risk.

## Report Summary
- Source report: `temp/test_reports/report_20260620_192211.md`
- Result: 80 passed, 0 failed, 0 timeouts, 6 manual skips.
- Total wall time: 00:55:35.
- The biggest cost is not random per-test timeout padding. It is repeated emulator launch/game setup and a few intentionally long host/network loops.

## Highest Runtime Tests
- `test_axis_mapping`: 03:42
- `test_autosave_resume_missing_pilot_unified`: 03:31
- `test_autosave_resume_unified`: 03:29
- `test_input_demo_regressions_d2_graphics`: 02:49
- `test_mp`: 02:45
- `test_input_demo_determinism_matrix`: 02:04
- `test_input_demo_regressions_d2`: 01:48
- `test_all_extracts`: 01:35
- `test_input_demo_regressions_d1`: 01:30
- `test_saf_archiver`: 01:27

## Recommendations
1. Combine or collapse the two autosave resume tests.
   - `test_autosave_resume_unified` and `test_autosave_resume_missing_pilot_unified` share almost the entire first launch/autosave/resume-offer setup.
   - The missing-pilot variant is close to a superset, but it currently clears pilots before pressing `Load Last Save`. Add the ordinary direct resume assertion to that script before `clear_pilot_files`, then either delete the basic test or keep only a minimal smoke.
   - Expected saving: up to about 3:29 from the default suite, plus lower log volume.
   - Risk: moderate. Autosave has been recently churned, so validate both D1 and D2 after combining.

2. Rescope `test_axis_mapping`.
   - It runs the full multi-phase axis matrix for both D1 and D2. Most controller default behavior is Android/shared-code behavior, not game-specific behavior.
   - Keep the full static/positive/negative/virtual/trigger matrix on one game, probably D2, and keep a smaller D1 smoke that verifies D1 reaches game and sees the expected default binding table.
   - The per-axis `send_axis` settle delays are also conservative (`300ms` plus resets). If a helper can wait on the timing field changing, replace fixed sleeps with state-driven waits.
   - Expected saving: likely 1.5-2 minutes if D1 no longer runs the full matrix.
   - Risk: moderate. This test catches real mapping bugs, so reduce duplicated coverage, not assertions.

3. Make the input-demo regression matrix a single runner.
   - `run_all_tests.ps1` expands six separate entries for D1/D2/D1-in-D2, headless/graphics.
   - That is clear in the report but expensive: the regression/demo group is about 11 minutes including determinism.
   - A single `test_input_demo_regression_matrix.ps1` runner could do the same six sections with one build/tool preflight and one summary, preserving section-level failure reporting.
   - Expected saving: moderate, probably seconds to a couple of minutes depending on repeated setup in the wrappers.
   - Risk: low to moderate. Keep per-section labels in the report or failures become harder to act on.

4. Split `test_mp` into default smoke and extended soak.
   - `test_mp.ps1` includes a fixed 90 second sustained connectivity check after the lobby/game-start assertions pass.
   - The default suite could assert connect, lobby creation/join, ready, start-game, and multiplayer introspection, while a separate extended/nightly mode keeps the 90 second ping-interval soak.
   - Expected saving: about 90 seconds in default full-suite runs.
   - Risk: policy decision. If the 90 second soak is the only guard for relay ping lifecycle, keep it in a named extended test rather than deleting it.

5. Fix standalone metadata consistency for stage scripts.
   - `Get-ScriptStandalone` only honors `_info._standalone`.
   - `test_controls_bottom_stage_d2.json5` and `test_kconfig_keyboard_stage_d2.json5` use `_info.standalone`, so the runner treats them as standalone even though the files say otherwise.
   - Either change those keys to `_standalone`, or make the helper accept both names and then decide whether those stages should be owned by a wrapper or intentionally promoted to normal tests.
   - Expected saving: small directly, but this is the cleanest correctness cleanup in the survey.
   - Risk: low if the intended ownership is confirmed.

6. Move/delete debug-only graphics probes that no longer assert the bug directly.
   - `test_door45_pose_repro` says it is "ready for manual verification" and mostly preserves a reproduction pose.
   - `test_door45_cover_gpu_regression` asserts actual GPU cover fields at the same problem area.
   - The three merged-wall probe/snapshot scripts also share setup and could be a single route-matrix script.
   - Expected saving: small, around 15-45 seconds, but it reduces clutter.
   - Risk: low if the stronger GPU/snapshot assertions remain.

7. Avoid redundant setup work inside heavy infrastructure tests.
   - `test_saf_archiver.ps1` has `-NoBuild`, but it always installs the APK. In `run_all_tests.ps1`, the APK has already been built and emulator tiers are managed.
   - Add a suite path that passes `-NoBuild` and, if safe, a new `-NoInstall` or "reuse suite install" option.
   - Expected saving: modest, concentrated around the 1:27 SAF test.
   - Risk: low if isolated runs still build/install by default.

8. Add a launcher/game setup macro or batched setup command support.
   - Many json5 scripts repeat the same launch flow and multiple `setup_command` calls with `250ms` post delays.
   - A first-class `launch_new_game` action, or at least a `setup_commands` batch, would reduce duplicated fragile JSON and cut fixed wait time suite-wide.
   - Expected saving: broad but incremental. More importantly, this reduces the chance of future ping-pong around menu waits.
   - Risk: moderate because it touches shared automation behavior.
