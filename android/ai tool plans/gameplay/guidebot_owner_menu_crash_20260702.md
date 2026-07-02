# Guidebot Owner Menu Crash

## Goal
Fix the guidebot menu crash after the previous guidebot owner leaves a coop game and another player becomes host/owner.

## Plan
- [completed] Read crash log and local repo instructions
- [completed] Trace guidebot menu, owner migration, and stale state paths
- [completed] Patch the smallest safe fix
- [completed] Run focused validation/build checks

## Validation
- [completed] `git diff --check -- d2\main\collide.c d2\main\escort.c d2\main\escort.h d2\main\multi.c d2\main\powerup.c "android\ai tool plans\gameplay\guidebot_coop_key_goal_20260701.md" "android\ai tool plans\gameplay\guidebot_spawn_release_sync_20260701.md" "android\ai tool plans\gameplay\guidebot_owner_menu_crash_20260702.md"`
- [completed] `.\android\run-code-quality.ps1 -Fix -Paths d2\main\collide.c d2\main\escort.c d2\main\escort.h d2\main\multi.c d2\main\powerup.c "android\ai tool plans\gameplay\guidebot_coop_key_goal_20260701.md" "android\ai tool plans\gameplay\guidebot_spawn_release_sync_20260701.md" "android\ai tool plans\gameplay\guidebot_owner_menu_crash_20260702.md"`
- [completed] `.\run-windows-build.ps1 -Target d2`
