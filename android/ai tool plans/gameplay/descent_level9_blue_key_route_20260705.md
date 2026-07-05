# Descent Level 9 Blue Key Route Investigation

## Goal

Fix metadata route analysis for Descent level 9, which currently reports `blue key route dependency loop` even though the intended path is the normal blue key, gold key, red key, reactor, exit progression.

## Plan

1. Done: Inspect the generated `Descent.json` entry for level 9 and reproduce the scanner output if needed.
2. Done: Read the route planning code to understand why the route selects the gold key before the blue key.
3. Done: Check whether the issue is objective ordering, path reachability state after entering the gold-key branch, locked-door modeling, hidden-door modeling, or a combination.
4. Done: Patch the scanner so ordinary key progression prefers the intended blue, gold, red chain when those keys exist and are reachable in order.
5. Done: Add or update tests covering a route where a later key is geometrically reachable earlier but the intended success path starts with an earlier key.
6. Done: Regenerate affected metadata and confirm Descent level 9 reports a complete blue, gold, red, reactor, exit route.

## Result

The route chain builder now tries reachable keys in blue, gold, red order before routing to the boss/reactor and exit. Each key attempt is rollback-protected, so an optional or currently unroutable key does not make the route fail early. Descent level 9 now reports `start -> blue key -> gold key -> red key -> reactor -> exit`.
