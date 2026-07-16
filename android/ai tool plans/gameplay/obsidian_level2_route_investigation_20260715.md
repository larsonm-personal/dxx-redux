# Obsidian Level 2 Route Investigation

## Goal

Determine why Obsidian level 2 routes to an apparently unreachable shoot switch before the blue key even though the key is reachable from spawn, and correct the responsible shared planner rule if confirmed.

## Plan

- [x] Capture the checked-in and fresh level 2 route, including step paths, trigger sources, visibility, and opened links.
- [x] Independently test direct blue-key reachability from spawn under initial progression state.
- [x] Validate whether the selected shoot-switch activation point is navigable and has a real line of fire to its target.
- [x] Fix the shared reachability or trigger-source selection rule without mission-specific exceptions.
- [x] Add focused native and live regression coverage.
- [x] Run scoped code quality, native tests, Android coverage, and required D1/D2 builds.
- [x] Record the result and any remaining limitations.

## Boundaries

- Do not add Obsidian-specific segment, wall, trigger, or key-order exceptions.
- Preserve legitimate early switches when they are actually required to reach a key.
- Keep the route planner as the shared source for metadata and Guide-Bot routing.

## Result

- The blue key in segment 101 is directly reachable from spawn through open topology.
- The old route began with trigger 4 from segment 90. A live test posed the player at that firing point, aimed at wall 114, and confirmed that sustained primary fire did not activate the trigger.
- The shared planner now tries a directly reachable key when an otherwise complete route begins with a remote shoot-switch objective. After that replan, it prefers the switch source segment when reachable. A legacy whole-route fallback prevents the heuristic from worsening the final route status.
- Obsidian level 2 now routes through the blue key first, then trigger 4 on its source wall in segment 231.
- The focused Android regression verifies the initial blue-key objective, the post-key wall-114 objective, and successful trigger activation from the selected firing position.
- Full metadata regeneration completed for 110 archives with 109 passes, one existing no-descriptor skip, and no generator failures. Only the scoped Obsidian metadata change is retained because the repository's reviewed corpus baseline has unrelated pre-existing regeneration drift.
