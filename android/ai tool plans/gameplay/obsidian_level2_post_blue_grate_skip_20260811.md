# Obsidian Level 2 Post-Blue Grate Skip

## Goal

Prevent the shared route planner from preferring Obsidian level 2's awkward
post-blue-key shot through a grate when a simpler direct shot can advance the
same route, without adding mission-specific exceptions.

## Plan

- [x] Reconstruct the post-blue-key recommendation and the recent simpler-shot
  preference rule from source, diagnostics, metadata, tests, and history.
- [x] Define and implement the narrowest data-driven condition that rejects or
  deprioritizes the grate skip while preserving legitimate remote shots.
- [x] Add focused regression coverage for Obsidian level 2 and an appropriate
  positive control.
- [x] Run scoped formatting, native route tests, relevant builds, and the
  focused Android integration test where practical.
- [x] Record the final behavior and validation results here.

## Constraints

- Do not add Obsidian-specific mission, level, segment, wall, trigger, or
  coordinate handling.
- Preserve valid switch shots through transparent or illusion geometry when
  they are genuinely the simplest reachable route action.
- Preserve unrelated worktree changes.

## Findings

- The grate shot was physically valid, so projectile collision validation was
  correctly accepting it. The bad recommendation came from firing-pose ranking:
  the nearer travel route won before the planner examined the switch-side pose
  beyond the blue door.
- The existing level-2 test granted the blue key while leaving the player at
  spawn and selected Guide `Next` before normal pickup-event coalescing. It did
  not reproduce ordinary play.
- Firing-pose selection now separately tests the switch's own segment when the
  player holds a key and the path to that segment traverses a door requiring
  that key. It overrides the ordinary shortest-travel result only when it also
  produces a physically shorter shot.
- Obsidian level 2 retains the direct trigger-4 pose in segment 231. After that
  shot, trigger 5 now uses its source segment 215 instead of the remote segment
  54 grate waypoint.
- Remote shots are unchanged when there is no held-key conventional approach.
  The focused native negative control and the full KCX-F2 GuideBot route test
  both pass.

## Validation

- Scoped code quality passed for the changed route files.
- D1 and D2 Windows builds passed.
- D1 and D2 `test_route_snapshot` and `test_level_metadata_scan` passed.
- The focused Obsidian metadata key-preference test passed.
- Android `assembleDebug` passed for arm64-v8a, armeabi-v7a, and x86_64.
- The installed 194-step Obsidian test passed with exact segment 231 and 215
  firing-waypoint assertions.
- The installed 176-step KCX-F2 positive-control test passed.
