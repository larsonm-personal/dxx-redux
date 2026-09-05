# Obsidian level 4 wall-bounce stall

## Phase 1: Reproduce and classify

- [x] Inspect the newest headed result and reproduce it headlessly
- [x] Capture the final objective, actor segment, waypoint path, and collision wall
- [x] Determine whether generation, passability, or path following caused the loop

## Phase 2: Implement

- [x] Make the smallest shared correction without mission-specific identifiers
- [x] Preserve physical in-engine navigation and collision behavior
- [x] Add or extend a deterministic focused regression

## Phase 3: Verify

- [x] Confirm Obsidian level 4 advances beyond the affected objective
- [x] Recheck representative established Guide-Bot routes
- [x] Run Windows and Android builds, D2 tests, quality, and diff checks
- [x] Record any later independent failure

## Findings

- Wall 79 was still a solid closed wall when the route attempted trigger 22
- The dependency planner could reach the trigger wall's source segment and then
  incorrectly treated that as sufficient to activate a non-shootable trigger
- Trigger 6 is the required opener for wall 79
- The corrected route performs trigger 6 first, then physically crosses trigger
  22 after wall 79 has become flyable
- No later failure remains: the simulation reaches the exit in 6,270 frames
