# Screen-off lobby liveness

## Goal

Keep LAN lobby networking alive with the display allowed to turn off normally.

## Plan

- [x] Remove the lobby screen-awake behavior
- [x] Reuse the multiplayer foreground service for active hosted/joined LAN lobbies
- [x] Hold a partial CPU wake lock only while the multiplayer foreground service is active
- [x] Verify lobby/service lifecycle cleanup and add focused coverage
- [x] Run scoped formatting, unit tests, Android build, and attempt screen-off two-emulator validation

## Constraints

- Do not keep the display awake
- Do not keep the foreground service or wake lock active for passive LAN browsing
- Preserve the existing game-process foreground-service behavior
- Preserve transport supervision, watchdog recovery, and unrelated dirty-worktree changes

## Validation

- Scoped code quality passed
- Foreground-service lease, background-deadline, lobby diagnostics, and mission-refresh tests passed
- `:app:assembleDebug` passed
- The LAN integration harness now turns both displays off during its 70-second liveness window and checks ready, unready, and chat while they remain off
- The screen-off emulator run was attempted twice, then with cold boot and software rendering; both AVDs failed before ADB registration, so this environment could not execute the updated device test
- `git diff --check` passed

Status: complete
