# Guidebot Spawn Release Sync

## Goal
Fix the Android guidebot spawn-at-player path so coop peers treat the guidebot as released, matching the normal cage release behavior.

## Plan
- [completed] Compare spawn-at-player and cage-release state changes
- [completed] Patch release-state synchronization for escort owner packets
- [completed] Run focused validation/build checks

## Validation
- [completed] `git diff --check -- d2\main\escort.c "android\ai tool plans\gameplay\guidebot_spawn_release_sync_20260701.md"`
- [completed] `.\android\run-code-quality.ps1 -Fix -Paths d2\main\escort.c "android\ai tool plans\gameplay\guidebot_spawn_release_sync_20260701.md"`
- [completed] `.\run-windows-build.ps1 -Target d2`
