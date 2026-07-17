# Ordinary shoot-open door routes

## Plan

- [x] Replace the hidden-door filter with the engine's actual unlocked, keyless shoot-open rule.
- [x] Characterize every base Counterstrike route change caused by the general rule.
- [x] Determine whether changed routes are physically valid or expose a separate dependency bug.
- [x] Update metadata and integration coverage for legitimate changes without mission-specific exceptions.
- [x] Run corpus comparison, D1/D2 and Android builds, device coverage, and scoped quality checks.

## Findings

- `WCF_HIDDEN` controls appearance and automap treatment, not whether weapon impact opens a door.
- `wall_hit_process` opens any `WALL_DOOR` that has `KEY_NONE` and is not `WALL_DOOR_LOCKED`.
- Projectile route analysis now uses that same rule. It does not inspect the hidden attribute and does not contain mission-specific handling.
- In Counterstrike, the rule changes firing poses for level 2 trigger 17, level 11 trigger 10, and level 14 trigger 5. It does not change objective order or route status. The previously observed level 13 shortcut came from a separate target-controlled-door experiment, which remains removed.
- Full-corpus regeneration changed firing geometry in 49 levels across 30 mission metadata files. There were no objective sequence, route status, or calculated/not-calculated count changes.
- The rule adds one wall predicate to an existing ray test. It adds no samples, searches, or route-analysis passes.
- Verification passed for D1 and D2 Windows builds, metadata scan tests, route cache tests, focused Counterstrike integration coverage, Android debug assembly, the Counterstrike level 2 device test, scoped code quality, and `git diff --check`.
