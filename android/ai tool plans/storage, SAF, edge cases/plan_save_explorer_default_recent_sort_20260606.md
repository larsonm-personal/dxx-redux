[x] Trace save explorer set-mode row ordering
[x] Change default save-set ordering to most recent first
[x] Add or update unit coverage for default set ordering
[x] Run scoped formatting and targeted tests/build

# Save Explorer Default Recent Sort Plan

## Goal

In the save explorer's default save-set view, present saves most-recent-first by default instead of slot-number order.

## Notes

- Do not change the explicit `ten most recent` mode, which already spans all game/pilot/mission sets.
- Keep empty slots visible if the current save-set view already shows them, but order occupied slots by save time before empty or undated entries.
