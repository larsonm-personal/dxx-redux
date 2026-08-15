# Guide-Bot key-gated exit routing

Date: 2026-08-14
Status: complete

## Request

Correct Guide-Bot routing on Obsidian level 4, where the intended progression is
yellow key, primary reactor, then blue and red keys plus additional time reactors,
then the exit after the red key opens access to it.

## Plan

- [x] Trace live route-plan construction and identify why the key-gated exit is
  selected before the red key
- [x] Add a general key-gated-exit dependency rule without map-specific data
- [x] Add or extend focused route-planner regression coverage for the progression
- [x] Run scoped formatting, focused tests, and the Windows D1/D2 build

## Constraints

- Preserve desktop and non-Android behavior
- Keep the change minimal and avoid hard-coding the Obsidian mission or level
- Preserve existing handmade comments and unrelated working-tree changes

## Result

- The route planner now acquires the key required by the selected exit wall
  after completing the primary reactor objective and before routing to the exit
- Existing dependency resolution discovers prerequisite keys along the route,
  so Obsidian level 4 now produces yellow key, reactor, blue key, red key, exit
- The paired D1/D2 route snapshot tests pass, the Windows D1/D2 build passes,
  scoped code quality passes, and fresh Obsidian level 4 metadata reports the
  corrected route with status `ok`
