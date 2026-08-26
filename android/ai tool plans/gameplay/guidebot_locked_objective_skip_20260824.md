# GuideBot locked objective skip investigation

## Goal

Determine why GuideBot selects later objective 7 while the automap still identifies earlier objective 4 behind the yellow door, and correct the live-plan selection without changing the valid precomputed order.

## Constraints

- Preserve original GuideBot movement, visit-player cadence, and route character.
- Do not treat temporary inaccessibility through a locked door as objective completion.
- Prefer current level state over stale route or save-derived history when deciding whether an objective remains valid.
- Keep Android GuideBot diagnostics available through launcher-exportable logs.

## Plan

- [x] Correlate objective 7 selection and rejection events in the supplied log.
- [x] Reconcile automap objective numbering with canonical and live route indices.
- [x] Prove whether live certification skips an earlier required but unreachable step.
- [x] Add a focused regression test for ordered selection with a locked frontier.
- [x] Implement the narrowest correction in shared Android route code.
- [x] Run focused tests, scoped code quality, and required host and Android builds.

## Findings

- The supplied log repeatedly records GuideBot `route_target=217`, which is canonical route step 7 (`Shoot switch trigger 6`).
- The automap still presents an earlier switch as the next objective. The prior conclusion incorrectly assumed that segment 217 was that same automap marker.
- The live certifier scans every still-required prepared step and currently overwrites its selection for each reachable candidate. This can select a later reachable action after an earlier required action is rejected as unreachable.
- The certifier now stops at the first usable action still required by current world state. An invalid or unreachable prepared target rejects canonical reuse and invokes the full live planner instead of allowing a later action to replace it.
- Completed actions and disabled triggers remain skippable, preserving current-state recovery without depending on saved completion history.
- GuideBot logs now include canonical and selected step indices plus certifier reachability counts in launcher-exportable GuideBot logs.

## Validation

- Scoped code quality passed for all changed source, test, and plan files.
- D2 Windows game, headless, metadata, and test targets built successfully.
- All 43 D2 host tests passed, including the expanded GuideBot certifier regression.
- Android `:app:assembleDebug` passed for `arm64-v8a`, `armeabi-v7a`, and `x86_64`.

## Unchanged behavior follow-up

- [x] Correlate the new `route_certifier` diagnostics with GuideBot goal adoption.
- [x] Identify how objective 4 is classified by current-world-state checks.
- [x] Trace any full-planner fallback that still produces objective 7.
- [x] Add only the diagnostics needed to distinguish completion, usability, target, and reachability decisions.
- [x] Implement and validate the correction supported by the new evidence.

### Follow-up findings

- The 16:14 log proves the ordering correction is running: canonical reuse selects no later step after rejecting its first still-required candidate.
- After that rejection, the full live planner runs independently and produces segment 217 as its first pending target. The previous certifier behavior was therefore real but was not the active cause of the reported objective skip.
- The old diagnostic did not identify the rejected canonical step or expose which route steps the current-world checks considered required, so it could not distinguish objective 4 being treated as complete from objective 4 being required but unreachable.
- The new launcher-exportable GuideBot diagnostics record the required-step bit mask, exact blocking step and segment, rejection reason, current start segment and key mask, and the full planner's selected route-step metadata.
- At that diagnostic stage, no further gameplay behavior was changed. The requested same-save log supplied the evidence needed for the correction.
- The 16:41 log reports `required=0x780`, proving current-world inference considers only steps 7 through 10 unfinished. Objective 4 is skipped because its linked-wall effects appear passable, not because its target is rejected.
- The same log reports canonical step 7 at segment 217 as unreachable. The full planner nevertheless publishes that step as a partial objective, reproducing the route toward the impassable frontier.
- A provisional certifier fallback retained the first reachable unresolved canonical switch when a later required objective was unreachable. The 17:27 log disproved that fallback and it has been removed.

### Follow-up validation

- D2 Windows game, headless, metadata, and test targets built successfully.
- All 43 D2 host tests passed, including assertions for the new blocking-step diagnostics.
- Android `:app:assembleDebug` passed for all configured ABIs.
- `git diff --check` passed.
- The new unresolved-switch fallback regression passed, as did all 43 D2 host tests.
- Android `:app:assembleDebug` passed again after the supported gameplay correction.

## Passability root-cause follow-up

