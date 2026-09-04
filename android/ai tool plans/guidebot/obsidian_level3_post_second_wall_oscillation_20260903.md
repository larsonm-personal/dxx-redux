# Obsidian level 3 post-second-wall oscillation

## Phase 1: Reproduce and classify

- [x] Capture the route, path indices, segment transitions, and replans after the second blastable wall
- [x] Determine whether the loop originates in path generation, path polishing, or movement recovery
- [x] Identify the exact world-state or geometry condition that sustains the oscillation

## Phase 2: Implement

- [x] Correct the shared Guide-Bot behavior without mission-specific exceptions
- [x] Preserve the completed narrow-floor detour and established D2 route results
- [x] Add bounded diagnostics or recovery only where needed

## Phase 3: Verify

- [x] Extend the Obsidian level 3 deterministic integration test past the oscillation
- [x] Recheck representative Counterstrike routes and exact frame counts
- [x] Run Windows and Android builds, D2 tests, code-quality checks, and diff validation
- [x] Record any next independent failure

## Findings

- The generated path from segment 351 to fly-through trigger 8 was valid and
  forward-only
- `ai_follow_path` accepted the final waypoint, then immediately applied the
  legacy patrol behavior for `AIM_GOTO_OBJECT` by reversing `PATH_DIR` inside
  the same call. The route driver therefore never observed a stable endpoint
  and the Guide-Bot repeatedly traversed segments 353, 354, 355, and 359 in
  both directions
- Active Guide-Bot objectives now hold their final waypoint instead of turning
  into a patrol. The route driver then performs its existing physical approach
  to the objective
- The focused run now reaches fly-through trigger 8 at frame 1404 and destroys
  the following blastable wall at frame 1565. A separate later frontier
  interaction still times out
- Counterstrike levels 1, 2, 6, 11, and 23 and Obsidian level 1 retained their
  established deterministic results. The static Counterstrike level 2 trigger
  21 firing-waypoint test still fails independently of path following
