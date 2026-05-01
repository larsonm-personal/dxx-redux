# Input Demo Replay State Compare Alignment Plan

## Goal

Align replay-side per-frame state comparison with the same frame phase used by
recording so `game_time64` does not produce a false mismatch before real
gameplay divergence.

## Phases

1. [completed] Confirm the local replay and record call ordering around
   `calc_game_time()` and the per-frame state capture/compare hooks
2. [completed] Move the replay-side state comparison to the matching frame
   phase in both D1 and D2 with the smallest possible code change
3. [completed] Re-run the same non-headless host replay and inspect the first
   reported mismatch window

## Notes

- Keep the change local to replay comparison timing
- Do not change the recorded `.dximdemo` schema or captured state fields in
  this tranche
- Validation outcome: frame-0 `game_time64` false mismatches are gone, and the
   first real tracked-state mismatch now appears at frame 400 on `player0.score`
   because host kills robot 68 one frame later than the Android recording