# Counterstrike level 20 post-blue-key stall

## Goal

Diagnose and fix the deterministic GuideBot route simulation stall after the blue key in Counterstrike level 20 without regressing previously confirmed base-campaign routes.

## Phases

- [x] Reproduce the current headed observation in a focused headless run and identify the stalled path edge, wall state, and objective
- [x] Compare the physical path with the strategic route and determine whether the failure is targeting, path polishing, clearance, or world-state activation
- [x] Implement the narrowest shared correction
- [x] Add or extend a focused deterministic integration regression
- [x] Validate level 20 repeatedly, run the full Counterstrike comparison, build D2, run CTest, and run scoped quality checks

## Findings

- The run deterministically stalls during fly-through trigger 35 after completing the blue key and switches 18 and 12
- Switch 12 opens wall 81 between segments 46 and 358
- Wall 81 is a locked automatic door and has closed again by the time the actor reaches it
- The physical path retains the formerly open edge and flare recovery correctly cannot open the locked door
- A live route refresh can select switch 12 again as a restoring objective, but the simulation previously waited for its 60-second movement timeout before reconsidering the route
- The AI advances `cur_path_index` to the point beyond a doorway before the actor crosses it, so door recovery must inspect the current path point before the following one
- Replanning at the closed trigger-controlled door selects switch 12 again, reopens wall 81, and resumes the interrupted fly-through objective
- Two focused runs completed all eight objectives in 7,902 frames with identical ending simulation RNG state and call count
- The full 30-level Counterstrike sweep had zero nondeterministic pairs and no status changes outside level 20
- Level 20 now finishes the engine route but is classified `route_mismatch` because the live route also selects switch 34, which is absent from the precalculated route input
- The scoped quality pass, final D2 Windows build, all 45 D2 CTest tests, and the focused two-run integration test passed
