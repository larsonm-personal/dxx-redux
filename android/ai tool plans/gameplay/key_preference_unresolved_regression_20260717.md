# Key-preference unresolved regression

## Plan

- [x] Reproduce the Counterstrike level 3 trigger 8 regression and compare permissive and strict plans.
- [x] Identify why a route containing an unresolved objective was accepted as the preferred strict route.
- [x] Reject preferred-route substitutions that reduce calculation quality, without base-mission or level-specific exceptions.
- [x] Add focused regression coverage for Counterstrike level 3 and retain Obsidian level 2 coverage.
- [x] Regenerate affected metadata and audit the full corpus for new unresolved or status regressions.
- [x] Run D1/D2 builds, Android verification as needed, and scoped quality checks.

## Constraints

- Treat newly unresolved objectives in the base Descent and Descent 2 missions as regressions.
- Preserve transparent-shot alternatives and `can_be_bypassed` annotations when the preferred route is at least as calculable as the permissive route.
- Do not add mission, level, trigger, segment, or texture-specific exceptions.

## Findings

- The key-preference selector treated route status `ok` as sufficient, even though the planner intentionally permits `ok` routes to contain unresolved-trigger fallback steps.
- Counterstrike level 3's permissive route calculated trigger 8 from segment 297, while the strict candidate replaced it with an unresolved trigger at segment 116.
- A strict candidate is now accepted only when it has a genuinely different objective sequence and does not increase the unresolved-trigger count.
- Counterstrike level 11 also returned to its original segment 201 firing waypoint because its strict candidate changed only shooting geometry, not progression.
- Obsidian level 2 still prefers blue key, triggers 4, 5, and 10, reactor, trigger 11, and exit, with the blue key marked `can_be_bypassed`.
- Full corpus regeneration passed 109 archives, skipped one archive without a descriptor, and failed zero archives.
- Compared with pre-key-preference metadata, the regenerated corpus has zero new unresolved objectives and zero `ok` status regressions.
- The tightened rule annotates 14 keys in 10 levels, down from the overly broad 87 keys in 51 levels.
- D1 and D2 metadata builds, metadata scan tests, route cache tests, route snapshot tests, focused Counterstrike and Obsidian tests, scoped code quality, and Android `assembleDebug` passed.
