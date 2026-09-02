# GuideBot exit pause and Obsidian level 1 repair

## Goal

Remove the route-confirmation pause before an authored exit crossing and restore
Obsidian level 1's prerequisite-aware strategic route without mission-specific
logic or regressions to Castaway.

## Phases

- [x] Reproduce the exit pause and Obsidian level 1 failure with current engine logs
- [x] Compare current and prior Obsidian level 1 route metadata and planner decisions
- [x] Make authored exit crossings extend immediately when their source side is reached
- [x] Repair the general prerequisite or route-state inference responsible for Obsidian level 1
- [x] Add focused Counterstrike, Obsidian, and Castaway regression coverage
- [x] Run scoped quality checks, Windows and Android builds, and relevant tests

## Result

- Counterstrike level 1 reaches the exit 138 frames after reactor completion,
  below the regression ceiling that excludes the former one-second hold.
- Obsidian level 1 now plans and confirms blue key, red key, switches 6 and 5,
  fly-through trigger 8, reactor, and exit in 9,325 deterministic frames.
- The simulator uses the larger mission-derived player or Guide-Bot radius,
  matching the host planner's clearance assumptions.
- Castaway levels 1 and 2 retain strict complete routes.
- Obsidian level 10 remains unchanged in the checked metadata. A full current
  regeneration still classifies its unresolved trigger 0 diagnostic chain as
  partial; that pre-existing strict-planner gap was not relabeled as a fix.
