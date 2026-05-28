# Multiplayer 100 Percent Spew Rate Option Plan

## Goal
Add a multiplayer quality of life option that lets the host force death item spew to keep every eligible item instead of using the normal chance-based loss rules

## Steps
- [x] Locate D1 and D2 death spew decisions, netgame option storage, host setup menus, Android launcher lobby QOL plumbing, and profile persistence
- [x] Add a host-controlled netgame option for 100 percent death spew in D1 and D2 without affecting single player
- [x] Expose the option through the Android multiplayer QOL setup flow and keep launcher/native constants documented if duplicated
- [x] Add or update focused tests where practical
- [x] Run formatting and targeted build or test validation

## Notes
- Keep D1 and D2 changes paired where the code is duplicated
- Avoid moving source of truth for gameplay behavior into Kotlin
