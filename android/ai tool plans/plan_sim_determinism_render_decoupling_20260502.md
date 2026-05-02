# Simulation determinism and render decoupling plan

## Goal
Make simulation deterministic and independent from rendering and wall clock for D1 and D2 in normal play, recording, and replay so the same inputs produce the same game state in all render modes including headless

## Scope and constraints
- Minimize source churn in d1 and d2 while fixing root causes
- Keep one simulation authority path for normal run, record, replay, and headless
- Do not rely on timer_query or other wall clock checks for simulation behavior
- Keep rendering as a consumer of simulation state, never a producer of gameplay state
- Apply equivalent changes to both d1 and d2 where the logic exists in both trees

## Survey findings to drive this plan
- `wake_up_rendered_objects` in object.c has a wall-clock equality guard against `Window_rendered_data[window_num].time` using `timer_query()`
- Robot wake and awareness behavior depends on rendered object lists and `d_tick_count` partitioning logic from render-side traversal
- Headless currently uses an alternate warning path (`render_warn_robots_about_player_fire` -> `update_all_robot_location_info_with_view`) instead of the full rendered-data wake path
- This creates a mode-dependent simulation path and explains one-frame awareness timing drift
- There are additional wall-clock seeded behavior sites in gameplay-adjacent code (`d_srand((fix)timer_query())` and other timer_query gates) that should be explicitly triaged

## Completed work (20260502 session)

Two root causes for windowed vs no-render state trace divergence were identified and fixed:

### Root cause 1: render modifies object free list (object.c/game.c)
- `game_render_frame()` creates non-physics objects (fireballs, explosions) in freed object slots
- This changes `free_obj_list[]` without changing `Highest_object_index`, so the existing probe did not detect it
- The consequence: in WIN mode, robot weapons get different slot assignments, leading to different trajectories
- Fix: comprehensive save/restore block around `game_render_frame()` in game.c EVENT_WINDOW_DRAW handler:
  - Snapshot ALL objects (not just MT_PHYSICS): type, movement_type, segnum, flags, physics fields
  - Snapshot `object_runtime_state` (free_obj_list, num_objects, Highest_object_index, homer state)
  - After render: restore runtime state, clean up render-created objects, restore render-destroyed objects
  - Restore physics fields and obj_flags for all surviving physics objects
  - Log any render-created/destroyed/mutated objects for future debugging

### Root cause 2: render modifies robot danger_laser_num via set_robot_location_info (object.c)
- `render_object()` calls `set_robot_location_info()` for each rendered robot
- When the player fires a laser and a robot is near center-screen, `danger_laser_num/sig` is set on the robot
- This AI state change persists after render and affects the robot's dodge/fire behavior in the next physics frame
- In NOR/headless mode this never happens (no render traversal)
- `update_all_robot_location_info_with_view()` already has `if (!input_demo_replay_is_loaded())` guard; `set_robot_location_info()` did not
- Fix: add early return in `set_robot_location_info()` when replay is loaded, matching the existing guard

### Verification
- All 4 variants (windowed-default, windowed-lowres, windowed-norender, headless-console) now PASS
- Tested with d2_descent2_level2_20260501_141150.dximdemo fixture
- Previous divergence was at frame=547 stage=motion, position delta=3 (caused by force delta from robot weapon hit)

## Phase plan

### Phase 0 - Baseline and observability lockdown
- [ ] Add and pin reproducibility fixtures for D1 and D2 using existing input-demo assets
- [x] Extend state trace comparison to emit first divergence stage labels for awareness and robot wake transitions
- [x] Add a deterministic mode run matrix: windowed render, low render, and no-render/headless on same host build
- [x] Record baseline parity/failure table for each fixture and mode

Deliverables
- Updated deterministic fixture list in android tests
- Baseline report attached to this plan with first-drift frame and subsystem label per fixture

### Phase 1 - Introduce simulation tick context and wall-clock boundary
- [ ] Add a small simulation tick context struct in engine code that carries frame index, FrameTime, and deterministic tick counters
- [ ] Centralize simulation time reads behind helpers for gameplay code (not render code)
- [ ] Classify timer_query callsites into buckets: simulation-critical, ui-only, audio-only, render-only, tooling-only
- [ ] Add a deterministic-build assert path that flags simulation-critical use of timer_query

