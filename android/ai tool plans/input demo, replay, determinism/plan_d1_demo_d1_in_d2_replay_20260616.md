# D1 demo replay through D1 and D1-in-D2

Goal: finish enough D1 input-demo determinism for the supplied level 4 demo to be a useful control artifact, then replay it under D1-in-D2 to expose gameplay-semantic desyncs.

Artifacts:

- `android/regression_demos/d1_descent_level4_20260616_090645.dem`
- `android/regression_demos/d1_descent_level4_20260616_090645.dximdemo`
- `android/regression_demos/d1_descent_level4_20260616_090645.dximdemo.rngtrace.jsonl`

Plan:

1. [done] Inspect the demo metadata and host replay tooling.
2. [done] Reproduce the D1 replay failure under the host D1 engine.
3. [done] Fix the first checkpoint determinism bug: the D1 save checkpoint restored `Point_segs` but not the `Point_segs_free_ptr` allocator cursor.
4. [done] Add D1 save-state version 15, save/restore the AI path allocator cursor, and rebuild it from active paths for older checkpoints.
5. [done] Re-run D1 host replay and confirm the first real mismatch moves from frame 19 to frame 1120.
6. [done] Investigate the frame 1120 D1 host replay mismatch before moving to D1-in-D2.
7. [done] Add narrow, durable diagnostics for the hidden robot/object state that can diverge before `robot_state_hash` or `live_object_hash`.
8. [in progress] Use the earliest new diagnostic mismatch to fix the next D1 engine non-determinism, preferring save-state/runtime-state fidelity over replay special cases.
9. [pending] Once the supplied D1 demo is stable enough under D1, run it under D1-in-D2 and start the gameplay-semantics fix loop there.
10. [pending] Keep commands, outputs, and remaining mismatch targets in this plan.

Notes:

- The `.dximdemo` is the primary determinism artifact.
- The `.rngtrace.jsonl` is the recorded-side RNG stream for mechanical comparison.
- The `.dem` is a legacy supporting sidecar and is not precise enough for frame-level desync localization.
- Current D1 host replay status after the AI path allocator fix: frame 912/913 has a transient `danger_laser_robots` diagnostic-only mismatch while hashes and RNG remain aligned; the first real mismatch is frame 1120 in `diag.robot_state_hash` and `diag.live_object_hash`.
- The frame 1120 mismatch appears to involve nearby moving robots in object slots 73, 74, 87, and 88. RNG state and call count remain aligned through the first real mismatch, which points toward hidden simulation state or deterministic collision/path behavior rather than random branching.
- Temporary frame-1120 probes showed the replay robot pair 73/74 physically overlaps at frame 1120, causing object-object collision retries and an early `create_random_xlate` call for robot 73 at frame 1123 instead of the recorded frame 1124. FVI was not near a threshold; one object started inside the other's radius.
- The old embedded state trace does not include robot `ctype.ai_info` or `Ai_local_info`, so the hidden pre-1120 difference cannot be compared directly from this recording. Added durable `robot_ai_static_state_hash` and `robot_ai_local_state_hash` diagnostic fields for new recordings/replays; old traces tolerate these as actual-only fields.
- Fixed a D1 checkpoint restore bug where weapon `creation_time` was saved relative to `GameTime64` but restored as an absolute timestamp. D2 already restores this as `GameTime64 + delta`; D1 now matches that behavior.
- Validation after these changes:
  - `.\android\run-code-quality.ps1 -Fix -Paths ...` passed for the touched D1/D2/shared trace files and this plan.
  - `.\run-windows-build.ps1 -Target d1` passed.
  - `buildd1\maths\test_input_demo_recorder.exe` passed.
  - `buildd1\maths\test_input_demo_replay.exe` passed.
  - D1 replay still fails at the same old expected-trace frame 1120, which is expected until a new trace includes the added AI diagnostics or another engine-state bug is found from audit. Latest trace output is under `temp\input_demo_state_traces\d1_level4_final_ai_diag_creation_time`.
  - `.\run-windows-build.ps1 -Target d2` did not reach compilation because CMake configure could not find `SDL_mixer` (`SDL_MIXER_LIBRARIES`/`SDL_MIXER_INCLUDE_DIRS`).
