# Plan: Headless Runner UX And Replay Logging Trim (2026-05-03)

## Goal
- Make explicit `-Runner headless-console` runs skip irrelevant prompt paths and fall straight into the supported accelerated/default flow when the user left those choices at their defaults
- Classify remaining replay logging into `keep`, `keep-but-gate`, and `discard` buckets before another cleanup pass

## Local Hypothesis
- `android/tests/run_input_demo_replay.ps1` currently calls `Get-RenderProfile` and `Get-LaunchMode` before `Resolve-ReplayRunnerSelection`, so explicit headless runs still hit prompt-only code even though headless ignores render profile and only supports accelerated checkpoint replay
- The remaining replay log noise is now concentrated in a small set of direct `con_printf()` families plus already-gated shared helper probes, so a file-level classification pass is enough to scope the next cleanup tranche

## Logging Buckets

### Keep
- `d2/main/game.c` and `d1/main/game.c`
  - replay stop reasons
  - replay result write and compare outcome lines
- `d2/main/input_demo_start.c` and `d1/main/inferno.c`
  - replay player-config summary at replay start
- `d2/main/newdemo.c` and `d1/main/newdemo.c`
  - recording start, stop, save, and failure lines
- recorder event append failures in gameplay files such as `d2/main/laser.c`, `d2/main/collide.c`, `d2/main/gauges.c`, and `d2/main/input_demo_energy_trace.h`

### Keep But Gate
- `android/app/src/main/cpp/shared/input_demo_debug_logging.h/.cpp`
  - shared replay debug gate stays the single switch for investigation probes
- direct RNG probe families in `d2/main/gamecntl.c` and `d2/main/laser.c`
  - `RNG_PROBE|weapon_item_*`
  - `RNG_PROBE|missile_loop_*`
  - `RNG_PROBE|missile_*`
  - keep because they are still useful for desync call-count work, but only behind `-inputdemo-debug-log`
- existing replay investigation families already routed through the shared helper in D2 gameplay files
  - keep available for desync work, but only behind `-inputdemo-debug-log`
  - `d2/main/ai.c`: robot pose track
  - `d2/main/object.c`: robot lifecycle, AI rng
  - `d2/main/render.c`: render target state, render boundary gate, render robot skip
  - `d2/main/physics.c`: drag, motion detail, physics probe
  - `d2/main/controls.c`: control probe, wiggle probe
  - `d2/main/escort.c`: escort state, goal, restore, rng progress
  - `d2/main/aipath.c`: path state, request, detail, points, follow probe

### Discard
- one-off render and object-lifecycle probe families that were added only to chase the replay robot visibility investigation once they are no longer needed for current desync work
- duplicate confirmation lines whose only purpose is proving local control flow when a richer gated probe already reports the same event with frame, object, or RNG state context
  - likely first discard candidates once the current replay-visibility investigation is closed:
    - `d2/main/render.c`: `render_mine_enter_unconditional`, `render_mine_enter`, `probe_heartbeat`
    - `d2/main/object.c`: `should_die_set`, `delete_sweep`, `obj_relink`, `update_seg_fail`
    - `d2/main/render.c`: per-object `render target state` snapshots when the narrower boundary or skip probes already identify the same failure

## Execution Plan
- Phase 1
  - normalize explicit headless runner defaults before prompt helpers run
  - validate that supported headless runs no longer prompt when the user leaves mode/profile at defaults
- Phase 2
  - validate that explicit unsupported headless mode combinations fail immediately without first prompting for unrelated options
- Phase 3
  - audit the remaining direct replay logs file by file
  - convert any remaining replay-only noisy lines from `replay loaded` gating to shared debug gating where appropriate
  - remove obsolete one-off probes after the current replay investigations no longer depend on them

## Status (2026-05-03)
- Phase 1 completed
  - `android/tests/run_input_demo_replay.ps1` now forces explicit `-Runner headless-console` runs with defaulted prompt values onto accelerated mode plus default render profile before the prompt helpers run
  - validated with `android/tests/run_input_demo_replay.ps1 -DemoPath android/regression_demos/d2_descent2_level2_20260501_141150.dximdemo -Game d2 -Runner headless-console`, which completed with `RESULT: PASS` and no prompt lines