- [x] Trace the linked-wall completion predicate against GuideBot movement and full-planner edge predicates.
- [x] Inspect Aquarius walls 22 and 44 and the route toward segment 217 using available level data or exact runtime diagnostics.
- [x] Determine whether shoot-through grates are being confused with object-passable sides at any shared boundary.
- [x] Replace or revise the provisional unresolved-switch fallback if the shared passability cause explains both symptoms.
- [x] Add a focused regression and run scoped quality, host tests, and Android assembly for any correction.

### Passability findings

- Objective completion, route snapshots, route certification, and GuideBot route path creation all use object flyability, represented by `WID_FLY_FLAG`, at their engine boundary. A solid transparent grate reports render-through without `WID_FLY_FLAG`, so these checks do not directly mistake an ordinary shoot-through grate for an object-passable wall.
- The 16:41 log proves segment 217 is outside the certifier's 320-segment reachable set. The full planner nevertheless publishes segment 217 as a partial objective.
- The original 1996 pathfinder deliberately returns a path to the last visited reachable segment when it cannot reach the requested goal. A grate on that frontier therefore becomes the visible stopping point even though the pathfinder did not traverse or classify it as passable.
- The prior Obsidian level 6 grate regression involved physical movement failing at graph edges after path creation and is handled by stalled-edge exclusion. It is a different lower-level geometry or clearance case from the unreachable partial objective shown here.
- The supplied log only records the aggregate result for objective 4. It cannot show whether wall 22 or wall 44 satisfied completion through `WALL_OPEN`, `WALL_DOOR_OPENED`, or current side flyability.
- Launcher-exportable GuideBot logs now report the locator and each effect wall for skipped unresolved objectives, including wall type, state, flags, doorway bits, flyability, opening state, and the final completion-passable result. This will identify the exact erroneous or legitimate condition in the same save.
- The 17:27 log resolves the aggregate ambiguity. Objective 4's locator wall 18 and effect walls 22 and 44 are all genuinely `WALL_OPEN`, with `WID_NO_WALL` doorway flags. They did not merely appear passable to the metadata code.
- Wall 18 is also the declared effect of later canonical trigger 9. A cached unresolved objective can therefore retain a locator whose wall was legitimately removed by another action.
- Objective 4's trigger has no disabled or one-shot state that can provide separate durable completion evidence. Its open effect walls are the only current-state evidence, and that evidence says the action is already satisfied.
- The provisional fallback consequently sent GuideBot to stale segment 172 and displayed its synthetic `Locate and activate switch trigger 0` label. It did not find a missing prerequisite.
- After the energy-center command, the certifier's reachable count changed from 320 to 470 while its start segment remained 265. A live wall transition made real objective 7 reachable; GuideBot did not discover a different static path by moving between connectivity components.
- The later generic instruction belongs to real objective 7, a known shootable switch at segment 217. The inconsistent wording came from switching between two different objective records, not from nondeterministic message selection.
- Repeated unresolved completion details are now logged only when their wall evidence changes. The diagnostic also lists every source wall attached to the unresolved trigger, including texture and shootability data.
- Future logs report the exact wall type, state, flags, key, trigger, and controlling trigger whenever a connectivity change alters the certifier's reachable segment count.
- A host scan of pristine Aquarius confirms trigger 0 is attached to closed wall 18 at segment 63 side 0. Its animated texture 418 has effect 74 and replacement bitmap 417, so it is a real shootable wall switch that happens to resemble a reactor surface.
- The actual level reactor is object 6 in segment 47. Objective 4 is not a reactor or pseudo-reactor object.

### Final validation

- The D2 Windows game, headless analyzers, metadata analyzer, and affected test targets built successfully.
- All 43 D2 host tests passed.
- Android `:app:assembleDebug` passed for all configured ABIs after compiling the shared diagnostics for both D1 and D2.
- Scoped code quality and `git diff --check` passed.

### Passability diagnostic validation

- Scoped code quality passed.
- The D2 Windows build and all 43 D2 host tests passed.
- Android `:app:assembleDebug` passed for all configured ABIs.

## Keyed-door route stability follow-up

- [x] Correlate the pseudo-reactor sequence and objective transitions in the 20:49 log.
- [x] Identify the world-state transition responsible for the 320-to-470 reachable-segment changes.
- [x] Separate strategic objective reachability from GuideBot's immediate physical door passability.
- [x] Add focused regression coverage for a closed keyed buddy-proof door.
- [x] Run scoped code quality, host tests, and Android assembly.

### Keyed-door findings

