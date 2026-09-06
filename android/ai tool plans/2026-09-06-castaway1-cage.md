# Castaway 1 cage

1. Reconcile incidental trigger crossings assumed by the strategic route with the simulation
2. Preserve physical trigger crossing and wall collision requirements
3. Verify level 1 and the four core missions

## Findings and implementation

- The simulator only honored incidental OPEN_WALL crossings while pursuing a
  reactor. Castaway 1 crosses trigger 1 at 203:5 while pursuing the blue key;
  this opens the reactor entry cage (1:2) and removes switch 2's surface (505:1).
  Honor those physical crossings during any objective, retaining normal
  check_trigger activation, type checks, and explicit-objective ownership
- After that correction, full-radius physics still pins GuideBot on the rim of
  adjoining segment 4 while steering from segment 0 toward a waypoint in 1.
  A long waypoint sweep alone does not catch the short-step collision
- General recovery: after a quarter-second of negligible actual movement,
  require a blocked predicted movement, a clear full-radius waypoint leg, an
  adjacent open portal, and a clear full-radius approach to its center. Steer
  toward that center without changing the route cursor, collision radius,
  position, or wall state
- An anticipatory version regressed First Strike 25 and Counterstrike 24.
  Requiring actual stalled movement restores both; keep the core suite as the
  acceptance check, not just Castaway 1

## Verification

- Windows D2 headed and headless binaries build successfully
- Native CTest: 47/47 passed; scoped quality checks passed
- New test: android/tests/test_castaway_level1_cage_route.ps1. Two identical
  physical completions, 3727 frames, approximately 62 seconds; checks ordered
  objectives and the actual incidental crossing of trigger 1
- Four-core 88-level run: no previous ok level becomes unsuccessful.
  New ok results: Counterstrike secret 3, Castaway 3, Obsidian 14
- Final Repeat 2 run completed all 88 levels and refreshed checked-in simulation
  files: android/temp/cage_final_core_20260906. First Strike remains 30/30 ok,
  Counterstrike is 29/30, Castaway 3/10, and Obsidian 13/18
- Castaway 1 and 9 now physically finish but honestly report route_mismatch.
  Castaway 8 changes timeout -> failed: it gets beyond blue to gold, then the
  live planner reports no actionable goal, rather than timing out at a frontier

## Remaining work

Castaway 1's static route omits the explicit fly-through trigger 20 needed to
restore switch 2 after incidental trigger 1. The live planner discovers it and
finishes, but the serialized objective list still disagrees. Do not hide this
with a successful status or relax objective comparison. Reconcile static
route emission with live incidental state in a separate focused fix
