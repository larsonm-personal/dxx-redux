# Guidebot unreachable fallback and exploration goal sketch

## Goal
Make stale or legacy unreachable guidebot goals fall back to navigating as close as possible, and sketch a future guidebot goal for finding the nearest waypoint toward the largest reachable unexplored mine area.

## Plan
- [x] Read project instructions
- [x] Inspect current guidebot nearest-point fallback and goal/menu plumbing
- [x] Patch unreachable boss/route fallback behavior with minimal source changes
- [x] Add an initial design sketch for the unexplored-area guidebot goal
- [x] Run scoped quality and relevant build/test checks

## Notes
- `exists_in_mine()` can return `-2` for "object exists but guidebot cannot reach it", which bypassed the existing nearest-point fallback because no target segment was retained.
- The existing nearest-point fallback was also controlled by the Android gameplay preference, so progression-critical boss/reactor/exit goals could still hard fail with the preference off.
- Normal Android play now treats route goals, boss, reactor, and exit as progression goals that should best-effort to the nearest useful reachable point. Input-demo replay keeps the old behavior unless the existing preference is explicitly enabled.

## Initial Sketch: Unexplored Goal
- Add a new guidebot command/menu goal named `Unexplored`.
- Treat `Unexplored` like the normal end-of-level route objective, not like an isolated point target.
- Normal route objective example: blue key, gold key, red key, reactor, exit.
- `Unexplored` route objective example: blue key, gold key, red key, required triggers/hidden walls/doors, unexplored frontier.
- When the player selects `Unexplored`, build a route to the best unexplored destination with the same intermediate waypoint machinery used for the exit route. The final destination changes from exit/reactor chain completion to unexplored area progress, but key pickup, door, trigger, hidden-wall, and nearest-progress handling should behave the same.
- Data sources:
  - `Automap_visited[MAX_SEGMENTS]` tells whether the player has explored/drawn each segment.
  - Existing guidebot live traversal helpers provide the currently flyable reachable set.
  - Existing nearest-progress helpers provide optimistic traversal through doors, key doors, hidden walls, and trigger-opened obstructions.
  - Metadata route links can describe the first useful obstruction waypoint when a direct frontier is blocked.
- Region model:
  - Mark unexplored candidates as `Automap_visited[seg] == 0`.
  - Flood-fill contiguous unexplored components over structural mine connectivity, using the same optimistic edge rules as nearest-progress pathing so hidden/door-separated areas remain coherent regions.
  - For each component, count segments and keep one or more frontier contacts: explored/reachable segment adjacent to the component, or the first route waypoint on the optimistic route toward it.
- Target selection:
  - Prefer the largest component that has some reachable or progress-reachable frontier.
  - Break ties by shortest current guidebot/player path to the frontier waypoint, then by fewer obstruction assumptions, then by segment index for stability.
  - Feed the chosen component/frontier into the existing route builder as the alternate terminal target.
  - If the selected component is directly reachable, the selected route step should be the nearest explored segment adjacent to the unexplored component.
  - If blocked, the selected route step should be the next intermediate waypoint that opens progress: key, shoot switch, hidden wall, door/frontier segment, using the same obstruction pathing already used for end-of-level goals.
- User-facing behavior:
  - If a frontier is reachable: `Finding unexplored`.
  - If the route is blocked but progress is possible: use the intermediate route step wording, such as `Finding NEXT: shoot the switch` or `Finding NEXT: shoot hidden wall`.
  - If no unexplored reachable/progress-reachable region exists: `No reachable unexplored area found`.
- Implementation shape:
  - Model this as an alternate route target mode for the Android metadata/guidebot route system.
  - Reuse the existing route step selection path so guidebot can advance through keys, triggers, hidden walls, boss/reactor-like object targets if needed, and nearest-progress fallback before reaching unexplored.
  - Add introspection fields for active route target mode (`end_of_level` or `unexplored`), selected component size, target segment, guidance mode, first blocker wall/trigger/key, and selected frontier segment.
- Test shape:
  - Add a focused script that starts a known level, seeds/uses natural `Automap_visited`, invokes `Unexplored`, and asserts the selected final target belongs to a nonzero-size unexplored component.
  - Add a second case where the largest unexplored component sits behind a known door/hidden wall, and assert the selected next route step is the intermediate obstruction/key/switch step rather than the final unexplored frontier.

## Implementation Phase
- [x] Add an explicit guidebot route target mode for `Unexplored`.
- [x] Select the largest contiguous automap-unexplored component and a reachable/progress waypoint toward it.
- [x] Reuse the existing end-of-level intermediate route steps before switching the terminal target to `Unexplored`.
- [x] Expose the goal through the guidebot/direct-command path used by Android automation.
- [x] Add focused validation for the new route target and for existing end-of-level routing.
