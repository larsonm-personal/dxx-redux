# Guidebot pathing modernization remediation

## Goal
Unify guidebot ownership, live route analysis, and unexplored-area navigation so
the guidebot executes the same path assumptions reported by metadata, survives
save and multiplayer ownership transitions, and does not get trapped on stale or
unreachable goals.

## Consolidated review findings

### 1. Multiplayer authority is split across incompatible state
- [x] Make nonowners skip all companion simulation, including path following,
  danger avoidance, flare creation, and simulation RNG use.
- [x] Apply ownership through one helper that updates `Escort_owner_player`,
  `REMOTE_OWNER`, `REMOTE_SLOT_NUM`, and `robot_controlled[]` on every peer.
- [x] Clear the old owner's control slot and reserve a valid slot for the new
  owner during initial claim, adoption, abdication, and packet receipt.
- [x] Preserve durable route intent across handoff, but clear transient path and
  route-step state and replan from the new owner's current state.
- [x] Send the new guidebot first toward the new owner before resuming the route.
- [x] Move disconnect adoption out of the generic player-ghost path so ordinary
  death does not transfer ownership.
- [x] Exclude host observers, disconnected slots, and other ineligible players
  from ownership candidates.
- [x] Make the current multiplayer master choose adoption and validate connected
  owners, request authority, generations, and stale/out-of-order updates.
- [ ] Bind the owner packet's claimed sender byte to transport-level peer
  identity; the legacy `multi_do_data` dispatch currently exposes only payload.
- [x] Resolve passive initial ownership through the same authority instead of allowing
  whichever client first notices the opened cage to claim it.
- [x] Map saved guidebot ownership through stable coop player identity instead of
  restoring a raw historical player slot.
- [x] Add an explicit unowned state broadcast when no eligible owner remains.
- [x] Send current owner generation and route intent through the late-join extras
  stream without disturbing already-correct control slots on existing peers.

### 2. Goal and route intent are not durable or synchronized
- [x] Persist the route target mode (`end_of_level` or `unexplored`) in normal and
  coop save state.
- [x] Include route target mode in ownership synchronization while keeping path
  buffers and selected transient waypoints local to the active owner.
- [x] On restore or handoff, invalidate stale object indexes, metadata steps,
  nearest-progress targets, and path buffers before replanning.
- [x] Define deterministic input-demo behavior for nearest-progress fallback;
  recording and replay must execute the same simulation path.
- [x] Add `Unexplored` to every default guidebot command surface, including the
  radial/menu configuration that currently exposes only older goals.

### 3. Metadata analysis does not model the live game state
- [x] Pass the current key mask into live metadata scans. Key objects can be gone
  after pickup, so object presence alone cannot reconstruct current progress.
- [x] Pass current trigger, wall, reactor, and boss state into route analysis
  rather than treating the level as an untouched static mine.
- [x] Include control-center trigger links and the missing unlock, illusion, and
  Descent 1 trigger mappings in the shared route model.
- [x] Use the guidebot owner or an explicit static level start as appropriate;
  never select the first `OBJ_PLAYER` or `OBJ_GHOST` implicitly in coop.
- [x] Make coop key semantics consistent. Route selection currently treats keys
  as team-wide while executable door traversal uses the owner's keys.
- [x] Preserve route status (`ok`, `partial`, or `failed`) and make the guidebot
  selector respond to partial chains instead of silently consuming their prefix.

### 4. Metadata and executable path rules diverge
- [x] Replace `ai_door_is_openable(ConsoleObject, ...)` in route analysis because
  the console-object shortcut treats every door as openable.
- [ ] Centralize live edge classification so metadata analysis, nearest-progress
  search, unexplored targeting, and `create_path_*` agree about doors, keys,
  hidden walls, triggers, and buddy-proof walls.
- [x] Use computed step reachability when selecting a goal, including selecting
  nearest-progress guidance for a currently unreachable step.
- [x] Preserve the exact trigger firing segment, side, wall, and required opened
  edge produced by metadata instead of reducing it to a generic segment target.
- [x] When a route step cannot be reached directly, promote its first actionable
  blocker rather than falling through to the terminal boss/reactor/exit target.
- [x] Ensure nearest-progress fallback reports and follows the same edge that the
  analyzer identified, including hidden-wall and shoot-switch activation.

### 5. Unexplored routing is still a parallel planner
- [x] Derive the unexplored terminal from the same optimistic route graph and
  blocker chain used by end-of-level routing.
- [x] Do not flood-fill unexplored components through every structural child edge;
  use explicit optimistic edge rules and retain the first obstruction.
- [x] Let blockers outside the precomputed exit chain become intermediate goals
  when they lead to the selected unexplored component.
- [x] Select the largest progress-reachable unexplored component, then use route
  cost, obstruction cost, and stable segment ordering as tie breakers.
- [x] Prevent the four-second return-to-player behavior from repeatedly replacing
  a valid long-running route goal without evidence that the guidebot is stuck.
- [x] Recompute the unexplored terminal from the new owner's local automap after
  handoff while preserving the shared intent to seek unexplored space.

### 6. Performance, diagnostics, and coverage are insufficient
- [x] Stop performing a complete metadata rescan every five-second guidebot goal
  refresh; preserve level metadata and rebuild only the live route fields.
- [x] Extend introspection with owner, owner generation, remote control slot,
  control-slot consistency, target mode, and unexplored target details.
- [x] Add route status, selected blocker, exact activation edge, and path-pending
  state to introspection.
- [x] Add the last replan reason to introspection.
- [ ] Add two-peer tests for initial claim, abdication, disconnect adoption, death
  without adoption, host observer exclusion, and ownership after slot-remapped
  coop restore.