- Phase 2 completed
  - validated with `android/tests/run_input_demo_replay.ps1 -DemoPath android/regression_demos/d2_descent2_level2_20260501_141150.dximdemo -Game d2 -Runner headless-console -Mode realtime`, which failed immediately without prompt lines
- Phase 3 not started
- Phase 3 in progress
  - completed the remaining direct `RNG_PROBE|missile_loop_*` trim in `d2/main/gamecntl.c` by switching the last replay-loaded-only guard to the shared debug gate
  - validated with a normal visual replay helper run using `Replay debug log: off`, which still passed and produced no `RNG_PROBE` lines in the captured helper output
  - survey result: the remaining large replay-investigation families in `ai.c`, `object.c`, `render.c`, `physics.c`, `escort.c`, `aipath.c`, and `controls.c` are already gated by `input_demo_debug_is_enabled()`
  - fixed the Android native build break in `d2/main/render.c` by declaring `input_demo_render_probe_list_has_object()` before first use and removing the shadowed `target_obj`, then cleared the leftover unused probe helper in `d2/main/object.c`
  - validated the Android-side build fix with `android\gradlew.bat ":app:buildCMakeDebug[arm64-v8a]"` under `JAVA_HOME=C:\local\jdk-21`, which now completes with `BUILD SUCCESSFUL`
  - removed the redundant render scaffolding probes `probe_heartbeat`, `render_mine_enter_unconditional`, and `render_mine_enter` from `d2/main/render.c`
  - removed the redundant robot lifecycle probes `delete_sweep` and `should_die_set` from `d2/main/object.c`, keeping the later `obj_delete` lifecycle line as the remaining deletion marker
  - validated the `object.c` trim with `android/tests/run_input_demo_replay.ps1 -DemoPath android/regression_demos/d2_descent2_level2_20260501_141150.dximdemo -Game d2 -Runner headless-console -ReplayDebugLog -HeadlessConsoleOutput 2`, which auto-rebuilt `d2` and completed with `RESULT: PASS`
  - removed the remaining redundant robot lifecycle probes `obj_relink` and `update_seg_fail` from `d2/main/object.c`, then cleaned up the dead locals left behind so the Android `arm64-v8a` task stays warning-free for this slice
  - removed the standalone target-visibility snapshot probe family from `d2/main/render.c`, keeping the narrower boundary-gate and skipped-robot probes in place
  - validated both follow-up trims with `android/tests/run_input_demo_replay.ps1 -DemoPath android/regression_demos/d2_descent2_level2_20260501_141150.dximdemo -Game d2 -Runner headless-console -ReplayDebugLog -HeadlessConsoleOutput 2`, which auto-rebuilt `d2` and completed with `RESULT: PASS`
  - kept the optional `Input demo render robot skip` logging in `d2/main/render.c`, but reduced it to first-occurrence or changed-state events so the boundary-gate and skip families remain available without emitting the same line every frame
  - kept the optional `Input demo player control probe` and `Input demo player wiggle probe` in `d2/main/controls.c`, but reduced the control probe to control-state changes and the wiggle probe to actual wiggle events
  - validated the render and controls trims with `android/tests/run_input_demo_replay.ps1 -DemoPath android/regression_demos/d2_descent2_level2_20260501_141150.dximdemo -Game d2 -Runner headless-console -ReplayDebugLog -HeadlessConsoleOutput 2`, which auto-rebuilt `d2` and completed with `RESULT: PASS`
  - revalidated the Android native build after both trims with `android\gradlew.bat ":app:buildCMakeDebug[arm64-v8a]"` under `JAVA_HOME=C:\local\jdk-21`, which stayed at `BUILD SUCCESSFUL`
  - kept the optional player-motion tracing in `d2/main/physics.c`, but reduced the broad `Input demo player drag probe` and `Input demo player motion detail` lines to changed-state events instead of repeating identical threat-window frames
  - left the event-specific `Input demo physics probe`, `Input demo physics object contact`, and `fix_illegal_wall_intersection` logs intact because they are already sparse and generally useful when debug logging is enabled
  - validated the `physics.c` trim with `android/tests/run_input_demo_replay.ps1 -DemoPath android/regression_demos/d2_descent2_level2_20260501_141150.dximdemo -Game d2 -Runner headless-console -ReplayDebugLog -HeadlessConsoleOutput 2`, which auto-rebuilt `d2` and completed with `RESULT: PASS`
  - revalidated the Android native build after the `physics.c` trim with `android\gradlew.bat ":app:buildCMakeDebug[arm64-v8a]"` under `JAVA_HOME=C:\local\jdk-21`, which stayed at `BUILD SUCCESSFUL`
  - kept the optional escort diagnostics in `d2/main/escort.c`, but reduced the broad `Input demo replay escort state` summary to changed-state events so the richer escort snapshot still appears when mode, path, goal, or visit gates actually change
  - kept the optional `Input demo replay snipe detail` and `Input demo replay thief detail` messages in `d2/main/escort.c`, but reduced their entry and exit traces to behavior-state changes using coarse action-time buckets instead of logging every frame while the same mode just counts down
  - left the escort path, goal, restore, segment-change, and visit-change probes intact because they are already event-style or path-creation diagnostics and remain useful when replay debug logging is enabled
  - validated the `escort.c` trims with `android/tests/run_input_demo_replay.ps1 -DemoPath android/regression_demos/d2_descent2_level2_20260501_141150.dximdemo -Game d2 -Runner headless-console -ReplayDebugLog -HeadlessConsoleOutput 2`, which auto-rebuilt `d2` and completed with `RESULT: PASS`
  - revalidated the Android native build after the `escort.c` trims with `android\gradlew.bat ":app:buildCMakeDebug[arm64-v8a]"` under `JAVA_HOME=C:\local\jdk-21`, which stayed at `BUILD SUCCESSFUL`
  - removed the one-off `Input demo awareness focus` probe path from `d2/main/ai.c`, including the hardcoded frame window and the extra stdout or stderr mirroring, while keeping the reusable optional awareness entry, post-add, post-gate, and result logs behind the shared debug gate
  - extracted the shared replay-debug activity context helpers into `android/app/src/main/cpp/shared/input_demo_debug_logging.h/.cpp`, so the common `record` or `replay` mode name and frame-index logic now lives under `android/` and the D2 files use thin local wrappers instead of open-coded copies
  - validated the host cleanup slice with `cmake --build buildd2 --target dxx-redux-d2-headless` and the shared cross-check `cmake --build buildd1 --target dxx-redux-d1`, both passing after the helper extraction repair
  - kept the optional `Input demo robot pose track` family in `d2/main/ai.c`, but removed the per-frame `step=pose` snapshots and changed the `step=summary` line to emit only when the tracked robot set actually changes, leaving discover, view-gate, and missing transitions intact for later desync work
  - removed the steady-state `Input demo replay follow probe` line and its paired per-frame path-state snapshot from `d2/main/aipath.c`, leaving the event-style `follow advance trigger`, `follow wrap`, `follow advance result`, and the path creation diagnostics intact
  - validated the AI and path trims with `cmake --build buildd2 --target dxx-redux-d2-headless`, plus `android/tests/run_input_demo_replay.ps1 -DemoPath android/regression_demos/d2_descent2_level2_20260501_141150.dximdemo -Game d2 -Runner headless-console -ReplayDebugLog -HeadlessConsoleOutput 2` and the same helper run without `-ReplayDebugLog`, both ending at `RESULT: PASS`
  - validation note: direct stdout redirection from host helper runs is not a trustworthy proof surface in this VS Code PowerShell session because redirected files can truncate mid-run; rely on helper PASS plus source inspection instead of redirected temp files when checking these host-only probe removals

## Validation
- `android/tests/run_input_demo_replay.ps1 -Runner headless-console` on a supported D2 checkpoint replay should run without render-profile or replay-mode prompts when mode/profile were left at defaults
- `android/tests/run_input_demo_replay.ps1 -Runner headless-console -Mode realtime` should fail immediately without prompting for render profile
- After any logging trim pass, rerun a quiet default visual replay and an explicit debug-log-on replay to confirm the default stays quiet and the probes still come back on demand