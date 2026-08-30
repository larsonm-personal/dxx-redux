# Mission ready terminal status

## Goal

Ensure a successfully downloaded, verified, and imported mission leaves the lobby in the `MATCH` state displayed as `Mission ready`, rather than remaining at `Finalizing mission - 100%`.

## Plan

- [x] Trace finalization completion and lobby status propagation
- [x] Correct the successful terminal state transition
- [x] Add a focused regression test
- [x] Run scoped formatting, focused tests, and an Android debug build

## Constraints

- Preserve `FINALIZING` as an active transient state while import work is still running
- Only report `MATCH` after the installed mission resolves against the host requirement
- Preserve unrelated mission transfer and lobby changes in the dirty worktree

## Validation

- Scoped code quality passed
- `LobbyMissionRefreshTest` and `LanMissionStatusDisplayTest` passed
- `:app:assembleDebug` passed
- `git diff --check` passed

Status: complete
