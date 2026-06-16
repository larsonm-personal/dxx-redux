# Multiplayer Resume Desync Implementation - 2026-06-16

## Context
- Logs from `game_data/logs_net_failure` show restore/rejoin churn before later item and forcefield desync.
- Likely root causes are incomplete coop auto-restore synchronization, object mapping divergence after player spew, and trigger/wall state drift.
- Start with the smallest implementation that reduces state divergence and adds targeted diagnostics for the next device run.

## Plan
- [x] Inspect current coop restore, object sync, and trigger packet code paths
- [x] Prevent non-master peers from applying coop auto-restore locally during multiplayer resume
- [x] Add targeted Android debug logs for coop autosave resync requests
- [ ] Add or extend focused host tests where practical
- [x] Run scoped formatting and available validation

## Notes
- No focused host unit test was added in this slice because the changed behavior depends on live UDP request/rejoin state and Android-only native branches.
- Validation covered `git diff --check`, scoped `android/run-code-quality.ps1 -Fix`, D1 Windows build, and Android `:app:assembleDebug`.
