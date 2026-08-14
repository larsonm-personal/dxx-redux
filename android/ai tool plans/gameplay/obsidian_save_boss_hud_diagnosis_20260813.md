# Obsidian save boss HUD diagnosis

## Goal

Explain why loading an Obsidian level 3 save can display the boss health bar before the player reaches or is attacked by the boss, and why the bar initially reports 50 percent health.

## Plan

- [x] Trace boss HUD visibility, boss selection, and maximum-shield initialization
- [x] Trace save restoration and level-start ordering for the HUD and boss AI state
- [x] Review recent changes and existing boss HUD regression coverage
- [x] Reproduce or inspect the relevant state with existing automation and introspection where practical
- [x] Record the most likely cause, supporting evidence, and the smallest safe correction to test

## Root cause follow-up

- [x] Reproduce an ordinary Obsidian boss save and reload at multiple difficulties
- [x] Compare serialized shields with pre-save runtime shields and restore-time defaults
- [x] Trace custom HXM/robot strength loading and difficulty ordering across save restore
- [x] Determine whether the invalid branch reflects serialization corruption, asset mismatch, or a bad validity rule
- [x] Add or identify the regression test needed before changing the repair behavior

## Constraints

- Diagnose first; do not change gameplay behavior without a separate implementation request
- Prefer introspection and Game Logs over visual inspection
- Keep D1 and D2 behavior in mind even if the report concerns a D2 mission

## Findings

- The August 2 boss HUD added a restore fallback that scans every live boss in the mine. Any boss whose current shields are below the difficulty-adjusted `Robot_info` strength is treated as active, even if the player has not seen, hit, or been attacked by that boss.
- D2's inherited save restore code recalculates each boss's default shields. If the saved shield value is nonpositive or exceeds that recalculated default, it rejects the value and executes `obj->shields /= 2` with the historical comment `give player a break`.
- Those paths compose exactly: a rejected saved value becomes precisely 50 percent, then the HUD's global damaged-boss scan immediately displays it.
- A fresh automated start of Obsidian level 13, `Obsidian 3 TCG Depot`, on Trainee produced `boss_health.active=false` and `drawn=false`. This rules out normal initial level data and the metadata worker as the immediate cause.
- The existing boss HUD integration test covers a fresh stock level and deliberate damage, but not save restore, invalid shield repair, or a distant boss.
- Added Game Logs diagnostics for the saved/default/result boss shield values during D2 restore and for damaged-boss HUD auto-activation. A phone reproduction will show which invalidity branch fired.
- The smallest correction to test is to stop using global shield deficit as an engagement proxy. Persist explicit HUD engagement state in new saves, or keep the bar hidden after restore until an actual boss attack/hit/teleport notification occurs. Separately, replace the ancient half-health fallback with validated preservation or clamping only after identifying why the saved value was rejected.
- Validation passed: scoped code quality, fresh Obsidian level 13 automation probe, Windows D1/D2 build, and Android debug APK build for arm64-v8a, armeabi-v7a, and x86_64.

## Root cause follow-up findings

- The save did not fail globally. One legacy D2 boss-shield validity check rejected a valid serialized value.
- The failure is reproducible when difficulty is lowered during a live level. This is an Android-supported feature, and its design intentionally leaves already-created robot shields unchanged.
- Obsidian level 13 reproduction: the boss starts on Rookie with `239616000` shields from robot strength `383385600`. Changing to Trainee correctly leaves the live boss at `239616000`, and that exact value is serialized.
- Restore reads the saved Trainee difficulty, recomputes the boss default as `95846400`, and incorrectly treats the legitimate `239616000` saved value as bogus because it exceeds the new-difficulty default.
- The inherited recovery branch then sets shields to half the new default, `47923200`. The HUD compares this against `95846400`, producing an exact 50 percent bar and activating globally.
- A same-difficulty Rookie save/restore control preserved `239616000` and kept the boss HUD hidden. Serialization, custom HXM loading, and ordinary restore ordering are working in that case.
- The correct regression should start a boss level on Rookie, lower to Trainee without touching the boss, save and restore, then require the original boss shields to remain unchanged and the HUD to remain hidden. The temporary reproduction passed for both the faulty sequence and the same-difficulty control; it was not retained because a checked-in test should assert the corrected behavior rather than bless the bug.
- The validity rule is incompatible with live difficulty changes. Difficulty history is already serialized (`difficulty_changed`, minimum, and maximum), so restore has enough information to distinguish this legitimate case from corruption.

## Implementation

- [x] Centralize the D1/D2 boss maximum-shield calculation used by level initialization, the HUD, and difficulty changes
- [x] Rescale every live boss proportionally when difficulty changes
- [x] Preserve deterministic rounding, positive live shields, and unchanged D1 behavior
- [x] Add focused native tests for full, damaged, dead, and round-trip scaling
- [x] Add an Obsidian save/restore regression covering Rookie to Trainee
- [x] Run scoped quality checks, native tests, Android integration, and D1/D2 builds

## Implementation results

- `difficulty_change_to()` now rescales every live boss before publishing the new difficulty. The calculation is `old_shields * new_maximum / old_maximum`, rounded to the nearest fixed-point unit.
- Full-health bosses remain full, damaged bosses keep the same health percentage, dead bosses remain dead, and a positive live boss cannot round down to zero.
- D1 maximum boss shields do not vary by difficulty, so D1 values remain bit-for-bit unchanged.
- Active boss HUD state refreshes its maximum after the rescale instead of briefly displaying the old scale.
- Obsidian level 13 evidence after the fix: Rookie `239616000/239616000` became Trainee `95846400/95846400`; the save wrote `95846400`; restore accepted `95846400` as valid; the boss bar remained hidden.
- Validation passed: scoped code quality, automation catalog validation, Windows D1 and D2 builds, all native CTest suites for both games, Android debug build for all three ABIs, and the new emulator save/restore regression.
