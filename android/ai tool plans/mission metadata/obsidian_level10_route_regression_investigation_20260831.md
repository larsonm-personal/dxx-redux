# Obsidian level 10 route regression investigation

- [x] Compare the HEAD, pre-transition-fix, and current route records for Obsidian level 10
- [x] Identify the first changed objective, obstruction, or simulated wall-state transition
- [x] Determine whether the partial result is a genuine prerequisite failure, a strict-planner limitation, or a metadata projection issue
- [x] Check for the same failure signature in related route regressions
- [x] Record the diagnosis and the smallest appropriate next step

## Findings

- The objective sequence did not regress. Both the checked `HEAD` record and
  the regenerated record contain the same unresolved step at index 4:
  `Locate and activate switch trigger 0`, source wall 18, reported from the
  physical frontier at segment 172. The subsequent trigger 7, trigger 9,
  trigger 6, red key, reactor, and exit steps are also unchanged.
- The old `route_status: ok` was a false-positive certificate. Before commit
  `b4793bf3`, the diagnostic continuation was allowed to apply an unresolved
  trigger state and then returned `ok` when that simulated state reached the
  exit. Commit `b4793bf3` added strict completion enumeration and explicitly
  prevents a plan containing `unresolved_trigger` from being published as
  complete. `HEAD` already contains that rule, but the checked Obsidian JSON
  predates a full regeneration with it.
- The regenerated distance increase from 4353.2 to 5091.5 is downstream of
  the same simulated unresolved action. It does not establish a new route
  failure and should not be used to restore the old status.
- A focused headless trace confirms that trigger 0 is an open-wall trigger on
  shootable source wall 18 at segment 63, side 0. It opens wall 22 at segment
  65 and wall 44 at segment 168.
- Wall 18 participates in a stateful switch-surface cycle: trigger 9 opens
  wall 18, and trigger 2 closes/restores wall 18. Trigger 14 is also an
  alternate opener for wall 22. This is not a missing trigger or metadata
  projection problem.
- The strict planner cannot prove a firing route to trigger 0. The diagnostic
  planner therefore inserts the unresolved step and calls
  `route_progress_apply_trigger` to continue exploring the level. Everything
  after step 4 is conditional on that unproved activation, so `partial` is the
  honest status.
- The structural weakness is that dependency planning reacts to the opener
  attached to the first obstruction. It does not perform a bounded search over
  other reachable switch actions, such as an alternate opener or a
  restore/open sequence, when the selected opener's source is itself behind a
  dependency cycle.
- Obsidian level 10 is the only currently partial Obsidian level. Across the
  checked mission metadata, 23 level records currently use the same `switch
  activation route unresolved` classification, so any fix must be general and
  corpus-reviewed rather than special-cased for Aquarius Falls.

## Smallest appropriate next step

Add a planner fixture for this topology and extend strict dependency search
with bounded, transactional exploration of relevant reachable trigger actions:

1. When firing an obstruction's opener fails because its source route loops or
   is unreachable, collect triggers that change either the blocked wall or the
   opener source wall, including alternate openers and restore actions.
2. Try each candidate from a copied progress state, recursively require its
   own firing route, and retain only branches that change the failed
   reachability state.
3. Re-run strict firing-path selection for the original opener after each
   state change. Do not admit unresolved actions into a completing branch.
4. Bound and memoize the search by progress-state signature plus target
   trigger to avoid cycling through trigger 0/2/9 or repeatedly firing trigger
   14.
5. Pin the real Aquarius Falls route only after the synthetic fixture proves
   the general behavior, then review all 23 same-signature levels for genuine
   improvements and regressions.

No production fix was made during this diagnostic investigation.

## Implementation

- [x] Correct the route-status boundary with a focused unresolved-trigger fixture
- [x] Preserve strict completing-plan selection ahead of diagnostic continuation
- [x] Prove Obsidian level 10 retains its complete sequence and localized gap
- [x] Review same-signature mission route changes
- [x] Run scoped quality checks, D1/D2 builds, focused native tests, and corpus validation

## Implementation findings

- The proposed proactive trigger search was tested and rejected. It could add
  unrelated reachable switches, but did not prove trigger 0 or complete
  Aquarius. Keeping it would make route output noisier without resolving the
  calculation gap.
- The earlier per-objective status design already supplied the correct
  representation: a diagnostic continuation may be level-complete while its
  isolated trigger objective remains `calculated: false`. The strict
  completing-plan enumeration accidentally converted every such route to
  `partial`, conflating the level summary with live GuideBot reachability.
- Strict fully calculated plans still take precedence. Only a diagnostic plan
  that reaches the end-level endpoint retains `ok`; a diagnostic plan that
  remains incomplete is still `partial` or `failed`.
- The retained diagnostic plan records the keys it uses as the required key
  mask and sole completing key-mask combination instead of publishing zeroed
  key proof fields.
- Aquarius now reports `ok`, an empty route problem, required key mask 7, and
  completing key-mask set 128. Its sequence still includes blue, gold, and red
  keys, trigger 0 remains `calculated: false`, and its downstream trigger,
  reactor, and exit objectives are unchanged.
- The regenerated corpus contains 23 `ok` routes with unresolved objectives
  and seven `partial` routes with unresolved objectives. This restores the 23
  structurally complete cases without upgrading the seven genuinely
  incomplete cases. Castaway level 1 remains `ok`.

## Validation results

- Scoped code quality passed for the shared planner.
- D1 and D2 Windows builds passed.
- All 44 configured D2 host tests passed, including the updated high-level
  unresolved-trigger route fixture.
- The focused real-level Aquarius scan passed with the expected status,
  objective sequence, and key masks.
- Full host regeneration completed 133 sources: 132 passed, one descriptorless
  archive skipped, and zero failed.
