# Obsidian Next-Level Precalculation Investigation

## Goal

Determine why entering Obsidian level 4 showed "Still calculating" even though level 3 had been active long enough to precalculate the next level.

## Plan

- [x] Extract the level transition and precalculation timeline from the supplied debug log.
- [x] Trace the relevant scheduling, cache, and level-transition code paths.
- [x] Identify the cause or the narrowest missing diagnostic evidence.
- [x] Report findings and recommended next action without changing behavior.

## Boundaries

- Treat this tranche as diagnosis only.
- Do not modify game behavior or invalidate existing user changes.

## Result

- The in-game scheduler skipped Obsidian level 4 during level 3 because its ledger already marked the level-4 artifact complete.
- The completed artifact used analysis profile `82284bf7585149ab` and topology `8109e06fc89d4a59`.
- On entering level 4, the live game requested analysis profile `81ab73f7579fcc8f` and topology `ec229adce904c311`, so cache adoption missed and active calculation began.
- Reversing the profile hash inputs showed that the worker used player radius `310325`, while the live game used `310313`; both used projectile radius `11883`.
- The player-radius difference also changes topology side-clearance values, explaining both mismatched key components.
- The likely repair is to normalize the metadata worker's player object to the runtime player-ship radius before scanning, then add transition coverage proving a next-level artifact is adopted by the live game.

## Follow-up: Radius Source

- [x] Trace all assignments and serialization paths for player object size.
- [x] Determine why the metadata worker and live game differ by 12 fixed-point units.
- [x] Report the exact source of the variation.

The level loader first reads the serialized object size from the level file. During verification it calls `init_player_object()` only when the loaded object occupies the slot currently referenced by the stateful `ConsoleObject` pointer. That call replaces the serialized size with the current player polygon model radius. The metadata worker and live game enter the load with different `ConsoleObject`/player-slot state, so one scan retains `310325` from the level while the other uses the current model radius `310313`. The radius is therefore not changing during play; level loading is selecting between two nearly identical sources based on prior global state.

## Follow-up: Constant Origins

- [x] Trace `310325` to the stock D2 player polymodel record.
- [x] Trace `310313` to Obsidian's per-level HXM replacement for player model 108.
- [x] Explain why the two values diverge between the worker and live transition.

Stock D2 data gives player polygon model 108 radius `310325`. Obsidian advertises a new player ship and embeds a replacement for model 108 in every level HXM; `pliom.hxm` gives that model radius `310313`. During a live transition, `secret_area_prepare_current_level()` runs before `load_level_robots()`, so the prior level's Obsidian HXM remains installed while the new level is fingerprinted. The metadata worker calls the lower-level `load_level()` and scans without loading the HXM, so it fingerprints the stock player ship. The mismatch is therefore mission replacement data plus inconsistent load order, not numeric instability or two engine constants.
