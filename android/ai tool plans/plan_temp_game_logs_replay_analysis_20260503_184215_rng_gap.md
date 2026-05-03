# Plan: temp_game_logs 184215 RNG gap isolation 2026-05-03

## Goal

Pinpoint the exact call site that advances SIM RNG call_count by one between the frame-188 spreadfire sequence and frame-189 awareness gate in:

- android/temp_game_logs/d2_descent2_level2_20260502_184215.dximdemo

## Steps

- [completed] Reproduce host mismatch and confirm first compare divergence near frame 188/189
- [completed] Add condition-based awareness probes in ai.c and verify call_count transitions
- [completed] Add player-shot fire probes in laser.c and confirm frame-189 first shot starts at sim_calls=80422
- [completed] Add caller-chain probes around missile firing path and rerun headless replay
- [in-progress] Identify exact pre-awareness caller that consumes call 80422 and implement deterministic fix
- [not started] Rebuild and rerun replay/compare validation to confirm parity

## Notes

- Existing logs show 80422 is consumed before frame-189 awareness post-gate and is absent from the frame-188 trace events
- Current tranche uses condition-based probe gating, not frame-number hardcoding
- New probes show frame-189 missile processing starts with sim_calls=80422 before any missile-fire body code
- In input demo replay, game.c input_demo_prepare_replay_frame logs mismatch then restores RNG with d_rand_set_state(replay_frame.rng_state) and d_rand_set_call_count(replay_frame.rng_call_count), which explains the apparent 80421 -> 80423 trace jump presentation
