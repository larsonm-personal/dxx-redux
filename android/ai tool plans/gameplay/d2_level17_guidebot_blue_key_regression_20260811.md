# D2 level 17 guidebot blue-key regression

## Plan

- [x] Locate the guidebot goal-selection and pathfinding code responsible for the exit/trigger message
- [x] Trace level 17's blue-key state, blue door, exit, and trigger topology against that code
- [x] Identify the regression and confirm it with focused history and checked-in route metadata
- [x] Report the cause and proposed fix
- [ ] Make key-change completion require the newly acquired key to match the active key objective
- [ ] Add and run a focused regression test for blue pickup while gold is active
- [ ] Run scoped formatting, D2 build verification, and relevant tests

## Notes

- Investigation only. Preserve unrelated working-tree changes.
- Counterstrike level 17's canonical order is blue key carrier, gold key behind
  blue progression, pass-through trigger 22, red key, reactor, exit.
- Destroying the blue-key carrier can advance the active route goal to gold.
  `escort_note_player_key_flags_for_player()` then treats pickup of any changed
  key as matching that active objective. The completion consumer therefore
  memoizes the gold step as complete when the player actually picked up blue.
- Canonical live-route reuse trusts that false completion, skips gold, and
  synthesizes trigger 22's authored segment as a one-segment terminal without
  checking reachability from the Guide-Bot. Classic path creation then cannot
  reach it and emits the reported message.
- The false completion became harmful when commit `0dd11e4d` added canonical
  completion memoization and reuse on 2026-07-16. The unconditional key-event
  match was introduced in `02166ebc` on 2026-07-14.
- An emulator was available but went offline while launching the app, so no
  fresh on-device state was used. The code path, history, generated base-game
  route metadata, and existing open audit finding BR-0335 agree on the cause.
- Proposed fix: only complete a key route objective when the newly acquired key
  matches `Escort_route_goal.objective_key_index`, and make canonical reuse
  validate/recompute a real path from the current Guide-Bot state rather than
  fabricating the pending step's segment as a reachable terminal.
