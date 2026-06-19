# D1 level 13 and 14 demo replay plan

Goal: verify the new D1 level 13 and level 14 input demos in plain D1, then run the same recordings as d1-in-d2 and fix any desyncs.

Demo files:

- `android/regression_demos/d1_descent_level13_20260617_204547.dem`
- `android/regression_demos/d1_descent_level13_20260617_204547.dximdemo`
- `android/regression_demos/d1_descent_level13_20260617_204547.dximdemo.rngtrace.jsonl`
- `android/regression_demos/d1_descent_level14_20260617_204837.dem`
- `android/regression_demos/d1_descent_level14_20260617_204837.dximdemo`
- `android/regression_demos/d1_descent_level14_20260617_204837.dximdemo.rngtrace.jsonl`
- `android/regression_demos/d1_descent_level14_20260618_091107.dem`
- `android/regression_demos/d1_descent_level14_20260618_091107.dximdemo`
- `android/regression_demos/d1_descent_level14_20260618_091107.dximdemo.rngtrace.jsonl`
- `android/regression_demos/d1_descent_level15_20260618_091355.dem`
- `android/regression_demos/d1_descent_level15_20260618_091355.dximdemo`
- `android/regression_demos/d1_descent_level15_20260618_091355.dximdemo.rngtrace.jsonl`

Plan:

1. [done] Inspect demo files and RNG sidecar availability.
2. [done] Run level 13 and 14 in plain D1 with result checks.
3. [in progress] If plain D1 result checks pass, run RNG/state trace comparisons where useful; if plain D1 fails, fix D1 replay/runtime first.
4. [in progress] Run level 13 and 14 as d1-in-d2 with result checks.
5. [in progress] If d1-in-d2 fails, use RNG/state traces to identify engine or save-translation splits and fix them.
6. [done] Add broader diagnostics for fresh hand-recorded level 13 and 14 demos.
7. [done] Run scoped build, replay, and code-quality validation for touched files.
8. [done] Run the fresh level 14 and 15 demos in plain D1, then d1-in-d2, using the embedded diagnostics to fix any desyncs.
9. [done] For the remaining fresh level 15 mismatch, compare D1 and d1-in-d2 transparent-wall pixel decisions for D1 textures before adding any broader wall-type rule.
10. [done] Audit the committed D1 behavior deltas and remove non-diagnostic D1 gameplay/replay changes from the working tree.

Notes:

- Level 13 RNG sidecar has 8,056 events and level 14 has 18,145 events, both untruncated.
- Keep terminal replay handling in input-demo hook files, not as menu-specific dismissal logic.
- Plain D1 level 13 still fails at frame 1431. The first useful split is in player homing missile weapon object 109: the new D1 homing diagnostics show frame 1430 updates `fvec`/velocity/track goal, but the older expected trace does not contain the added per-weapon fields. A freshly recorded level 13 demo is needed to continue that branch with the new diagnostics.
- Plain D1 level 14 passes state trace comparison through 2,667 frames. The RNG comparator now treats source-line-only differences as ignored differences, so the level 14 RNG sidecar also passes after local instrumentation shifted `laser.c` line numbers.
- D1-in-D2 level 14 has an immediate diagnostic-only frame 0 mismatch in D2-only/D1-only AI state fields, but the first user-visible `state` mismatch is frame 62: actual shields are 21 while expected shields are 22.
- The frame 60 player/robot and robot/player-weapon inputs match through the shared bump math. D1 and d1-in-d2 both apply the player weapon hit to robot 82 with identical pre/post robot velocity `(-543565,275720,1248828)`, but the D1 object watch shows robot 82 is already back to `(190681,79663,25154)` by the frame 60 `after_move` snapshot. The remaining question is which later collision or cleanup path changes D1's robot velocity before `after_move`; add a shared slot-82 bump watch before changing gameplay.
- The shared bump watch ruled out a later `bump_two_objects()` call: there is no hidden post-weapon bump involving robot 82. A per-object-loop watch shows D1 observes robot 82 with its frame-displacement velocity by the end of the frame, while d1-in-d2 keeps the weapon-bump velocity. A broad attempt to restore already-moved robot velocity after d1-in-d2 weapon hits changed earlier frame-60 state and made the replay less D1-like, so that gameplay change was backed out. Keep the diagnostics and look next for the D1-specific restore inside the moving weapon's physics step, or for a D2-only side effect that prevents that restore.
- Current instrumentation pass should avoid object-number-specific probes. Add broad homing-state, player-owned weapon/robot collision, and per-frame robot velocity-change logging so newly hand-recorded demos can expose similar divergence on different objects and frames.
- Added broad robot watch deltas for nearby robots and player-owned weapon sources, richer shared weapon/robot path and damage-pose logging, and D1 recording-side JSON frame events for homing state, weapon lifetime, and weapon/robot path decisions.
- Follow-up: `-inputdemo-debug-log` is a replay-runner command-line switch, not an Advanced tab option. Recording-side diagnostic events now embed in `.dximdemo` without that hidden flag.
- Verification: scoped `android/run-code-quality.ps1 -Fix` passed and Android `:app:assembleDebug` passed.
- Fresh level 14 and 15 plain D1 replays pass with the new recordings. Fresh level 14 also passes under d1-in-d2 after the homing robot firing readiness fix.
- Fresh level 15 d1-in-d2 is improved but not semantically identical yet. The closed-door `FQ_TRANSPOINT` guard in D1-in-D2 FVI fixed an early player-laser path through a translated closed D1 door and reduced the final count from `powerups_remaining=80` to `powerups_remaining=78`; plain D1 ends at `powerups_remaining=77`.
- Tried and backed out a broader D1-in-D2 block for unblasted `WALL_BLASTABLE` transparent-point crossings. It fixed player-laser signatures 3278/3279 at segment 38 side 4 wall 10, but incorrectly killed later signature 3284 at the same wall. This points to a D1 texture/transparency pixel decision mismatch, not a simple wall-type rule.
- Last-commit D1 audit: the committed `d1/main/game.c`, `d1/main/gamecntl.c`, and `d1/main/state.c` hunks changed replay/gameplay behavior, so they were removed from the working tree. The remaining D1-side changes are diagnostic/probe logging in `collide.c`, `input_demo_hooks.c/.h`, and `physics.c`.
- Level 15 root cause: D1-in-D2 checkpoint world-state restore read saved D1 side `tmap_num`/`tmap_num2` values directly into live D2 segment sides. That made later FVI transparent-wall pixel checks sample the wrong D2 texture even though D1 wall animations had been loaded. `d1_save_translate_apply_d1_world_state()` now converts restored side textures through `convert_d1_tmap_num()`, preserving zero `tmap_num2`.
- D1-in-D2 wall animation support now loads and converts D1 wall clips, applies them while emulating D1, and restores the D2 wall animation table when leaving D1 emulation.
- Current validation: `run-windows-build.ps1 -Game d2` passed, fresh level 15 d1-in-d2 passed strict result checks, fresh level 14 d1-in-d2 passed strict result checks, and scoped `android/run-code-quality.ps1 -Fix -Paths @(...)` passed.