Deliverables
- New helper API and callsite inventory for d1 and d2
- Deterministic assert logs clean for replay path except explicitly allowlisted transitional sites

### Phase 2 - Remove render-produced simulation side effects
- [ ] Split robot wake candidate generation from rendering and move it to a simulation-visible-set builder that is called from simulation phase
- [ ] Keep render using the same visible-set output but prohibit it from mutating AI awareness state
- [ ] Replace `wake_up_rendered_objects` timer equality behavior with deterministic frame-phase validation instead of wall-clock validation
- [ ] Keep legacy behavior parity by preserving object selection rules during migration (including `d_tick_count` partition semantics) before any intended gameplay change

Deliverables
- One shared robot wake path used by normal, replay, and headless
- No simulation state writes in render-only frame functions

### Phase 3 - Unify replay and normal frame pipeline
- [ ] Make replay frame stepping execute the same simulation phase ordering as normal gameplay frame
- [ ] Remove special-case headless warning path once shared wake path is live
- [ ] Ensure record and replay both pass through identical awareness enqueue and consume phase boundaries
- [ ] Add invariants that compare critical counters across modes (awareness event count, robot wake count, AI state transition count)

Deliverables
- Single simulation pipeline for normal, record, replay, headless
- Removal or hard deprecation of replay/headless-only simulation branches

### Phase 4 - Eliminate wall-clock seeded randomness in simulation
- [ ] Replace simulation-relevant `d_srand((fix)timer_query())` and similar seeds with deterministic seeds derived from replay/session state
- [ ] Keep non-simulation presentation randomness on FX stream where appropriate
- [ ] Document RNG stream policy: SIM stream only for gameplay decisions, FX stream never feeds gameplay state
- [ ] Add tests to detect SIM stream drift and frame-phase drift together

Deliverables
- Deterministic seed policy for simulation paths in d1 and d2
- Updated RNG trace checks in android tests

### Phase 5 - Determinism regression gates and rollout
- [ ] Add CI-style replay parity task that runs the same fixture set in windowed and no-render/headless mode and enforces identical state traces
- [ ] Add at least one integration fixture each for robot awareness, collision chain, and homing/weapon behavior
- [ ] Run windows host verification build plus deterministic replay suite before merge
- [ ] Run full android code quality pass and targeted replay tests on emulator after each tranche

Deliverables
- Replay parity gate preventing render-mode-induced simulation drift
- Stable fixture suite for future determinism work

## Implementation order recommendation
1. Phase 0 and Phase 1 first in one tranche to lock visibility and stop new timer leaks
2. Phase 2 and Phase 3 second to remove render coupling and unify pipelines
3. Phase 4 third to finish RNG and seeding determinism
4. Phase 5 continuously, with hard gate enabled after Phase 3

## Risks and mitigations
- Risk: hidden gameplay dependence on render traversal ordering
  - Mitigation: preserve legacy selection rules initially and compare per-frame candidate/object lists before behavior-changing cleanup
- Risk: large d1 and d2 duplicated edit surface
  - Mitigation: land small mirrored patches and add shared helper code only for newly introduced logic
- Risk: false confidence from pass/fail only at final result
  - Mitigation: first-divergence tracing and per-phase counters, not just final replay result

## Exit criteria
- Same replay fixture yields identical per-frame state traces in windowed and headless/no-render modes on same build
- No simulation-critical use of wall clock APIs in deterministic mode
- Normal play, recording, and replay all use the same simulation phase functions and ordering
- Headless no longer requires separate simulation logic for robot warning or awareness behavior

