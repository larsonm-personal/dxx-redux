# Obsidian level 3 narrow-portal route fix

## Phase 1: Reproduce and identify the false edge

- [x] Capture the segment path and physical frontier immediately after trigger 6
- [x] Inspect the portal geometry, wall state, and effective actor radius
- [x] Identify trigger 21 as the optional shortcut opener and confirm a longer route exists without it

## Phase 2: Correct route feasibility

- [x] Validate generated Guide-Bot waypoint legs against live engine collision before committing to them
- [x] Retry around an unoccupiable waypoint beside an active closed-wall trigger barrier
- [x] Keep already-open barriers, ordinary doors, and unrelated collision-shell contacts unchanged
- [x] Avoid mission-specific segment or trigger exceptions

## Phase 3: Verify

- [x] Extend the Obsidian level 3 integration test through fly-through trigger 7
- [x] Check Counterstrike levels 1, 2, 6, 12, and 17 for regressions
- [x] Run Windows and Android builds, D2 tests, and scoped code-quality checks
- [x] Record the later independent stall while approaching fly-through trigger 8

## Results

- The short generated path placed a full player-sized collision sphere at a
  waypoint overlapping wall 102, part of the floor retracted by trigger 21
- Trigger 21 is an optional shortcut opener, not a true prerequisite; a valid
  longer route to trigger 7 exists while the floor remains closed
- Guide-Bot route paths now validate generated waypoint legs with the live
  engine collision query and the larger of player and Guide-Bot radii
- Recovery is limited to destination waypoints obstructed by an active
  `WALL_CLOSED` side linked from a `TT_OPEN_WALL` trigger, then retries while
  avoiding that interior segment
- Two deterministic Obsidian level 3 runs complete trigger 7 at frame 600 and
  the following blastable wall at frame 1143
- The next independent stall is near trigger 8, after the section fixed here
- Counterstrike levels 1, 2, 6, 12, and 17 retain their prior successful status
  and exact frame counts