- [ ] Add multiplayer route tests with different key inventories and different
  `Automap_visited` sets.
- [x] Strengthen KCXF2 and unexplored tests to assert the selected action and
  required edge, not only that a path endpoint exists.
- [x] Add fixtures for partial metadata routes, live key masks, already-fired
  triggers, control-center links, hidden walls, and alternate unexplored blockers.
- [x] Add focused fixtures for held live keys and multiplayer owner-request policy.
- [ ] Regenerate checked-in mission metadata after scanner changes and verify both
  host regeneration and the Android import path.

## Implementation phases

### Phase 1: Multiplayer authority and handoff
- [x] Introduce one ownership-application helper for claim, packet receipt,
  abdication, and disconnect, and enforce the same slot invariant after restore.
- [x] Stop nonowner companion AI before any state mutation.
- [x] Separate death/escape/disconnect handling and filter ownership candidates.
- [x] Preserve route intent and force a clean new-owner replan.
- [x] Add focused host-side tests or test hooks for ownership-policy invariants.

### Phase 2: Live route-state contract
- [x] Extend the scan view with explicit start, current keys, trigger state, and
  progression state.
- [ ] Unify edge passability and blocker selection between scanner and guidebot.
- [x] Preserve exact activation geometry and honor partial route status.

### Phase 3: Unexplored integration
- [x] Replace the independent component planner with an alternate terminal on the
  shared route graph.
- [x] Preserve target mode across save, replay, and multiplayer handoff.
- [x] Add blocked-frontier and long-route behavior tests.

### Phase 4: Performance and regression validation
- [ ] Split static topology scanning from live-state evaluation.
- [ ] Expand integration coverage and regenerate mission metadata.
- [x] Run scoped quality checks, host builds, Android tests, and focused emulator
  scripts.

## Current tranche
- [x] Consolidate and de-duplicate the three review passes.
- [x] Implement Phase 1 ownership and AI-authority fixes.
- [x] Add focused regression coverage for Phase 1.
- [x] Run Phase 1 validation and record results here.
- [x] Model live keys, walls, triggers, reactor state, control-center links, and
  explicit guidebot starts in the shared scanner.
- [x] Promote partial-route blockers and retain exact activation geometry through
  guidebot selection and nearest-progress fallback.
- [x] Route the unexplored terminal through the shared dependency chain while
  preserving its durable target mode across save, replay, and handoff.
- [x] Keep active long paths from being replaced by the four-second player-return
  gate, and remove the replay-only nearest-progress behavior fork.
- [x] Replace five-second full metadata rescans with route-only live refreshes.
- [x] Run Phase 2 and current Phase 3 validation and record results here.

## Continuation tranche
- [x] Factor one progression prefix for end-of-level, explicit-segment, and
  unexplored routes.
- [x] Select the largest progress-reachable unexplored component inside the
  shared scanner using its live edge and blocker model.
- [x] Remove the independent unexplored component graph from `escort.c` and
  expose scanner-selected target diagnostics through the engine adapter.
- [x] Make route-start provenance diagnostics stable after the guidebot moves
  away from the segment where its route was scanned.
- [x] Add missing-key, obstruction, and no-unexplored regression fixtures, then
  rerun native, Android, and focused emulator validation.

## Validation, 2026-07-09
- `run-windows-build.ps1 -Target both`: passed before the final D2-only additions.
- `run-windows-build.ps1 -Target d2`: passed after ownership policy, late-join,
  and master-only initial assignment changes.
- D2 native CTest: 14/14 passed, including `test_escort_owner_policy` and
  `test_level_metadata_scan`.
- D1 native CTest: 13/13 passed for the shared scanner changes.
- `gradlew :app:testDebugUnitTest`: 447 completed, 446 passed and 1 skipped.
- `gradlew :app:assembleDebug`: passed for all configured Android ABIs.
- Scoped `run-code-quality.ps1 -Fix` and `git diff --check`: passed.
- Two-emulator ownership/handoff behavior still needs runtime validation; use the
  new `guidebot.owner_*`, `remote_*`, and `local_control_slot_matches` fields.

## Validation, route remediation, 2026-07-09
- `run-windows-build.ps1 -Target both`: passed after the live scanner and
  route-only refresh changes.
- D1 native CTest: 13/13 passed; D2 native CTest: 14/14 passed.
- `gradlew :app:assembleDebug`: passed for all configured Android ABIs.
- Scoped `run-code-quality.ps1 -Fix` and `git diff --check`: passed.
- `test_guidebot_unexplored_goal.json5`: passed with the shared
  `Start -> Reactor -> Unexplored` route.
- `test_kcxf2_guidebot_hidden_door_next.json5`: passed with hidden wall 61 at
  segment 221/side 4 selected, both linked sides blocked, and the route path
  still pending after seven seconds.

## Validation, shared unexplored routing, 2026-07-09
- `run-windows-build.ps1 -Target both`: passed for D1 and D2 after the shared
  progression-prefix and route-provenance changes.
- D1 native CTest: 13/13 passed; D2 native CTest: 14/14 passed.
- `gradlew :app:testDebugUnitTest :app:assembleDebug`: passed for all configured
  Android ABIs.
- Scoped `run-code-quality.ps1 -Fix` and `git diff --check`: passed.
- `test_guidebot_unexplored_goal.json5`: passed with scanner-selected component
  size 193 and the shared `Start -> Reactor -> Unexplored` route.
- `test_kcxf2_guidebot_hidden_door_next.json5`: passed with hidden wall 61 at
  segment 221/side 4 selected and path-pending behavior retained after seven
  seconds.
