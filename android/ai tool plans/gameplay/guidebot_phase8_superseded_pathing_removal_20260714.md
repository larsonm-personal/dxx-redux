# Guide-Bot Phase 8 Superseded Pathing Removal

## Goal

Remove route-planning, fallback, visibility, completion, and diagnostic implementations that were retained only for migration comparison after the shared C++ planner became authoritative for metadata and live Guide-Bot semantic goals.

## Plan

- [x] Inventory every Phase 8 deletion item, its callers, introspection fields, tests, and shared replacement.
- [x] Remove live legacy selector comparison and superseded selector diagnostics.
- [x] Remove the duplicate optimistic reachable-segment BFS and nearest-point fallback from `escort.c`.
- [x] Replace live route-step satisfaction reconstruction with shared plan/action completion state, then remove duplicate helpers.
- [x] Remove duplicate live visibility sampling while retaining shared player-guidance visibility and classic AI visibility unchanged.
- [x] Remove the old C semantic route planner, global scratch arrays, and keyed/non-keyed opener policies from `level_metadata_scan.c`.
- [ ] Remove dead route kinds, mappings, debug fields, and tests that no longer have a producer; retain stable public data needed by metadata JSON and automap presentation.
- [ ] Prove no `Metadata_route_path_depth` or `create_path_to_segment_metadata_route` shim remains.
- [ ] Run scoped quality and all D1/D2 native route, completion, projection, ownership, and determinism tests.
- [ ] Run Windows D1/D2, Android all-ABI, full route corpus, strict base-campaign, and focused live Guide-Bot fixtures.
- [ ] Audit every Phase 8 requirement against current source and test evidence, then update the master plan.

## Boundaries

- Do not alter the classic generic AI pathfinder, doorway predicate, `Point_segs`, path request cadence, path polishing, return-to-player behavior, steering, collision handling, generic flare behavior, or simulation RNG.
- Do not remove shared C++ player-guidance visibility, normalized action completion, topology snapshots, or endpoint policies.
- Do not treat a freshly generated metadata corpus as its own baseline; compare against the reviewed frozen regression data.

## Checkpoint 2026-07-14

- Removed the live legacy selector, duplicate route satisfaction analysis, and optimistic nearest-point BFS from `escort.c`.
- Preserved shared semantic frontier selection followed by unchanged classic path construction, polishing, return-to-player, and scram behavior.
- Removed the obsolete Android nearest-point preference and JNI bridge.
- Replaced Guide-Bot metadata accessibility with `route_planner_segment_reachable_view`, backed by the shared C++ planner.
- Removed the route-edge shadow API and migrated route snapshot tests to direct shared-planner assertions.
- Windows D2 builds successfully. All 23 registered D2 native tests pass; direct `test_route_snapshot` and `test_level_metadata_scan` runs pass.
- Scoped C/C++, Kotlin, PowerShell, and repository hygiene checks pass after formatting.
- Planner-shadow introspection and headless reporting are temporarily compile-disabled and still need physical deletion together with `route_planner_compare_view` and the old planner in `level_metadata_scan.c`.

## Removal Checkpoint 2026-07-14

- Physically removed `route_planner_compare_view`, its C ABI summary, introspection serialization, headless strict-shadow handling, and comparator-only helpers.
- Reduced `level_metadata_scan.c` to non-route mine statistics plus the shared-planner projection wrapper; the old C semantic planner, visibility sampler, opener policy, and route scratch state are gone.
- Removed old metadata route/shadow declarations and migrated endpoint tests to `route_planner_plan_view`.
- Confirmed the remaining edge cost constants and route target bound are live shared-planner data, not migration-only APIs.
- Native builds, route tests, corpus checks, and live fixtures remain to be rerun after this physical deletion.
