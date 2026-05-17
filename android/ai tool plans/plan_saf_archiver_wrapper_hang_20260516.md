# Plan: SAF Archiver Wrapper Hang 2026-05-16

## Goal

- Reproduce the current `test_saf_archiver.ps1` wrapper failure, localize the blocking point, and fix only that wrapper path issue.

## Local hypothesis

- The wrapper may still be failing before its numbered test steps, likely in a helper or adb call rather than inside the SAF manifest or inner automation script.

## Cheap check

- Run `Resolve-GameDataDeps -Deps (Get-StandardGameDataDeps)` directly with explicit markers and timing on the current healthy emulator.
- If it returns, move one helper deeper to the specific adb call group inside `Resolve-GameDataDeps`.

## Result

- `Resolve-GameDataDeps -Deps (Get-StandardGameDataDeps)` returned `ok=True` in about 209 ms on the current emulator, so the old helper-hang hypothesis did not reproduce.
- Fresh wrapper reruns reached Step 8 and failed inside `test_saf_basic.json5` with `SELECT: item "New game" not found in menu (timed out)`.
- Fresh setup introspection confirmed the SAF manifest/readiness path was correct: `d2.ready=true`, `can_launch=true`, and `descent2.ham` was `saf_linked=true` from `.saf_manifest.json`.
- Fresh game introspection showed the real live failure state: the game had landed on the D2 `Multiplayer` submenu instead of the main menu.
- Root cause on the current repro: intro-skip taps in the inner automation can leave D2 in `Multiplayer` before the script tries to select `New game`.
- Fix: `android/game_scripts/test_saf_basic.json5` now normalizes the menu state by optionally selecting `Multiplayer`, pressing `esc`, then selecting `New game`.
- Validation: `./android/tests/test_saf_archiver.ps1 -NoBuild` passed end to end after the script change.

## Steps

- [x] Reproduce the wrapper failure at the helper boundary
- [x] Identify whether a blocking helper or adb call still reproduces
- [x] Fix only the local wrapper-path root cause
- [x] Rerun `test_saf_archiver.ps1` to confirm the wrapper succeeds
- [x] Update this plan with the final result