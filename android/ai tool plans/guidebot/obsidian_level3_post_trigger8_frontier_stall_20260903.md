# Obsidian level 3 post-trigger-8 frontier stall

## Phase 1: Reproduce and classify

- [x] Inspect the newest headed-run log and result around the final turn-around
- [x] Map the selected blastable wall, navigation target, path, and final actor segment
- [x] Distinguish an incorrect frontier choice from movement or waypoint failure

## Phase 2: Implement

- [x] Correct the smallest shared routing or simulation behavior responsible
- [x] Avoid mission-specific segment, wall, or trigger exceptions
- [x] Preserve prior Obsidian and Counterstrike route results

## Phase 3: Verify

- [x] Extend the deterministic Obsidian level 3 regression through this frontier
- [x] Run representative route regressions, builds, D2 tests, and scoped quality checks
- [x] Record any later independent failure precisely

## Findings

- The simulation recorded wall 92 as destroyed when the Guide-Bot merely
  reached its activation point. The wall remained `WALL_BLASTABLE` with no
  `WALL_BLASTED` flag
- The next objective therefore had no strategic route. The nearest-progress
  fallback selected segment 239 only because its center was geometrically near
  trigger 11; segment 239 is a sealed three-portal pocket with no actionable
  wall, explaining the turn-around and freeze
- Blastable-wall objectives now emit real flares toward their authored wall and
  complete only after the engine reports `WALL_BLASTED`. The existing
  close-range flare recovery remains the deterministic backup
- Obsidian level 3 now confirms all 21 objectives through the exit in 9,616
  frames. No later failure remains in this level