- The run completes the first logged pseudo-reactor action at route step 3, then advances through the paired switches at segment 302 and the switch at segment 217 before selecting the real reactor in segment 47.
- The saved state already contains the destroyed replacement texture for route step 4's source wall and open linked walls, so skipping that step in this run is supported by current level state rather than stale completion history.
- Reachability repeatedly changes from 470 segments to 320 segments when keyed buddy-proof walls 41 and 42 close, then returns to 470 when the player causes the door to open.
- The certifier currently uses GuideBot's immediate physical passability for strategic objective reachability. A closed buddy-proof door therefore invalidates an otherwise valid route even when the player owns its key and can reopen it.
- The correction must keep the original physical path behavior at the door while preventing this transient door state from resetting the selected objective.
- Route certification and navigation-access auditing now treat an unlocked, visible keyed door as strategically reachable only when the player currently owns its key, even if the buddy-proof rule prevents GuideBot from opening the closed door himself.
- GuideBot path creation continues to use immediate physical passability, so he still approaches and waits at the closed door until the player opens it instead of flying through or forgetting the selected objective.

### Keyed-door validation

- The focused regression proves the same closed keyed buddy-proof door is physically blocked but strategically reachable with the matching key, and strategically unreachable without it.
- The D2 Windows game and headless targets built successfully.
- All 43 D2 host tests passed.
- Android `:app:assembleDebug` passed after compiling all configured ABIs.
- Scoped code quality and `git diff --check` passed.

## Route-aware physical frontier follow-up

- [x] Define a route-aware frontier that is physically reachable by GuideBot and minimizes the remaining strategic route distance to the selected objective.
- [x] Use that frontier for physical path creation without replacing or clearing the selected objective.
- [x] Preserve original visit-player cadence and ordinary non-route GuideBot path behavior.
- [x] Add focused tests for useful-frontier selection, keyed-door waiting, and genuinely unreachable objectives.
- [x] Run scoped code quality, D2 host build and tests, and Android assembly.

### Frontier design

- The original partial-path fallback selects the last breadth-first-search segment enqueued. That endpoint is traversal-order-dependent and can be an irrelevant side branch.
- A useful frontier must be selected by graph progress toward the objective, not by geometric proximity.
- The route objective remains active while GuideBot waits at a frontier. Temporary physical obstruction must not switch him to scram behavior or erase the route destination.

### Frontier implementation

- Reverse strategic search assigns every segment its remaining route distance to the objective. A forward physical search then chooses the reachable segment with the smallest remaining strategic distance, using physical distance as the stable tie-break through breadth-first visitation order.
- The physical search respects GuideBot clearance, current wall passability, the path-length limit, and both stalled edges already recorded by collision recovery.
- If an alternate route avoids a stalled edge, the objective itself remains the physical target. Otherwise GuideBot targets the best useful segment before the obstruction.
- A path that reaches this deliberate frontier is treated as successful physical progress. The semantic objective remains active and is retried after ordinary path refreshes or visit-player behavior.
- Manual non-route GuideBot commands and the original `time_to_visit_player` function are unchanged.
- Launcher-exportable GuideBot logs report `route_frontier` with the objective, selected frontier, and avoided edges whenever the physical and semantic targets differ.

### Frontier validation

- The focused graph regression includes a tempting passable side branch. It selects the segment immediately before a closed keyed buddy-proof door rather than the side branch.
- The same regression reaches the objective when the door is open, stops before a recorded stalled edge when no alternate route exists, and returns no frontier when the objective is strategically unreachable without the key.
- The D2 Windows game and headless targets built successfully.
- All 43 D2 host tests passed.
- Android `:app:assembleDebug` passed after compiling all configured ABIs.
- Scoped code quality and `git diff --check` passed.

## Full-suite failure follow-up

- [x] Inspect artifacts for all five failures in report `report_20260824_230254.md`.
- [x] Classify GuideBot failures as behavior regressions, intended expectation changes, or test-instrumentation mismatches.
- [x] Diagnose the unrelated JSONC parser and level-complete failures independently.
- [x] Implement narrow fixes where the evidence identifies a product or test defect.
- [x] Re-run affected tests plus proportionate host and Android validation.

### Full-suite findings

