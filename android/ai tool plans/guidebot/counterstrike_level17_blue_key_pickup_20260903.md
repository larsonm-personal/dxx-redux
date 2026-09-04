# Counterstrike level 17 blue-key pickup

## Goal

Determine why the GuideBot simulation reaches the blue key area but oscillates
outside pickup range, then correct the shared navigation behavior without
regressing previously confirmed routes.

## Plan

1. Reproduce level 17 twice and capture the objective, actor, key, waypoint,
   distance, and progress-time state at the stall.
2. Compare live key pickup navigation with the route metadata target and engine
   pickup/collision rules.
3. Identify whether the failure is target placement, arrival tolerance,
   collision avoidance, or recovery behavior.
4. Add a focused unit or integration regression that confirms actual key pickup
   and deterministic completion.
5. Implement the smallest shared correction and verify levels 17, 2, and 12.
6. Run the Windows D2 build, native tests, and scoped quality checks.

## Status

- [x] Level 17 reproduced and diagnosed
- [x] Regression coverage added
- [x] Shared correction implemented
- [x] Cross-level simulations passed
- [x] Build, native tests, and quality checks passed

## Findings

- The carrier robot died normally after three simulation seconds and spawned
  the blue key. The stall occurred during the resulting pickup action.
- The GuideBot reached within 175,597 fixed-point units of its waypoint but
  remained outside actual key contact while oscillating.
- The waypoint was placed on the combined actor-plus-key contact shell. Since
  the path follower may stop one actor radius from its final point, that
  placement could leave the actor one radius short of pickup.
- Placing the waypoint just inside the key's segment-center-facing surface
  preserves the wall-safe approach while guaranteeing sphere overlap at the
  normal final-waypoint tolerance.
- After the correction, level 17 collects the blue key and continues through
  the gold key, trigger 14, switch 16, red key, trigger 22, and reactor. A
  separate post-reactor exit-frontier timeout remains.

## Final validation

- The carried blue key is collected at frame 625 in both focused runs.
- Counterstrike level 10's three carrier keys remain confirmed.
- Counterstrike levels 2 and 12 remain confirmed.
- All 30 Counterstrike levels were run twice. Status, frame count, and ending
  simulation RNG matched for every pair.
- Counterstrike retains its prior status totals: 25 ok, 1 failed, 4 timeout.
- Counterstrike level 24 remains confirmed after the fix was narrowed to carrier
  drops.
- Windows D2 build, all 45 native tests, clang-format, and PSScriptAnalyzer
  passed.
