# Guide-Bot routing failure review, September 23

Read-only assessment of the current checked-in level results, historical
investigations and implementation. No new engine simulations were run for this
review. All 2639 stored route-input hashes match current metadata; this checks
input consistency, not equivalence to the latest executable

## Current baseline

- Routing development set: 19 exact manifest paths, 389 levels, 377 OK (96.9%)
  Remaining: 4 failed, 3 timeout, 5 unsupported
- FirstStrike (D1-in-D2), Counterstrike, Vertigo, TEW, Obsidian and Castaway:
  143/144 OK (99.3%). TEW 9 is the only unresolved record in these six missions
- All 140 simulation files: 2176/2639 OK (82.5%), 152 failed, 140 timeout,
  146 unsupported, 24 route_mismatch, 1 infrastructure_error
  These are corpus records, including archive variants/mirrors, not unique maps
- Exact development-set paths matter: recursive basename matching includes a
  second Diehard file and incorrectly inflates the denominator to 408
- Historical continuation notes end at 376/389; current Counterstrike is 30/30
  Its formerly deferred secret -5 is no longer a remaining published failure
- Latest inspected focused suite, android/temp/route_regression_cases/
  run_20260923_101321_706/summary.json, passed 23 selected cases out of 84 available
  It is explicitly non-exhaustive

Sources: android/helpers/routing_development_missions.ps1 and
game_data/mission_files/**/*.simulation.json with matching metadata JSON

## The twelve development-set failures

| Mission/level | Published result | Evidence and likely next work |
| --- | --- | --- |
| Diehard 1 | Failed | Static planner exhausts its work budget after the gold-key carrier; live selector has no remaining action. Profile prerequisite/key-order search and repeated visibility work |
| Diehard 9 | Timeout | After red key, native collision blocks the 314 -> 31 leg. Destination fits, but center and sampled connecting legs do not. Inspect portal topology and connected full-radius free space |
| Diehard 10 | Timeout | Planned approach enters a two-unit-deep switch recess with a roughly 4.7-unit-radius ship. A broader firing search passed this obstruction experimentally but then trapped the actor elsewhere and regressed other missions. Develop occupancy-aware outside firing poses |
| FFYL 4 | Failed | After blue key, modeled gold/red prerequisites cycle. Inspect switch 13's reachable outside firing positions and actual projectile visibility, preserving key restrictions |
| TEW 9 | Timeout | Gold-key approach fails through segments 256/257; clearance masks, centers and native path generation disagree. Prior full-radius samples did not establish a connected route. Previously explicitly deferred |
| Lostlvls secret -1 | Failed | Published planner budget failure also hides a multi-visit inventory/world-history requirement. Verify actual native entries, returns, carried keys and persisted walls/triggers |
| EAF 5 | Failed | Authored level has no exit/reactor/triggers in the prior investigation; content applicability issue |
| Bitesize -3 | Unsupported | Guided-missile equipment and physical flight verification missing |
| EAF 2 | Unsupported | Same guided verification limitation |
| Lostlvls 21 | Unsupported | Same classification, but authored documentation also describes Phoenix rebound shots; recheck the actual required action |
| Plutonia 4 | Unsupported | Guided verification limitation |
| Plutonia 22 | Unsupported | Guided verification limitation |

Historical native diagnoses are in ../2026-09-09-routing-continuation.md and
../2026-09-08-tew-routing.md. They are investigation leads, not fresh reproductions

## Suggested order of work

1. Offline planning efficiency, starting with Diehard 1
   Across the broad corpus, 31 failed records report shared planning budget
   exhaustion; another 5 report collision-analysis budget exhaustion. Profile
   repeated prerequisite states, key-order trials and visibility searches;
   preserve a complete feasible route before optional optimization. TEW 15 is
   an existing successful example. Keep costly full planning off the live frame
   path. Main implementation: route_planner.cpp and its visibility/snapshot budget

2. Physical passage and reachable firing-position proof
   132 of the 140 timeout records have static route_status=ok. This establishes
   a plan/execution gap, not one common root cause. Use Diehard 10 and FFYL 4
   for outside-shot planning, and Diehard 9 for connected swept-path geometry
   TEW 9 is the eventual six-mission completion target, subject to its prior
   deferral. Preserve full collision radius and native wall/door semantics
   Main implementation: route_planner.cpp firing selection, route_collision.c,
   route_snapshot.cpp, guidebot_path_recovery.c and route_confirmation.cpp
   frontier/interaction handling

3. Audit metadata/live objective disagreement alongside those fixes
   There are 24 route_mismatch records; 14 end with an exit objective. Inspect
   their raw physical results and legal trigger ordering, regenerate metadata
   where justified, and repeat before accepting a pass. These could improve
   recorded agreement without requiring new movement capability; an exit label
   alone is insufficient proof. Main implementation: normalization/projection
   in guidebot_simulation_regression.ps1 and shared route planning

4. Physically verify indirect shots
   Guided requirements account for 32 unsupported records, five in the dev set
   First distinguish ordinary remote shots and Phoenix rebounds from genuine
   guided flight. Genuine guided routes require equipment acquisition, missile
   flight/collision and actual trigger activation evidence. Merely accepting
   the planner's proposed guided path does not verify a completed level
   Main implementation: guided_missile_route.cpp and route_confirmation.cpp

5. Native multi-visit secret scenarios
   Saved-world checkpoint and secret-transition test foundations exist, as does
   the recent co-op world travel work. Lostlvls secret -1 still needs authored
   entry provenance and repeated visits with real inventory/world persistence
   A visit return and a completed mine need distinct outcomes. Do not combine
   keys from separate visits into an invented starting inventory

For each engine candidate: reproduce current failure, require deterministic
physical completion, retain all 377 current development-set passes, then sample
the broad failure family and run appropriate native/live/demo regression gates

## Reporting and live-play limits

- 65 failed corpus records have static "missing exit"; audit content and mission
  intent before treating them as navigation defects
- Other unsupported records comprise 59 unsupported-format, 31 missing-file and
  24 missing-verification-actor records. Keep these visible separately from
  routing progress; a denominator change is not a new physically completed map
- Uneasy4 is the one retained infrastructure error, an explicitly accepted
  planning watchdog timeout. Its expected-timeout policy does not turn it into OK
- Canonical simulations use fixed seed/timestep, 160% test speed and sandboxed
  gameplay. They do not prove ordinary-speed, arbitrary-start or co-op behavior
- The September 16 live-navigation notes retain a Counterstrike 11 reverse
  connector rejoin failure at ReturnTargetSegment 106. The level's canonical
  route passes, so this needs a separate live acceptance test

Practical first batch: Diehard 1 planning profile, Diehard 10/FFYL 4 outside-shot
diagnostics, and a small audit of exit-ending route mismatches. Then tackle the
harder connected-clearance cases with explicit geometry evidence
