# Plan: Next failing tests triage 2026-05-16

## Goal

- Reproduce the next currently failing Android tests from the latest report, fix the most local root causes, and validate each fix with a fresh rerun.

## Initial targets

- `test_launcher_dpad`
- One additional still-failing test from the latest fresh report after confirming current repro

## Steps

- [x] Confirm the latest still-failing tests from the fresh report/logs
- [x] Reproduce `test_launcher_dpad` locally and identify the current root cause
- [x] Fix the smallest local cause and rerun `test_launcher_dpad`
- [x] Pick the next reproducible failing test and repeat
- [x] Update this plan with validated outcomes

## Validated outcomes

- `test_launcher_dpad` fixed by waiting for main-page DPAD focus readiness before focus-dependent key input
- `test_pause_menu_return` rerun looked non-repro on the current build
- `test_autosave_resume_unified` rerun looked non-repro on the current build
- `test_lan_lobby_discovery` reran cleanly on a fresh two-emulator setup
- `test_mp` was a real current failure: host launch could stall in startup movies when one emulator had optional intro MVLs and the other did not
- `test_mp` fixed by forcing the launch-intro skip for multiplayer launches in `MainActivity` both on create and on resume so the transient override is not lost before the engine reaches auto-host/auto-join
- Fresh validation after rebuild/install: `test_mp.ps1` passed through game launch and the full 90-second sustained connectivity phase
