# Routing improvements through item 3

The user requested items 1-3 of the September 23 conversation: planning
efficiency, reachable firing positions, and connected physical paths through
awkward geometry. This includes revisiting previously deferred TEW 9

## Required outcomes and validation

1. Diagnose and improve offline planning efficiency using Diehard 1
2. Diagnose and improve physically reachable firing positions using Diehard 10
   and FFYL 4
3. Diagnose and improve connected full-radius passage using Diehard 9 and TEW 9

Seek actual deterministic mine completion for these five cases through general
engine fixes. An intermediate objective, budget increase or changed expected
status is not completion. Preserve native collision radius, key permissions,
trigger/wall effects and ordinary live frame budgets

- Refresh the current Windows engine and full 389-level development baseline
  Preserve the exact manifest identities and the current 377 published passes
- Instrument the first failing operation before making a candidate change
  Remove temporary probes and rejected experiments before final acceptance
- Add reusable physical integration coverage with identical repeats and
  metadata agreement for accepted improvements
- Regenerate affected metadata when planning semantics change; verify the
  whole development set and sample the wider affected failure family
- Validate relevant native unit/live tests and unchanged replay expectations
  Build Windows D1/D2 and Android; run scoped formatting and whitespace checks
- Publish verified metadata/simulation records only after comparison against
  the baseline. Keep unrelated worktree changes intact

## Starting state

The review is in routing_failure_review_20260923.md. The current worktree has
unrelated in-progress D1-in-D2/save/replay changes and earlier co-op follow-up
changes. No native build or route-simulation process was active at inspection

## Current evidence

- Windows D2 baseline build passed; fresh full sweep is preserved under
  temp/guidebot-routing-through3/baseline-current with an executable snapshot
  It finds eight previously published D1-in-D2 passes now timing out, plus two
  AF D1 levels hitting the process watchdog. These precede our planner edits
  and require separate tracking/verification; do not claim 377 preserved yet
- Published identity/status/input hashes are saved in published-baseline.json
- Diehard 1 first ordinary attempt uses only 43185 work units before failing
  to find a remote-door firing pose. Expanded fallback exhausts two million
- Candidate: share the visibility sample cache in the alternate door-aim search,
  using a distinct namespace. Work drops to 183431; level still fails after gold
  The fallback repeatedly attempts walls 49 (563:2) and 55 (569:4), from segment
  552 with blue/gold keys. This is not yet a completed-level improvement
- All temporary planning, geometry, dependency and collision probes removed
  Diagnostic artifacts remain under the temp root
- Switch search now preserves outside firing alternatives when the source room
  has no verified firing pose. This gets DH10 past the two-unit recess: trigger
  1 fires from segment 134. It still enters one-way room 414 through trigger 11
  and cannot return to firing segment 391 for switch 10. Do not count a pass
- FFYL 4 was not just missing a firing pose: after switch 13, the shortest
  return path selected trigger 18 behind a gold/red key cycle. The planner
  stopped at that failure even though trigger 12 permits a longer return path
  Treat unreachable-source/key failures as avoidable dependencies, roll back
  their speculative state, and search again under the existing permissions
- FFYL 4 now completes in 3994 frames, twice identically, full radius 310325
  The original partial metadata imposed a 60-second test limit; regenerated
  complete metadata uses the unchanged distance-based limit. Completion takes
  66.57 seconds, with 7.90 seconds of its native 45-second reactor countdown
- Refine recovery samples toward each segment face at 7/8, 15/16, 31/32 and
  63/64. Retain full-radius occupancy and stepped incoming/outgoing sweeps
  TEW 9 now repairs tapered waypoints at 550 and 555 and completes in 7132
  frames, twice identically, full radius 308279. Static metadata already agrees
- DH9 still fails after red at 314 -> 31. The native portal-center sweep hits
  solid side 0 of 314; refined samples did not establish a connected safe path
  No radius reduction, wall mutation or forced movement was introduced
- Cache generation advanced from 42 to 43 in native and Kotlin definitions
- Added reusable TEW and FFYL completion/repeat tests to the route case registry
  Both pass. Live escort navigation passes three cases with two repeats each
- Windows D1/D2 builds pass; native suites pass 52/52 and 60/60. Subsequent
  routing/cache checks pass 6/6 per engine. Scoped formatting/schema checks pass
- Android three-ABI build and 19 scheduling/monitor JVM tests pass. The native
  build was repeated successfully after the final shootable-switch restriction;
  Kotlin did not change subsequently
- The final Android rebuild caught a concurrent addition, input_demo_world_trace.cpp,
  including automap.h before MAX_SEGMENTS is defined. Moved that include after
  segment.h; no tracing behavior changed. The repeated native build passes
  Replay/simulation evidence uses executable snapshots from before this later
  tracing addition, with the final routing source
- The first 389-level candidate sweep preserves all 367 fresh baseline passes,
  adds TEW9, and retains the ten baseline D1-in-D2 failures. DH10's intermediate
  route mismatch is resolved by regenerating its metadata (it remains timeout)
- Current final source has no temporary probes. Final 389-level guarded sweep
  finished: 369 ok, 3 failed, 10 timeout, 5 unsupported, 2 infrastructure errors
  It preserves all 367 fresh baseline passes and adds FFYL4 and TEW9, improving
  the fresh pass rate from 94.34% to 94.86%. The exact 389 identities match
  The two AF D1 watchdog errors reproduce the pre-edit baseline
- Both new integrations pass twice identically on the final source. All 19
  existing input demos pass without modifying their expectations
- The broader budget sample (chron10b3 specimen, D2Crossfire1 crikgul and
  levigen4 lvn-irr) still fails under the shared planning budget. Do not
  extrapolate the DH1 cache improvement to those unresolved failures
- Refreshed FFYL and Diehard metadata are staged in candidate-metadata, with
  their original archives. Guarded regeneration matches byte-for-byte. Changed
  metadata levels: DH1 problem text, DH6/DH10 paths, FFYL4 complete route
  Published FFYL.json and diehard.json plus FFYL, Diehard and TEW simulation
  records after validation. All 38 original input files matched their saved
  hashes before publication; the other 33 files remain unchanged
  Historical published passes become 379/389, but this is not a fresh sweep:
  the ten previously published D1-in-D2 passes now failing remain unchanged
  The authoritative fresh result for this work is 369/389

## Remaining work after this batch

- DH1: establish a physically reachable shot for unlocked reverse door faces
  49/55 after gold. Planning work is reduced by about 91%, but no exit yet
- DH10: prove continuation after crossing trigger 11 into segment 414; the
  current route cannot return to segment 391 to shoot switch 10
- DH9: establish connected full-radius movement from segment 314 to 31; native
  collision still rejects the attempted passage. Do not assume impossibility
- Diagnose the ten pre-existing D1-in-D2 baseline failures separately, and
  continue the broader planning-budget family from its unchanged failures

No level-specific exceptions, reduced collision radii, enlarged analysis
budgets or replay expectation changes were needed for the two new completions
