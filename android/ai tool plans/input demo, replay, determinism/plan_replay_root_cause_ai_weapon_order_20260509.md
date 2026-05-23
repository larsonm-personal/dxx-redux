# Replay root cause ai weapon order 2026-05-09

## Goal
- identify a shared engine-side nondeterminism path behind the recent replay desync demos
- prefer a fix or source-level owner instrumentation over per-demo classification
- focus on same-frame ordering between AI rng consumers and player-weapon collision/physics work

## Steps
- [completed] inspect the frame/object processing path that orders AI, weapon sequence, collision, and rotation rng consumers
- [completed] state one falsifiable root-cause hypothesis with one cheap discriminating check
- [completed] make the smallest source change that exposes the deciding unstable order at the owner path
- [completed] rerun the narrowest replay check that can falsify the change
- [completed] add per-frame player-owned weapon slot/order diag fields to the shared state-trace payload and D1/D2 capture hooks
- [completed] write the same diag payload into recorded demo frame JSON so future expected traces carry the recorded-side weapon signature too
- [completed] validate with scoped code quality, D2 headless build, D1 build, native recorder/replay tests, and a replay state-trace rerun on level 8 `224859`

## Notes
- current demos suggest owner-order swaps, not just extra rng draws:
  - level 9 `225113`: recorded `create_awareness_event` from player-shot wall impact arrives one frame earlier than replay
  - level 8 `224859`: recorded frame 202 orders `phys_apply_rot`, `phys_apply_rot`, then obj 84 `do_ai_frame`; replay orders obj 84 `do_ai_frame` first
- checkpoint restore was confirmed to restore `d_tick_state`; a preserve-serialized-object-links change in `state.c` is a real replay-fidelity fix but it did not move either new demo's first rng owner mismatch
- replay-side movement-context instrumentation in `object_move_one()` proved level 8 frame 202 is internally consistent with the fixed object loop:
  - replay call 30267 is obj 84 `do_ai_frame`
  - replay calls 30268-30269 are `phys_apply_rot` owned by weapon obj 123 during its movement/collision path
  - replay call 30270-30271 is the player `do_laser_firing` gauss spread
- recorded rng trace at the same state/call counts still orders `phys_apply_rot`, `phys_apply_rot`, then obj 84 `do_ai_frame`
- the remaining root-cause question is earlier than the first rng owner mismatch: some hidden object-slot or object-order divergence must already exist on the recorded side before frame 202, or the recorded weapon move path must be attached to a lower-slot object than replay
- the player-weapon slot/order signature is now emitted in replay state traces and recorded demo frames from this build:
  - replay validation on level 8 `224859` still first state-mismatches at frame 227, but frames 198-230 now show weapon-slot churn directly in `diag`
  - replay frame 198 reports player weapon obj 115, frame 199-202 reports obj 123, frame 203-205 returns to obj 115, and frame 206 briefly shows two live player-owned weapons (obj 58 and obj 115)
  - older already-recorded demos do not retroactively gain recorded-side `diag`; the new fields appear on future recordings made with this build and then flow through `export_input_demo_state_trace.ps1`