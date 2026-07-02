# Guidebot Coop Key Goal Trace

## Goal
Trace and fix the coop guidebot behavior where the released guidebot keeps targeting a blue key that the team already has, instead of advancing to the next missing key.

## Plan
- [completed] Locate guidebot goal selection and key ownership logic
- [completed] Identify why coop ownership does not advance the key goal
- [completed] Patch the smallest D2-side behavior change that matches existing style
- [completed] Run focused validation or build checks, then update this plan

## Validation
- `git diff --check -- d2\main\escort.c d2\main\escort.h d2\main\collide.c d2\main\multi.c d2\main\powerup.c "android\ai tool plans\gameplay\guidebot_coop_key_goal_20260701.md"`
- `.\run-windows-build.ps1 -Target d2`
- `.\android\run-code-quality.ps1 -Fix -Paths d2\main\escort.c d2\main\escort.h d2\main\collide.c d2\main\multi.c d2\main\powerup.c "android\ai tool plans\gameplay\guidebot_coop_key_goal_20260701.md"`
