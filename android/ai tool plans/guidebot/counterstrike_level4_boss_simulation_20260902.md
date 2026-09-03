# Counterstrike level 4 boss simulation investigation

## Plan

- [x] Reproduce the checked `timeout` with focused engine logs
- [x] Trace boss completion, replanning, and exit-objective selection
- [x] Determine why the headed run appears to stop after boss destruction
- [x] Implement the smallest general boss-to-exit correction
- [x] Verify Counterstrike level 4 deterministically reaches the exit
- [x] Check another boss level and a non-boss baseline for regressions

## Initial evidence

- Checked simulation output completes the boss at 65 seconds and terminates at
  frame 3932 with `semantic objective repeated without route advancement`.
- Checked mission metadata contains an exit step immediately after the boss.
- The recorded state is a semantic-repeat timeout, not the global simulation
  timeout.

## Findings and correction

- The simulation applied lethal boss damage at frame 3899, then immediately
  rescanned the live route. The engine correctly continued to report the boss
  as the primary objective while its death roll was active.
- That produced 33 identical boss selections and exhausted the semantic-repeat
  guard at frame 3932. This explains why the headed window closed immediately
  after the boss appeared to die.
- Boss completion now waits for the engine's authoritative
  `Control_center_destroyed` transition, which occurs when the death sequence
  finishes and the exit is opened. Only then does route confirmation record the
  boss and request the next live objective.

## Verification

- Counterstrike level 4, two identical seeded runs: `confirmed`, boss at frame
  4260 (71 seconds), exit at frame 4634 (77 seconds).
- Counterstrike level 8 boss control: the engine run reaches the boss at frame
  2431 and the exit at frame 3295. The aggregate remains `route_mismatch`
  because its live route omits precomputed switch 27, but it no longer stops at
  the boss. That is a separate route-sequence discrepancy.
- Counterstrike level 1 non-boss control: unchanged `confirmed` result at frame
  1960 with the same ending RNG state and call count.
- D2 Windows release build completed and all 45 CTest tests passed.
