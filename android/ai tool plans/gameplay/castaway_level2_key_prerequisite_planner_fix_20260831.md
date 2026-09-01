# Castaway level 2 prerequisite proof plan

## Goal

Make the shared Guide-Bot planner prove that the blue and gold keys are true
Castaway level 2 prerequisites. A route that merely collects every visible key,
or that reaches the reactor through a zero-radius topology path, is not accepted
as proof.

Physical frontier recovery and the Guide-Bot release wall remain tabled.

## Findings from the rejected prototype

- [x] The checked route can emit the intended blue, gold, red sequence when all
  keyed branches are proactively explored.
- [x] That result is not a prerequisite proof. The implementation asks for all
  placed key colors and prefers the candidate with more key steps.
- [x] Counterfactual key-mask planning exposed the flaw: a red-only mask still
  reports a complete route.
- [x] The false red route crosses this run after trigger 1:
  `229 -> 231 -> 232 -> 239 -> 238 -> 237 -> 236`.
- [x] Every segment in that run fails full-size occupancy. Its recorded
  clearance is `1`, while the Guide-Bot navigator radius is `310325`.
- [x] The current end-level fallback sets the radius to zero, so those collision
  failures become ordinary topology edges and incorrectly prove red-only
  completion.
- [x] Simply banning every failed-center segment is also invalid. The intended
  blue branch contains isolated skewed geometry whose vertex-average center
  fails the same test.
- [x] Sampling alternative segment centers did not distinguish the intended
  blue transit from the false red bypass. Clearance must be certified as a
  path through a segment, not inferred from one occupiable point.

## Required design

### 1. Add a swept-corridor certificate

- [x] Extend the scan/snapshot topology with a directed transit certificate for
  `(segment, entry side, exit side)` at the Guide-Bot radius.
- [x] For a segment whose center is not occupiable, construct deterministic
  inset points for the entry and exit portals and use the engine collision/FVI
  code to sweep a Guide-Bot-sized sphere between them.
- [x] Keep wall semantics separate from geometry: the certificate answers only
  whether the body can fit through the corridor. The existing effective wall,
  key, and trigger state still decides whether each portal is currently open.
- [x] Cache the 6-by-6 directed transit matrix per segment in the route snapshot
  and include it in the snapshot/profile hash.
- [x] Validate the Castaway cases before integrating it into search:
  - the intended transit through the isolated blue-branch skew must certify;
  - at least one directed transit in the seven-segment red bypass must fail.
- [x] Portal-center insets certify the blue transit, so the conditional AI
  safety-path fallback was not needed. No length-of-run or Castaway-specific
  exception was added.

### 2. Make route search entry-aware

- [x] Change the strict semantic search state from only `current segment` to
  `(previous segment/current entry side, current segment)` when a segment lacks
  ordinary center clearance.
- [x] Permit an exit edge from such a segment only when its entry-to-exit
  corridor certificate succeeds.
- [x] Preserve the cheaper segment-only state for ordinary full-clearance
  geometry so normal mission analysis does not multiply every search node.
- [x] Keep radius-zero search only for partial/frontier diagnostics. It must
  never establish `route_status: ok`, key sufficiency, or key necessity.

### 3. Prove prerequisites counterfactually

- [x] Replace the Boolean `probe_required_keys` mode with an explicit allowed
  key mask. `acquire_key`, exit-key handling, and recovery acquisition must all
  reject colors outside that mask.
- [x] Enumerate all subsets of relevant blue/red/gold keys. There are at most
  eight masks.
- [x] For each mask, try every acquisition order for its selected keys before
  the reactor/exit proof. With three colors this is at most six permutations
  and avoids declaring a mask impossible because one fixed order failed.
- [x] Run every trial with authoritative effective wall state and complete
  pass-through trigger transitions.
- [x] Record only trials that reach the reactor and exit through the strict,
  corridor-certified graph.
- [x] Define the universally required mask as the bitwise intersection of all
  completing masks. Choose the cheapest inclusion-minimal completing route for
  guidance, but do not confuse that chosen route with universal necessity.
- [x] If no strict mask completes, return a partial/unresolved result. Never
  fall back to a relaxed complete claim.

### 4. Feed the proof to Guide-Bot objectives

- [x] Store the proved required-key mask in the route result and generated
  metadata.
- [x] Require semantic key steps for every bit in that mask before reactor/exit
  steps are accepted.
- [x] Let the runtime `next` chain consume those steps normally. No Castaway
  level, trigger, wall, segment, or hard-coded key-order special case is added.

## Regression coverage

- [x] Add an entry-aware synthetic transit test and validate the real skewed
  Castaway blue transit through the swept-corridor integration test.
- [x] Add a synthetic collision chain test where topology connects both ends but
  no Guide-Bot-sized swept corridor exists; radius-zero search must not produce
  a complete route.
- [x] Add counterfactual-mask tests for:
  - one universally required key;
  - two alternative sufficient keys, neither universally required;
  - an optional placed key behind a keyed side area;
  - acquisition-order independence.
- [x] Add a real Castaway level 2 assertion that the completing-mask set has no
  member missing blue, gold, or red, and that the selected route remains:
  `blue -> gold -> red -> reactor -> exit` with the required trigger chain.
- [x] Retain the PowerShell generated-metadata assertion as an integration test,
  but do not use it as the only proof of prerequisite inference.
- [x] Run D1/D2 native tests and builds, Android unit/build tasks, and the full
  mission corpus. Audit status changes and analysis time, especially levels
  that currently rely on radius-zero completion.

## Acceptance criteria

- Castaway level 2 reports blue, gold, and red in the universally required mask.
- Every Castaway completion trial that excludes blue fails.
- Every Castaway completion trial that excludes gold fails.
- No `route_status: ok` plan contains an uncertified radius-zero transit.
- Optional keys in other missions are not promoted merely because a keyed door
  and matching pickup exist.
- The checked metadata and runtime objective chain are generated from the same
  proof result.

## Current status

Implementation and validation are complete. Castaway level 2 reports required
key mask `7` and completing-mask set `128`, so only the blue+red+gold subset
completes. The selected guidance order is blue, gold, red, the required switch
chain, reactor, and exit.

The D1 and D2 Windows builds pass, all 44 registered D2 native tests pass, the
Android unit tests and debug APK build pass, and the focused checked-metadata
test passes. The full host corpus analyzed 133 sources in 580.7 seconds: 132
passed, one descriptor-less archive was skipped, and none failed. Compared with
the previously checked metadata, 31 old `ok` results become honest partial
results for unresolved switch activation and five become failed because no
strict key/corridor completion exists. Those corpus artifacts were generated in
scratch with `-NoRegressionCopy`; only Castaway checked metadata was refreshed.