## Progress
- [x] Survey current render-coupled and wall-clock-coupled hotspots for D2
- [x] Scan D1 for equivalent timer and determinism-sensitive usage
- [x] Draft phased plan for unified deterministic simulation path
- [ ] Execute Phase 0
- [x] Implement replay frame diagnostics (`awareness_events`, `camera_awake_robots`, `danger_laser_robots`, `d_tick_count`) in D1 and D2 state traces
- [x] Add mismatch stage labels in state trace compare using per-frame diagnostic transitions
- [x] Add deterministic matrix runner (`android/tests/test_input_demo_determinism_matrix.ps1`) and pinned fixture list (`android/tests/input_demo_determinism_fixtures.txt`)
- [x] Run matrix baseline for current committed D2 fixture and capture artifacts under `temp/input_demo_determinism_matrix/20260502_131803`
- [x] Improve replay wrapper and matrix parsing so no-render runs in state-trace-only fallback mode and reports `missing_trace` instead of a hard wrapper abort
- [x] Replace D2 `wake_up_rendered_objects` wall-clock gate with deterministic simulation-frame stamp (`game_get_simulation_frame_id` + `Window_rendered_data.simulation_frame_id`)
- [x] Make host replay build guardrail auto-retry with supported `run-windows-build.ps1` vcvars arch when default arch is unsupported
- [x] Decouple no-render replay stepping from draw events by driving `input_demo_step_replay_frame()` directly from SDL event processing in no-render replay mode (D1/D2)
- [x] Ensure no-render replay still creates `Game_wind` so the replay/game frame pipeline and state trace writes execute (D2)
- [x] Add matrix no-oracle mode (`-NoOracle`) to compare same-build variant traces against a baseline variant instead of stale embedded demo expectations

## Baseline snapshot 20260502_140023
- Fixture set: 1 committed D2 fixture (`android/regression_demos/d2_descent2_level2_20260501_141150.dximdemo`)
- Windowed default: FAIL (`motion`) first mismatch `frame=547 stage=motion state.position.x: expected -14668450, actual -14668449`
- Windowed lowres: FAIL (`motion`) first mismatch `frame=547 stage=motion state.position.x: expected -14668450, actual -14668449`
- Windowed no-render: PASS
- Headless console: PASS
- CSV artifact: `temp/input_demo_determinism_matrix/20260502_140023/matrix_results.csv`

Open follow-ups
- Add at least one pinned D1 fixture so Phase 0 can cover both game trees
- Resolve host build guardrail/toolchain mismatch (`run-windows-build.ps1` default x86 preset vs vcvars supporting arm only) so matrix reruns exercise new native code paths
- Investigate first deterministic drift at frame 547 (`stage=motion`) now that no-render and headless variants are both producing usable deterministic runs

## No-oracle parity snapshot 20260502_140707
- Mode: `android/tests/test_input_demo_determinism_matrix.ps1 -NoOracle -IncludeHeadless`
- Baseline variant: `windowed-norender` state trace
- `windowed-norender`: PASS
- `headless-console`: PASS (trace parity with no-render)
- `windowed-default`: FAIL at `frame=547 stage=motion` (`x: expected -14668452, actual -14668449`)
- `windowed-lowres`: FAIL at `frame=547 stage=motion` (`x: expected -14668452, actual -14668449`)
- RNG parity check (`windowed-default` vs `windowed-norender`) passes, so current divergence is non-RNG state mutation likely from render-coupled or wall-clock-coupled gameplay path

## No-oracle parity snapshot 20260502_143044
- Mode: `android/tests/test_input_demo_determinism_matrix.ps1 -NoOracle -IncludeHeadless`
- Baseline variant: `windowed-norender` state trace
- `windowed-norender`: PASS
- `headless-console`: PASS
- `windowed-default`: FAIL at `frame=547 stage=motion` (`x: expected -14668452, actual -14668449`)
- `windowed-lowres`: FAIL at `frame=547 stage=motion` (`x: expected -14668452, actual -14668449`)
- Added state-trace motion diagnostics (`diag.player_vel_*`, `diag.player_last_*`) show first upstream divergence at `frame=546` in player velocity while position is still identical
- Motion diag sample at frame 546:
  - no-render vel `(1323897,-261676,245567)`
  - windowed vel `(1323989,-261493,245692)`
- This confirms the first divergence enters before frame-547 position serialization and should be treated as a between-frame render-to-sim mutation path

## Current replay-side guards landed in this tranche
- Replay-only guard in homing selection to avoid render-window/timer dependency in `d2/main/laser.c`
- Replay-only early return in `wake_up_rendered_objects` in `d2/main/object.c`
- Replay frame FP reset helper in `d2/main/game.c` (`_controlfp_s` + `_mm_setcsr(0x1f80u)`)
- Replay control-event gating in `game_handler` so replay ignores live input events
- Replay draw wrapper preserves/restores player `velocity` and `last_pos` around `game_render_frame`
- No-render replay no longer calls `render_warn_robots_about_player_fire`
