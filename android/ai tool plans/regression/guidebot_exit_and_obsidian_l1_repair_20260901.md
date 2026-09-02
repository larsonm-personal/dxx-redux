# GuideBot exit pause and Obsidian level 1 repair

## Goal

Remove the route-confirmation pause before an authored exit crossing and restore
Obsidian level 1's prerequisite-aware strategic route without mission-specific
logic or regressions to Castaway.

## Phases

- [ ] Reproduce the exit pause and Obsidian level 1 failure with current engine logs
- [ ] Compare current and prior Obsidian level 1 route metadata and planner decisions
- [ ] Make authored exit crossings extend immediately when their source side is reached
- [ ] Repair the general prerequisite or route-state inference responsible for Obsidian level 1
- [ ] Add focused Counterstrike, Obsidian, and Castaway regression coverage
- [ ] Run scoped quality checks, Windows and Android builds, and relevant tests

