# D1 level 13 and 14 demo replay plan

Goal: verify the new D1 level 13 and level 14 input demos in plain D1, then run the same recordings as d1-in-d2 and fix any desyncs.

Demo files:

- `android/regression_demos/d1_descent_level13_20260617_204547.dem`
- `android/regression_demos/d1_descent_level13_20260617_204547.dximdemo`
- `android/regression_demos/d1_descent_level13_20260617_204547.dximdemo.rngtrace.jsonl`
- `android/regression_demos/d1_descent_level14_20260617_204837.dem`
- `android/regression_demos/d1_descent_level14_20260617_204837.dximdemo`
- `android/regression_demos/d1_descent_level14_20260617_204837.dximdemo.rngtrace.jsonl`

Plan:

1. [done] Inspect demo files and RNG sidecar availability.
2. [done] Run level 13 and 14 in plain D1 with result checks.
3. [in progress] If plain D1 result checks pass, run RNG/state trace comparisons where useful; if plain D1 fails, fix D1 replay/runtime first.
4. [in progress] Run level 13 and 14 as d1-in-d2 with result checks.
5. [in progress] If d1-in-d2 fails, use RNG/state traces to identify engine or save-translation splits and fix them.
6. [pending] Run scoped build, replay, and code-quality validation for touched files.

Notes:

- Level 13 RNG sidecar has 8,056 events and level 14 has 18,145 events, both untruncated.
- Keep terminal replay handling in input-demo hook files, not as menu-specific dismissal logic.
- Plain D1 level 13 still fails at frame 1431. The first useful split is in player homing missile weapon object 109: the new D1 homing diagnostics show frame 1430 updates `fvec`/velocity/track goal, but the older expected trace does not contain the added per-weapon fields. A freshly recorded level 13 demo is needed to continue that branch with the new diagnostics.
- Plain D1 level 14 passes state trace comparison through 2,667 frames. The RNG comparator now treats source-line-only differences as ignored differences, so the level 14 RNG sidecar also passes after local instrumentation shifted `laser.c` line numbers.
- D1-in-D2 level 14 has an immediate diagnostic-only frame 0 mismatch in D2-only/D1-only AI state fields, but the first user-visible `state` mismatch is frame 62: actual shields are 21 while expected shields are 22.
- The frame 60 player/robot and robot/player-weapon inputs match through the shared bump math. D1 and d1-in-d2 both apply the player weapon hit to robot 82 with identical pre/post robot velocity `(-543565,275720,1248828)`, but the D1 object watch shows robot 82 is already back to `(190681,79663,25154)` by the frame 60 `after_move` snapshot. The remaining question is which later collision or cleanup path changes D1's robot velocity before `after_move`; add a shared slot-82 bump watch before changing gameplay.
- The shared bump watch ruled out a later `bump_two_objects()` call: there is no hidden post-weapon bump involving robot 82. A per-object-loop watch shows D1 observes robot 82 with its frame-displacement velocity by the end of the frame, while d1-in-d2 keeps the weapon-bump velocity. A broad attempt to restore already-moved robot velocity after d1-in-d2 weapon hits changed earlier frame-60 state and made the replay less D1-like, so that gameplay change was backed out. Keep the diagnostics and look next for the D1-specific restore inside the moving weapon's physics step, or for a D2-only side effect that prevents that restore.
