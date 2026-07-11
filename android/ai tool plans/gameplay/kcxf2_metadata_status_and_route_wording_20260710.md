# KCXF2 metadata status and route wording

## Goal
Resolve KCXF2RM level 4's contradictory travel/route status and make switch and
hidden-door route instructions describe the required player action accurately.

## Plan
- [x] Compare level 4 legacy travel traversal with the executable route result
  and identify which status should own the UI summary.
- [x] Trace `activate_switch`, `shoot_switch`, and `open_hidden_door` to their
  actual trigger and wall mechanics.
- [x] Correct the status source and route labels without obscuring distinct
  activation behavior needed by guidebot.
- [x] Add focused native and emulator regression coverage for status precedence and
  wording.
- [x] Regenerate mission metadata and run native, Android, and quality checks.

## Findings
- Level 4's legacy travel traversal reached all four hostages, then failed to
  reach the exit because that traversal does not model the hidden-door route
  action. The executable route planner did model the hidden door and produced a
  complete route through the boss and exit.
- Legacy travel always handles every hostage and the reactor before it attempts
  the exit. A complete executable route can therefore reconcile legacy travel
  only when exactly one target remains: the exit. This does not conceal an
  unreachable hostage or reactor.
- A `shoot_switch` is a destructible trigger source and must be hit by a weapon.
  The former `activate_switch` is a non-shootable wall trigger that fires when
  the player crosses its side; it is now `pass_through_trigger`, displayed as
  `Pass through trigger`.
- A hidden door is a real animated `WALL_DOOR` with the `WCF_HIDDEN` wall-clip
  flag, not a trigger type or a generic destructible wall. It can be opened by
  normal door interaction, so the route action is now `Open hidden door`.
- Current regenerated level 1 has three `shoot_switch` steps and no
  pass-through step. The mixed activate/shoot display was from older metadata.

## Changes
- Reconcile legacy travel's exit-only failure from a complete executable route,
  including the target count, problem, status, and no-reactor note.
- Rename activation kind 4 from `activate_switch` to
  `pass_through_trigger` without changing its numeric value.
- Use `Pass through trigger` and `Open hidden door` consistently in scanner
  metadata, the Android step viewer, guidebot instructions, and automation.
- Regenerate all checked mission metadata so persisted route actions use the
  current vocabulary.

## Validation
- Scoped `run-code-quality.ps1 -Fix`: passed.
- Windows D1 and D2 builds: passed.
- D1 CTest: 13/13 passed. D2 CTest: 14/14 passed.
- Android `:app:testDebugUnitTest :app:assembleDebug` with JDK 21: passed.
- Full host metadata regeneration: 109 passed, 1 archive without a mission
  descriptor skipped, 0 failed.
- Regenerated KCXF2RM level 4: travel 5/5 with no problem, route `ok`, and
  `no reactor, exit exists`.
- Emulator `test_kcxf2_guidebot_hidden_door_next.json5`: 34/34 steps passed on
  the final APK; live route and guidebot text use `Open hidden door`.