- The strict JSON parser reflected a one-argument `JsonDocument.Parse` method that is absent in the current .NET runtime. Direct static invocation preserves strict parsing and works across the supported overload shapes.
- Counterstrike level 20 metadata was regenerated with trigger 31, not trigger 18, as the action after the blue key. Both integration fixtures still encoded the superseded route and now follow trigger 31 into the gold-key objective.
- The level 20 certifier fixture's shadow comparison now records a target-ranking semantic mismatch instead of the old reversible-wall proof mismatch. The maintained fixture asserts that current classification and its replay artifact.
- The physical-frontier change intentionally makes both the active and guidance route segments 47 in the level 2 switch case while the semantic objective remains trigger 21 in segment 24.
- KCXF2 path parity differs in generated path points and may consume a different number of temporary RNG calls. This is expected because route paths now use stricter physical passability than the ordinary pathfinder. The comparison restores the live AI and RNG state before returning.
- The parity debug action now records results without aborting before the script can assert and report the individual comparison fields.
- The extended KCXF2 run exposed an auto-closing hidden door being selected again after the player and GuideBot had advanced to the boss side. The certifier now ignores a reclosed hidden door only when a later required objective is already reachable from the current start, preserving the door objective while it still blocks forward progress.

### Full-suite validation

- Strict JSONC and tracklist parsing passed with seven current tracklists.
- The Counterstrike dropped-notification fixture passed all 30 steps.
- The Counterstrike trigger fixture passed all 40 steps, including movement away from the formerly stalled segment.
- The level-complete touch-skip fixture passed all 88 steps.
- The rebuilt KCXF2 fixture passed all 183 steps, including the reclosed-hidden-door transition and later objective checks.
- The focused GuideBot route certifier host test passed, including the new reclosed-hidden-door regression.
- The D2 Windows game, headless analyzers, metadata analyzer, and test targets built successfully.
- Android `:app:assembleDebug` passed for all configured ABIs after final formatting.
- Scoped code quality passed.
## D2 level 21 post-reactor grate follow-up (2026-08-25)

- [x] Locate and parse the supplied exported GuideBot log.
- [x] Correlate the post-reactor exit objective with route certification and frontier selection.
- [x] Trace how the shoot-through grate and its linked switch are represented in level metadata and runtime passability.
- [x] Record the root cause and the narrowest behavior-preserving fix direction.

### Results

- The level metadata is correct: step 5 is shoot-switch trigger 16 in segment
  271 (wall 148), which opens the closed transparent wall pair at segments
  339/302 (walls 121/120); step 6 is the exit in segment 297.
- At 22:02:54.187 the player selects the legacy Exit command. The command
  clears the semantic route and sets `Escort_special_goal` to
  `ESCORT_GOAL_EXIT` (`special=5`, `route_active=0`).
- The legacy path then targets exit segment 284 directly. Its non-route path
  search calls `ai_door_is_openable`, which treats a closed wall with a
  `controlling_trigger` as openable while GuideBot is going to an objective.
  Wall 121 is closed and not flyable (`doorway=0x6`) but has controlling
  trigger 16, so the legacy search plans through the impassable grate.
- The live route certifier independently selects step 5 / segment 271 at
  22:03:08, proving that the planner recognizes the switch prerequisite, but
  the explicit special goal prevents that certified route from being adopted.
- When trigger 16 is finally shot at 22:05:14, wall 121 changes from closed to
  illusion-off and the certifier immediately advances from step 5 to the exit
  step 6. This confirms the switch/grate dependency from runtime state rather
  than only static metadata.
- The narrow behavior-preserving fix is to let the Android Exit command use
  the certified end-of-level route when available, including its physical
  frontier, while retaining the original direct Exit behavior as the fallback
  when no usable metadata route exists. The general legacy door-openability
  rule should not be globally changed as part of this fix.

### Implementation

- [x] Add a shared route-layer operation that adopts the next certified
  end-of-level objective for an explicit Exit request.
- [x] Add the smallest Android-only hook in the original escort command code,
  retaining the legacy Exit goal as fallback.
- [x] Add regression coverage for prerequisite adoption and fallback behavior.
- [x] Run scoped formatting, tests, and the required Windows CMake build.

### Implementation results

- Explicit Exit now adopts a ready, validity-certified semantic route with a
  pending objective. The special legacy Exit goal remains the fallback for
  calculating, failed, invalid, complete, or absent route decisions.
- The adopted route retains its activation guidance and physical-frontier
  calculation, so Counterstrike level 21 selects trigger 16 before the exit.
- The exported GuideBot log records `exit_command route_adopted` with the goal,
  target segment, objective kind, trigger, and wall.
- Scoped code quality passed. The Windows D2 build passed, all 43 configured D2
  CTest tests passed, and Android `assembleDebug` passed for arm64-v8a,
  armeabi-v7a, and x86_64.
