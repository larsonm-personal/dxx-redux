# Counterstrike level 23 post-key oscillation

## Goal

Diagnose and fix the deterministic GuideBot simulation oscillation between the regular and red doors after all keys are collected in Counterstrike level 23, without regressing confirmed campaign routes.

## Phases

- [x] Reproduce level 23 twice and identify the active objective, path indices, and relevant door states during the oscillation
- [x] Determine whether the reversal comes from physical path following, door recovery, or live prerequisite replanning
- [x] Implement the narrowest shared correction
- [x] Add a focused deterministic integration regression
- [x] Validate level 23 repeatedly, compare the full Counterstrike campaign, build D2, run CTest, and run scoped quality checks

## Findings

- The simulation deterministically completes all three keys and fly-through trigger 5 before oscillating
- The movement is goal-driven rather than waypoint jitter: live rescans alternate between already-completed fly-through triggers 10 and 5
- Each trigger restores an automatic door, but travelling to the other trigger lets the first door close, producing an endless prerequisite cycle
- The planner needs route-progress memory: normally advance past already-recorded objectives, while retaining the existing explicit replan when a physical path actually encounters a reclosed trigger-controlled door
- Recovery now resolves a controlled door from either wall side and requests the exact controlling trigger rather than the live scan's first global prerequisite
- Reactors and bosses are destroyed as soon as visible from a firing position, matching the simulation design instead of requiring contact with the target point
- Two focused runs completed all fourteen objectives in 3,846 frames with identical ending simulation RNG state and call count
- The full 30-level Counterstrike sweep had zero nondeterministic pairs, no `ok` regressions, and only level 23 changed status from `timeout` to `ok`
- The full Counterstrike simulation regression was regenerated so firing-point timing changes are recorded consistently
- The final D2 Windows build, all 45 D2 CTest tests, the focused level 23 test, the level 20 restoration test, and scoped quality checks passed
