# 2026-08-29 Sample Test Failure Diagnosis

## Scope

Diagnose and fix the four failures in `temp/test_reports/report_20260829_101305.md`.

## Plan

- [x] Read repository instructions and identify the four failing tests
- [x] Inspect complete logs, artifacts, test definitions, and relevant implementation history
- [x] Classify each failure as product regression, test defect, environment issue, or inconclusive
- [x] Record evidence, likely causes, and focused next actions

## Findings

- `test_abort_game_to_main_menu_d2` is a stale test expectation. The game abort,
  main-menu return, autosave kind, slot, and level all passed. The test expects
  `[auto] abort`, but automatic save descriptions intentionally gained level
  suffixes and the launcher correctly returned `[auto] abort L1`.
- `test_obsidian_level1_objective_markers` is a Guide-Bot firing-position
  regression. The same invariant failed on August 26, was fixed in `02390c32`,
  and passed in focused verification. The later firing-position search added in
  `f1ec1f80` can once again select the switch aim point itself as the activation
  position, producing guidance with no distinct firing position. The failure is
  repeatable on August 29 and is not a timeout flake.
- `test_lan` is an emulator shared-Wi-Fi readiness/timing failure. Its 15-second
  EMU2-to-EMU1 ping preflight expired, but the same emulators shortly afterward
  passed ping, UDP unicast, UDP broadcast, subnet broadcast, and launcher LAN
  lobby discovery. The product LAN path was not reached by the failing test.
- `test_mp` is transient test/observability failure. The failing run's server log
  proves the join succeeded and both clients submitted STUN results and completed
  connectivity checks, despite the polling assertion reporting no join. A focused
  rerun passed lobby join, chat, ready state, launch, relay traffic, and two-player
  in-game introspection with exit code 0.

## Verification

- Native `test_guidebot_route_certifier`: passed
- Android `:app:assembleDebug` with JDK 21: passed
- `test_abort_game_to_main_menu_d2.jsonc`: passed, 36/36 steps
- `test_obsidian_level1_objective_markers.jsonc`: passed, 196/196 steps
- `test_lan.ps1 -SkipBuild`: passed two-player direct-LAN synchronization
- `test_mp.ps1 -SkipBuild -SoakSeconds 0`: passed lobby, chat, ready, launch,
  relay traffic, and two-player in-game verification
- Scoped code quality and `git diff --check`: passed

## Fix Plan

- [x] Update the abort-save automation expectation for level-suffixed descriptions
- [x] Reject Guide-Bot switch firing candidates that equal the switch aim point
- [x] Harden direct-LAN readiness and matchmaking introspection synchronization
- [x] Run scoped formatting and focused native, automation, LAN, and multiplayer tests
