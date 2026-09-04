# Counterstrike level 6 blue-door stall

## Goal

Identify and fix the GuideBot route-confirmation stall beside the blue door after collecting the blue key, without weakening deterministic route validation.

## Plan

1. Inspect the checked-in route metadata and simulation result for Counterstrike level 6.
2. Reproduce the level with the current Windows headless route engine and capture the post-blue-key planner, waypoint, door, and flare state.
3. Determine whether the failure is route planning, waypoint traversal, keyed-door discovery, flare targeting, or door activation.
4. Implement the smallest shared engine correction and add a focused deterministic regression for this level.
5. Rebuild D2, run the focused reproduction twice, run native tests and scoped quality checks, and update only the level 6 simulation record if successful.

## Status

- [x] Metadata inspected
- [x] Stall reproduced and diagnosed
- [x] Fix and regression implemented
- [x] Build and tests passed
- [x] Simulation record updated

## Result

The route stalled at the open portal from segment 62 to segment 65, before the
blue door frontier. The safety waypoint was assigned to segment 65 but placed
close enough to its portal plane that the player-sized simulation actor stopped
outside the normal waypoint threshold while still classified in segment 62.

Route-aware waypoint advancement now accepts this clearance case only when the
waypoint segment is directly adjacent, the connecting portal is currently
flyable, and the actor is within one radius of the normal threshold. Closed
doors and solid walls cannot use this recovery. Counterstrike level 6 now
confirms twice deterministically in 2,914 frames, and levels 10, 11, and 24 also
retain deterministic confirmation.
