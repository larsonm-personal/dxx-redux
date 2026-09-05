# Obsidian level 7 exit stall

## Phase 1: Reconstruct the failure

- [x] Compare level 7 metadata objectives with the recorded simulation objectives
- [x] Locate the latest level 7 engine log and identify the post-reactor physical target
- [x] Reproduce the level headlessly with topology diagnostics enabled

## Phase 2: Classify the cause

- [x] Verify the exit trigger, exit wall, red-door frontier, and traversable segment path
- [x] Determine whether the planner, objective handoff, or live GuideBot navigation diverges
- [x] Document the smallest safe fix and relevant regression coverage

## Findings

- The metadata plan and live objective handoff both select exit trigger 4 on wall 53, segment 9, side 5
- The simulation completes all 11 prerequisites through reactor destruction before selecting the exit
- The exit leg clears its three door frontiers, including red door wall 157, then constructs a 24-waypoint path to segment 9
- The actor stops in segment 72 while waypoint 8 requests segment 74; no wall or unopened frontier exists on that edge
- The level contains 62 sides below the effective player-sized navigation clearance
- Live GuideBot physical-frontier search and its legacy BFS intentionally disable metadata clearance, while the canonical planner uses clearance and entry-to-exit transit masks
- The recent generated-path collision retry only rejects collisions caused by closed trigger-retractable walls, so an ordinary geometry collision on the segment 72 to 74 leg is accepted and stalls forever

## Recommended fix

Reject a colliding interior waypoint only when its waypoint and segment center intersect solid geometry and the existing topology cache also identifies insufficient segment clearance

Use the existing bounded BFS segment-avoidance retry to find another route, leaving doors and trigger interactions unchanged

Broader collision-only and bad-center-only rejection experiments damaged earlier objectives because skewed rooms can contain usable paths despite center collisions; retain the topology cache's isolated-center exclusions

## Implementation verification

- [x] Inspect actual collision results before choosing an edge or waypoint repair
- [x] Implement the shared navigation repair and add an engine regression
- [x] Build Windows D1 and D2 and verify Obsidian 7 plus existing successful routes
- [x] Refresh the affected simulation entry and run scoped code quality

## Results

- Obsidian 7 completes all prerequisites and the exit in 7845 frames (131 rounded seconds)
- Checked all 88 levels in Counterstrike, First Strike (D1-in-D2), Castaway Redux, and Obsidian: no formerly ok result became partial or failed
- Castaway 2 progresses from 10 to 14 completed objectives, then reports a later no-goal failure instead of its earlier timeout; that separate failure remains unresolved
- The repair is in the live D2 GuideBot path generation used by the simulation and Android, not a simulation-only shortcut; native D1 has no GuideBot, while D1-in-D2 uses this same D2 path
- Windows D1 and D2 builds passed; existing D1 weapon.c return-path warnings remain unrelated
- All 45 D2 CTest tests passed; D1 has no registered CTest tests
- The new test_obsidian_level7_exit_route.ps1 passed as written, confirming all keys, reactor, exit, actor radius, and identical full results across two runs
- Scoped code quality passed; only Obsidian 7 and the corresponding mission summary changed in checked-in simulation data
