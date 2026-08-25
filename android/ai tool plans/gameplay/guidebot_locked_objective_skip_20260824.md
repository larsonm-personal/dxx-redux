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
