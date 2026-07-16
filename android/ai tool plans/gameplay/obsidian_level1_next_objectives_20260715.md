# Obsidian Level 1 Next Objective Corrections

## Goal

Make the automap next-objectives list and world markers reflect the actual remaining Obsidian level 1 route after its early objectives have been completed.

## Plan

- [x] Reproduce the bad selection and inspect the route, completion state, and marker geometry.
- [x] Fix the shared objective filtering and marker/connector rules without mission-specific exceptions.
- [x] Extend focused native or emulator regression coverage for reactor and exit behavior.
- [x] Run scoped code quality, relevant tests, and required CMake build/test verification.
- [x] Record results and any remaining limitations.

## Boundaries

- Do not add Obsidian-specific segment, wall, trigger, or objective-number exceptions.
- Preserve completed-objective handling in All and Remaining display modes.
- Keep D1 and D2 behavior aligned through the shared implementation.

## Result

- Opening the Android automap now replans the live route from the local player, replacing stale canonical or Guide-Bot routes before objective text, markers, and connectors are selected.
- Objective numbering now excludes the synthetic Start step. Obsidian level 1 therefore identifies the reactor as objective 6 and the exit as objective 7.
- After both keys and opening triggers are complete, the remaining overlay contains only Reactor and Exit. It has two marker candidates, no merged labels, and no connector candidates. The exit remains at segment 312 side 1.
- Added a 46-step stale-route emulator regression and updated the existing 46-step Obsidian marker regression for the corrected numbering. Both pass.
- Scoped code quality, the Android multi-ABI debug build, both Windows builds, and D1/D2 route snapshot and metadata scan tests pass.
