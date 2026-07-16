# GuideBot Nearest Shoot Guidance Point

## Goals

- Identify the GuideBot destination used for Obsidian level 1 objective 3.
- Route the GuideBot to the nearest valid player firing position for shoot-switch objectives.
- Keep classic GuideBot movement, simulation, firing, flares, and RNG unchanged.

## Plan

- [x] Trace shared route activation positions into GuideBot high-level goals.
- [x] Capture the candidate firing positions and current selected destination for Obsidian level 1 objective 3.
- [x] Select the nearest valid firing position in shared high-level routing without mission-specific exceptions.
- [x] Add focused route and GuideBot integration coverage.
- [x] Run scoped quality, native tests, D1/D2 builds, Android build, metadata regression, and focused emulator validation.

## Boundaries

- Do not alter classic GuideBot path following or movement.
- Do not make the GuideBot shoot, aim, or modify progression state.
- Do not add Obsidian-specific segment, trigger, wall, or coordinate exceptions.

## Result

- The shared planner selects segment 167 at `(73.68013, 35.77408, 297.35968)` as the reachable firing position for Obsidian level 1 objective 3. The switch aim point is `(-171.15210, 38.89893, -205.40771)`.
- GuideBot route goals now retain the planner's exact firing position. After the original path generator and path polisher run unchanged, the final waypoint is set to that validated in-segment position.
- Added introspection for the route target and physical path endpoint positions. The focused 36-step emulator test verifies that both positions and segments match before advancing the objective.
- The change does not alter firing, aiming, progression, flares, RNG calls, path search, path polishing, or path following.
- Scoped code quality, D1/D2 route snapshot and metadata tests, both Windows builds, Android unit tests, Android debug assembly, Obsidian's 18-level metadata scan, and the focused emulator test pass.
