# Guidebot Mid-Level Trigger Routing Plan

## Goal
Make guidebot `NEXT` reliable when the guidebot is released after the player has already advanced partway through a complex trigger-gated level. The guidebot should route to the next necessary route step based on the current level state, not the original start-of-level route state.

## Current Understanding
- The route chain is generated once by level metadata as a static success path.
- Android guidebot `NEXT` calls `escort_resume_default_goal()`, which recomputes the route goal from current player keys and current wall/trigger state.
- Current trigger steps are treated as satisfied when their trigger is disabled or all linked walls are passable.
- Current hidden-door steps are treated as satisfied when their linked walls are passable.
- Current trigger guidance can retarget a shootable trigger to a nearby segment with line of sight to the trigger wall.
- KCXF2 level 2 route chain is: `start -> blue key -> trigger 4 -> trigger 7 -> trigger 8 -> trigger 13 -> trigger 19 -> trigger 18 -> trigger 17 -> exit`.

## Design Target
- A late guidebot release after triggers 4/7/8 have opened paths should make `NEXT` target trigger 13.
- A later release after trigger 13 should target trigger 19.
- A later release after triggers 19/18/17 should target the exit.
- The guidebot should prefer the next unsatisfied route objective, but if that exact target is unreachable, it should route to the nearest useful progress point or firing position.
- The metadata route should remain the source of route order, while runtime state decides which steps are already satisfied.

## Phase 1: Runtime Route State Introspection
- [x] Add a small runtime route-state serializer for each metadata route step.
- [x] Include `index`, `kind`, `label`, `satisfied`, `satisfied_reason`, `reachable`, and `selected_next` fields.
- [x] For trigger steps, expose `trigger_flags`, `trigger_disabled`, `linked_walls_passable`, and per-link passability.
- [x] For hidden-door steps, expose per-link passability.
- [x] For key steps, expose whether the player has the required key and whether the key object still exists.
- [x] Add this under existing introspection as `guidebot.route_analysis`.

## Phase 2: Route Step Satisfaction Helper
- [x] Refactor the current satisfaction checks in `d2/main/escort.c` into one helper that returns both boolean state and a reason enum/string.
- [x] Keep the helper Android-scoped if it only supports Android guidebot route metadata.
- [x] Treat a trigger as satisfied when any of these are true:
  - the trigger is one-shot and disabled,
  - all route-recorded opened links are passable.
- [x] Deliberately do not treat "later route step is reachable" as satisfaction yet. KCXF2 level 2 showed this can skip a still-required shootable trigger once the guidebot can reach a firing position.
- [x] Keep close-wall/open-door edge cases conservative: a reclosed door is not considered satisfied unless the route links are currently passable.
- [ ] Add explicit open-wall animation diagnostics if a future test catches a just-fired trigger still being selected for a few frames.

## Phase 3: Current-Position Reachability
- [x] Add a route reachability check from the current guidebot segment when available, otherwise the player segment.
- [x] For each unsatisfied route step, classify:
  - exact objective reachable,
  - firing position reachable,
  - unreachable.
- [ ] Add a separate nearest-progress-point classification if an exact objective or firing position is unavailable.
- [x] Use the same pathing primitives already used by guidebot path creation.
- [x] Avoid full metadata rescan during live gameplay unless the static route chain is clearly insufficient.

## Phase 4: Next Goal Selection Rules
- [x] Update route next-goal selection to pick the first unsatisfied route step that is currently actionable from the player or guidebot region.
- [ ] Revisit behind-the-player skipping only with stronger proof than generic later reachability.
- [x] If a trigger source is shootable, set guidance to `reach_firing_position`.
- [x] If a trigger source is walk-through/contact-only, set guidance to `reach_objective`.
- [ ] If the exact target is unreachable, set guidance to `nearest_progress_point` and keep the original objective metadata visible.
- [x] When no route step remains before reactor/boss/exit, fall back to the existing guidebot default behavior.
- [x] Make Android `NEXT` immediately start the computed route goal so the live guidebot path follows the selected route step.

## Phase 5: Automation Support
- [x] Add a debug/automation action to fire a specific trigger by number.
- [x] Use `check_trigger_sub()` rather than directly mutating walls.
- [x] Reuse the existing debug `player_keys` action for route tests.
- [ ] Add a debug action to move player/guidebot to a segment by level metadata route step when deterministic staging is needed.
- [x] Keep these in debug automation, not normal UI.

## Phase 6: Regression Tests
- [x] Expand `test_kcxf2_guidebot_route_next.json5` into staged assertions for the first two important states:
  - fresh level plus guidebot `NEXT` should target the first needed route objective.
  - after blue key and triggers 4/7/8, guidebot `NEXT` should target trigger 13.
- [x] Continue expanding staged assertions:
  - after trigger 13, guidebot `NEXT` should target trigger 19.
  - after triggers 19/18, guidebot `NEXT` should target trigger 17.
  - after trigger 17, guidebot `NEXT` should target exit.
- [x] Add one test for a shootable trigger where the guidebot routes to a firing position rather than the trigger wall segment.
- [x] Add one test for a trigger whose opened wall is already passable before the guidebot is released.
- [ ] Add one test for an immediate-post-trigger animation delay so `NEXT` does not regress to the just-fired trigger.
- [x] Use introspection fields for assertions, not screenshots.

## Phase 7: Obsidian Study Pass
- [x] Identify Obsidian levels with placed guidebot but inaccessible from start: levels 4, 7, and 10.
- [x] Inspect route steps and trigger dependencies for the placed-guidebot candidates.
- [x] Pick Obsidian level 7, `Beryllium`, for a staged guidebot route test.
- [x] Record route metadata gaps for follow-up: level 10 is still partial with `route target unreachable`; level 13 still fails with a trigger dependency loop.

## Phase 8: Metadata Feedback
- [x] If runtime analysis finds static route-chain gaps, add scanner metadata/scanner ordering fixes for those cases instead of special-casing the guidebot.
- [ ] Consider adding route step `activation_kind`: `touch`, `shoot`, `hidden_door`, `key`, `boss`, `reactor`, `exit`.
- [ ] Consider adding route step `optional_if_reachable_after` for trigger steps that become obsolete once later geometry is already open.
- [ ] Regenerate focused mission JSON after scanner changes: KCXF2, Obsidian, Counterstrike, and Descent.

## Verification
- [x] Run scoped `android/run-code-quality.ps1 -Fix` for touched files.
- [x] Rebuild and run `test_level_metadata_scan` for D2 and D1 if shared scanner files change.
- [x] Run `:app:assembleDebug` after C/JNI/Kotlin/introspection changes.
- [x] Run expanded `test_kcxf2_guidebot_route_next.json5`.
- [x] Run the selected Obsidian staged guidebot route test.
- [x] Confirm `git diff --check`.

## Risks
- Trigger animation timing can make a correct route look stale for a few frames.
- Triggered open doors can reclose, so historical trigger activation is not always equivalent to current route progress.
- Some levels may allow alternate progression that makes an earlier route trigger unnecessary even though its linked walls remain closed.
- A full dynamic route rescan during gameplay could be expensive and should be avoided until the cheaper satisfaction/reachability model is proven insufficient.
