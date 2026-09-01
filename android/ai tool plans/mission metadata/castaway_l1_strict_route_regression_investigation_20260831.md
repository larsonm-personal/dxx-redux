# Castaway level 1 strict route regression investigation

- [x] Compare the HEAD and worktree route steps, keys, triggers, target, and completion evidence for Castaway level 1
- [x] Identify the first planner state or transition that prevents the old route from receiving a strict completion certificate
- [x] Trace that failure through the host metadata analyzer and shared route-planning implementation
- [x] Check whether the same failure signature explains other `ok -> partial` or `ok -> failed` regressions
- [x] Define the smallest correct planner fix and the regression coverage needed for Castaway levels 1 and 2
- [x] Record findings and recommended implementation phases

## Findings

- HEAD reports an `ok` six-step route from segment 203 through the three keys,
  reactor segment 582, and exit trigger segment 124. The strict worktree route
  is `partial`, ends after the reactor with a trigger 2 locator, and reports
  `route target unreachable`.
- Full pass-through trigger effects are necessary for strict Castaway level 2
  key proof. Reverting to opening-only effects or accepting a radius-zero route
  would hide the state error rather than fix it.
- The first bad state transition is on the selected red-key-to-reactor path.
  The path crosses trigger 20 at segment 127 side 0 into segment 501. Trigger 20
  closes wall 143 between segments 516 and 518, but that wall occurs later in
  the already-selected path to the reactor.
- `accumulate_path()` calls `route_progress_traverse_path()` for the complete
  pre-transition path. The traversal applies trigger 20, then continues over
  the now-invalid suffix without reevaluating it. The caller subsequently sets
  the current segment directly to reactor segment 582. The planner has therefore
  teleported its simulated progress across the wall that trigger 20 just closed.
- From that impossible reactor-side state, neither strict clearance routing nor
  radius-zero routing can return to trigger 2. This is not a transit-certificate
  false negative. The state is topologically sealed on the wrong side of the
  closure.
- The final `Shoot switch trigger 2` item was not reached or fired. On failure,
  `finish_partial()` calls `append_unresolved_obstruction()`, which currently
  appends the switch with the normal actionable activation kind. This makes the
  partial route look more complete than it is.
- The intended state boundary is before wall 143: stop after crossing trigger
  20 into the segment 501 enclosure, apply its close effects, and replan there.
  Trigger 2 opens wall 143 and the other enclosure walls, so the dependency
  planner can then route to the reactor without crossing a wall after it closes.
- Castaway level 1 has a distinctive downgrade signature: it reaches the
  reactor, then emits an unreachable switch locator. Other start-only hard
  failures are not explained by this bug. Castaway level 9 and other routes that
  cross passive close or toggle triggers should be retested after the fix, since
  the same whole-path transition flaw can affect them.

## Recommended implementation phases

1. Make authoritative path accumulation transition-aware. Consume a path only
   through the first pass-through trigger that changes navigation state, update
   the simulated position to the segment just entered, apply the trigger, and
   return control to `move_to_target()` so it searches again. Never certify or
   accumulate a suffix selected under the previous wall state.
2. Preserve the consumed prefix distance and pending path while replanning. Use
   the crossed portal and child segment center as the transition endpoint, and
   guard one-shot and repeating trigger behavior against accidental duplicate
   activation.
3. Add a synthetic route-planner test in which a fly-through close trigger
   invalidates the remaining shortest-path suffix and an alternate switch
   reopens progress. Assert that the simulated route stops at the transition,
   chooses the switch, and never crosses the newly closed edge.
4. Correct unresolved-obstruction projection so an unreachable switch locator
   is emitted as `unresolved_trigger`, not `shoot_switch`. Keep this diagnostic
   correction separate from completion logic.
5. Extend `test_castaway_level2_route.ps1` or add a paired Castaway integration
   test. Require level 1 to be `ok` and reach its exit under authoritative wall
   state, while retaining level 2 `ok`, required key mask 7, completing key set
   128, and its switch sequence.
6. Regenerate the focused Castaway metadata, then run the full mission corpus.
   Audit every `ok -> partial` and `ok -> failed` result involving passive
   close/toggle triggers before accepting the new routing output.

## Implementation result

- [x] Authoritative path consumption now stops after a pass-through navigation
  transition, preserves the consumed prefix, updates the simulated location,
  and replans from the new wall state.
- [x] End-of-level planning tries the transition-aware model first. If no strict
  completion exists, it may retain a completing legacy plan so this new model
  cannot downgrade established `ok` coverage. Transparent-shot validation does
  not use that fallback, so it cannot replace a valid transition-aware route.
- [x] Unreachable switch locators now use `unresolved_trigger` rather than the
  actionable `shoot_switch` activation kind.
- [x] A synthetic close-trigger test proves that the planner stops before an
  invalid suffix, routes to the reopening switch, and then reaches the exit.
- [x] The paired Castaway integration check requires level 1 trigger 2 before
  reactor and exit, and retains level 2's exact completion sequence and key
  masks.
- [x] Final D1 and D2 builds, both route snapshot binaries, scoped code quality,
  and the paired Castaway test pass.
- [x] The final full corpus completed 133 sources: 132 passed, one archive with
  no mission descriptor was skipped, and zero failed. Against the pre-fix corpus,
  Castaway level 1, `-MOON-` moon12, and Entropy v1 level 2 improve from partial
  to `ok`; no previously `ok` route is downgraded. The temporary D2Crossfire,
  gigalo, and levigen regressions are all restored to `ok`.
