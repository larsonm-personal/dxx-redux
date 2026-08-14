# Live Difficulty State Adjustment Audit

## Goal

Identify persistent game state, beyond boss health, that is derived from difficulty and may need a deterministic adjustment when difficulty changes during a level.

## Scope

- Audit D1 and D2 creation-time assignments and timers that depend on `Difficulty_level`.
- Separate continuously evaluated behavior from values baked into existing objects.
- Recommend adjustments only when they preserve the player's existing state cleanly.
- Avoid retroactive grants, encounter resets, and changes that could introduce multiplayer or demo divergence.

## Plan

1. [completed] Inventory difficulty-dependent persistent object fields, resources, capacities, and timers.
2. [completed] Check save/restore, multiplayer, and demo implications for each candidate.
3. [completed] Rank candidates as adjust now, intentionally leave unchanged, or investigate separately.
4. [completed] Record focused regression tests for any recommended implementation.

## Implementation

1. [completed] Centralize D1/D2 boss gate interval and D2 vulnerable-dot formulas and refresh them on live changes and save restore.
2. [completed] Centralize D2 thief/guidebot maximum shields and preserve live shield percentages during difficulty changes.
3. [completed] Add focused maximum-health and percentage tests.
4. [completed] Run scoped formatting, native tests, D1/D2 desktop builds, the Android debug build, and the D2 boss difficulty/save integration test.

## Verification

- Windows D1 and D2 release builds passed, including game and headless metadata targets.
- D1 native suite passed 33 of 33 tests.
- D2 native suite passed 40 of 40 tests.
- Android `:app:assembleDebug` passed for all configured ABIs.
- `test_d2_boss_difficulty_save_restore.json5` passed all 33 automation steps on the emulator.

## Findings

### Adjust immediately with a live difficulty change

- D1 and D2 `Gate_interval` are derived from difficulty only in `init_ai_objects()`. A live change currently leaves boss robot gating cadence on the old difficulty indefinitely. Recompute the interval in the shared change path, but retain `Last_gate_time` so changing difficulty does not manufacture or erase a completed gate event.
- D2 `Boss_invulnerable_dot` is derived from difficulty only in `init_robots_for_level()`. A live change currently leaves the vulnerable-area threshold on the old difficulty indefinitely. Recompute it in the shared change path.
- D2 guidebot shields are difficulty-scaled at level initialization, including a special Trainee maximum. Treat this like boss health: calculate old and new maxima and preserve the live shield percentage. Skip dead/dying guidebots. Extracting the maximum formula to a helper avoids duplicating the unusual Trainee rule.

### Leave existing state unchanged

- Existing robot fire, thief, and miscellaneous AI timers may have one already-scheduled interval from the previous difficulty. Future scheduling uses the current difficulty. Rewriting every pending timer is intrusive and the stale effect is short-lived.
- Existing weapon objects contain creation-time speed and strength, while some homing, guided, and explosion behavior reads current difficulty. Do not walk and rewrite projectiles in flight; that would create abrupt trajectory/damage changes and complicate demo and coop determinism.
- Reactor countdown duration is chosen when destruction occurs. Do not lengthen or shorten a countdown already underway. Reactor firing cadence before destruction already uses current difficulty when scheduling its next shot.
- Matcen capacity, remaining lives, activation state, and timers represent encounter history. Do not refund or remove capacity/lives on a difficulty change. New activations and live limits already consult current difficulty where designed.
- Player shields, energy, ammunition, and already-awarded score represent player history. Do not grant or remove resources retroactively.
- Regular robot and reactor health are not difficulty-scaled maxima, so they need no proportional adjustment.

### Separate scoring policy issue

- Level-end bonus calculation and high-score difficulty labels use the difficulty active at the end. A player can raise difficulty immediately before exiting to receive or display a harder-difficulty result. If this matters, add a per-level minimum difficulty and use it for level-end bonuses; use the run-wide minimum or an explicit mixed label for whole-run records. Do not reuse the existing run-wide minimum for every later level's bonus.

### Regression coverage for an implementation

- Unit-test D1 and D2 gate interval formulas for every old/new difficulty pair.
- Unit-test D2 boss vulnerable-dot refresh for every difficulty.
- Unit-test D2 guidebot percentage preservation across every pair, especially transitions into and out of Trainee.
- Extend the deterministic difficulty-change replay test to assert the derived globals and guidebot shields at the change frame.
- Save after a change and restore with a different launcher/default difficulty, then assert difficulty, gate interval, vulnerable dot, boss health, and guidebot health all agree with the save.
