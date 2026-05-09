# Plan: input demo replay analysis 20260507_210511

## Goal
- determine whether `android/regression_demos/d2_descent2_level6_20260507_210511.dximdemo` reproduces the robot-behind agitation-path desync or a different divergence
- find a deterministic replay-state fix for the level-6 projectile desync without adding non-input replay correction events

## Steps
- [completed] rerun the demo with preserved sandbox output and inspect the recorded/replay agitation-path probes
- [completed] identify the earliest meaningful replay divergence using the existing result, probe, and trace outputs
- [completed] add a narrow replay shield-change probe so the next rerun can distinguish known shield mutations from unknown ones
- [completed] decide whether the controlling path matches the known agitation-path gate or a different local cause
- [rejected] extend the shared replay event reader so future demos can expose recorded robot weapon spawn state to gameplay replay
- [rejected] apply recorded non-homing robot weapon spawn state in the D2 laser creation path
- [rejected] add focused shared replay coverage for the new frame-event path
- [completed] unwind the rejected weapon-create replay-event correction
- [completed] take a deeper look for the missing deterministic state or timing cause behind the frame-1044 damage
- [completed] add future checkpoint serialization for AI path allocator timing and player path cursors
- [completed] build and validate the deterministic state fix

## Notes
- the demo is a D2 level 6 save-checkpoint replay failing with broad player, position, and level-summary divergence by frame 3777
- prior D2 level 3 investigation established a specific agitation-path gate probe for behind-behavior robots; this run may or may not hit the same path
- direct headless replay shows pre-frame RNG mismatch lines before the first visible state mismatch, but `input_demo_sync_replay_rng_to_current_frame()` resets RNG at frame start, so the useful signal is that some intra-frame RNG use happened before frame 1044 rather than that replay stayed permanently off-state
- durable shield and collision probes identified the first mismatch as a direct player and weapon collision, not a blast-radius damage path
- decisive replay-only divergence is `frame=1044 gt=3727687 kind=shield_change ... cause=apply_damage ... killer_obj=26 killer_sig=27 killer_seg=42`; recorded state still shows shields 98 through frame 1045
- exact culprit is direct `player_weapon_hit` at `frame=1044 gt=3727687`: weapon object 140, signature 567, id 41, seg 24, parent robot 26/signature 27, created at `gt=3651665`
- expected and actual player position, player segment, game time, RNG state, and RNG call count match through frame 1044; position diverges only after the replay-only shield loss
- object 26 fired normal non-homing shots at `gt=3651665` and `gt=3662151` from seg 42 with ordinary `probe_robot_fire` and `probe_awareness_result` entries, but the recorded demo has no matching `player_damage` at seg 42 and no projectile lifecycle for those shots
- replay fire-vector probing showed the same frame 1015 robot state and RNG stream at a high level, but recorded `probe_robot_fire` has hidden AI path `hide=526` while replay has `hide=523`; this points at serialized checkpoint state missing transient AI path allocator or timing fields rather than a missing input
- this does not match the known agitation-path desync: object 26 never emits `probe_ai_agitation_path_gate` anywhere in this demo even though other robots do later in the same file
- recorder visibility gap is the current blocker: `input_demo_record_weapon_probe_target()` records player-owned weapons and homing weapons, so non-homing robot projectiles like object 26 weapon 41/22 are skipped in `.dximdemo`
- rejected approach: recording robot non-homing `weapon_create` as replay-consumable events and snapping matching D2 projectiles in `Laser_create_new()` violates the input-based replay design and must not be used as a fix pattern
- hard constraint for future fixes: the replay stream must remain input-based. Non-input frame events may be used for diagnostics only, not for authoritative replay correction. Future fixes must restore or serialize the missing deterministic game state so the same inputs recreate the same future projectile naturally
- unwound: the shared typed `weapon_create` replay reader, D2 staged weapon-create queue, robot projectile snap-on-create hook, and focused event parser test have been removed
- fix direction implemented for future demos: save and restore AI path runtime state in checkpoints, including `Last_tick_garbage_collected`, D2 `Last_buddy_polish_path_tick`, and the editor-only player path globals when those helpers are compiled
- diagnostic cleanup after validation: the shield probe now resets when input-demo mode changes so replay and record sessions do not inherit stale baseline shield state, while the detailed `player_weapon_hit` console line stays behind the debug gate and its durable replay probe remains available
- old demos with version 24 checkpoints cannot gain the omitted fields retroactively, so `d2_descent2_level6_20260507_210511.dximdemo` is still expected to fail at frame 1045 when replayed from its embedded checkpoint
- validation: `run-windows-build.ps1 -Target d2`, `run-windows-build.ps1 -Target d1`, and `buildd2\maths\test_input_demo_replay.exe` passed; the old level-6 replay still fails at frame 1045 as expected because its embedded checkpoint predates the new path runtime fields
- tooling note: `android\run-code-quality.ps1 -Fix` still reports the existing non-autofixable `SetupActivity.kt` max-line warnings and its path-scoping wrapper remained unreliable in this session