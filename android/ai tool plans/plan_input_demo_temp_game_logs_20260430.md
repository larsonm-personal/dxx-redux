# Input Demo Temp Game Logs Analysis 2026-04-30

## Goal

Analyze the new hand-recorded D2 level 2 demo in `android/temp_game_logs` with
the added durable frame-event logging, identify the first meaningful replay
divergence on host, and decide whether current logging is sufficient or what
additional cross-demo logging would unstick the investigation.

## Working Hypothesis

The host replay logging was masking the real ordering. The earliest stable
divergence is now frame 817, where host state is already off by one
`powerups_remaining` with a tiny position delta while energy still matches.
The later frame-818 energy mismatch is therefore secondary to an earlier object
or pickup timing divergence, not the first break.

The current best candidate is a robot-dropped powerup lifecycle mismatch. D2
robot drops can consume RNG when deciding whether a robot drops anything, how
many contained items it drops, whether redundant weapons are replaced with
energy or shields, whether high-energy or high-shield pickups are suppressed,
the spawned powerup velocity, and energy/shield lifetime. A small difference in
that path could make a spawned energy powerup drift or expire differently, and a
later pickup would explain the frame-818 energy mismatch without being the root
cause.

## Powerup/RNG Survey

- `fireball.c` handles the robot death drop path. If a destroyed robot has no
  explicit `contains_count`, the robot-info fallback tests `contains_prob` with
  RNG, chooses a random count, copies `contains_type` and `contains_id`, then
  calls replacement and egg creation.
- `maybe_replace_powerup_with_energy()` can randomly replace already-owned or
  nearby weapons with energy or shields. It can also suppress boss-gated energy.
- `object_create_egg()` can randomly suppress energy or shield drops when the
  player already has high energy or shields.
- `drop_powerup()` spends RNG on spawned powerup velocity and, for energy and
  shields, lifetime. The spawned object uses `OBJ_POWERUP`, `CT_POWERUP`,
  `MT_PHYSICS`, and `RT_POWERUP`.
- Existing recording already writes a `.rngtrace.jsonl` sidecar with frame,
  game time, call count, RNG state before and after, result, file, function,
  and line for annotated `d_rand()` calls. Replay restores per-frame RNG state,
  so the missing piece is semantic event logging that ties those RNG calls to
  source objects and spawned powerups on both record and replay.

## Next Diagnostic Plan

1. Add durable semantic powerup events to the input demo stream, not just debug
   printlines. Emit them during recording and replay so a single diff can show
   whether record and replay chose the same robot contents, created the same
   powerup, and moved it similarly.
2. Start with low-volume event types: `powerup_drop_decision`, `powerup_spawn`,
   `powerup_pickup`, and `powerup_remove`. Include frame, game time, RNG state
   and call count, source object index/signature/type/id, segment, position,
   velocity, `contains_*` before and after replacement, reason, created object
   index/signature/id, created position/velocity/lifeleft, and player energy or
   shields when relevant.
3. Add a compact per-frame powerup summary to the recorded frame state rather
   than logging every object. Track counts by powerup id, total live powerups,
   total `OF_SHOULD_BE_DEAD` powerups, and nearest energy/shield powerups to the
   player by signature, segment, position, velocity, and distance. This makes
   hand-recorded future demos useful even when the exact robot/frame differs.
4. Keep the existing raw RNG sidecar, but add a small comparison script that can
   join semantic events to nearby `.rngtrace.jsonl` entries by frame and call
   count. If record and replay diverge in an event before global state does, the
   event tells us which RNG call or object lifecycle step matters.
5. Once the semantic events reproduce the issue, remove the temporary bounded
   probes in D2 and replace any generally useful pieces with shared D1/D2 or
   android-shared helpers where practical.

## Cheap Discriminating Check

Run the direct non-headless host replay with bounded frame probes, verify the
first mismatching frame and properties, then step one hop closer to the code
that can remove a powerup or perturb player state before frame 817.

## Steps

1. [completed] Run the non-headless host replay and fix the host-side logging so
   the tail window reports expected-vs-actual state directly instead of hiding
   behind an earlier mismatch latch
2. [completed] Compare the mismatch window against the demo `state` to identify
   the earliest divergent property: frame 817 starts with `powerups_remaining`
   low by one and a tiny position delta while energy still matches
3. [completed] Survey robot-drop and powerup physics code for RNG-dependent
   decisions that could explain a hand-recorded energy pickup mismatch
4. [completed] Add live replay state-trace logging plus wrapper compare support
   so future temp-game-log runs can locate the first visible mismatch without
   relying on temporary bounded replay probes
5. [not_started] Add durable semantic powerup events for drop decisions, spawned
   powerups, pickups, and removals
6. [not_started] Add compact per-frame nearby powerup summaries to make future
   hand-recorded demos diagnosable by condition type instead of exact frame
7. [not_started] Correlate the frame-816/frame-817 object-state divergence with
   the code paths that can remove or expire a powerup without going through the
   normal player-powerup collision callback
8. [not_started] Keep the probes narrow and extend them only one frame earlier
   at a time until the exact removal path is identified
