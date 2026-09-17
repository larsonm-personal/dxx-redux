# Live Guide-Bot objective endpoint motion

Preserve original escort movement while the planner retains an unfinished objective

## Evidence

- Supplied exports exist as Downloads/debuglog_20260916_221230.txt and debuglog_20260916_221246.txt
- The second export announces switch guidance at 22:16:10 and 22:18:02, then repeated reactor guidance from 22:18:32; it contains no detailed escort movement trace
- aipath.c explicitly clamps active objective paths at their final waypoint, bypassing original patrol reversal
- escort.c suppresses both original short-path wandering fallbacks while any strategic objective is active
- These restrictions belong to exact scripted route confirmation, not ordinary live escort behavior

## Work

1. Extend the native endpoint regression to cover AIM_GOTO_OBJECT and establish failure before fixing
2. Restrict endpoint holding to the actual route-confirmation actor; restore live short-path fallback
3. Run native live regressions and route-confirmation simulations, scoped formatting, Windows and Android native builds

Do not change switch completion, reactor destruction requirements, player actions, planner cadence, or objective selection

## Implemented and verified so far

- Added a read-only route-confirmation actor ownership query and used it to scope the endpoint clamp
- Restored both original short-path wandering fallbacks in the live escort decision loop
- Native endpoint comparison fails before the fix and passes after it
- Added actual Counterstrike level 1 reactor arrival and Maximum level 16 switch arrival coverage, including 12 seconds of GameProcessFrame with ordinary AI and physics, movement beyond 20 units, and retained unfinished objectives
- All three native fixtures pass twice with byte-identical results
- Revised the older Obsidian Android test so it no longer requires a stationary one-point path; it checks retained switch guidance and a multi-point path instead
- Scoped mixed-language code quality checks passed

The Android Obsidian script has not been rerun on an emulator; the native live tests provide the executed behavioral coverage

## Final validation

- Windows D1 and D2 full builds passed; Android x86_64 native build passed
- CTest: D1 50/50, D2 57/57
- Final native live runner: three fixtures, two identical runs each, all assertions passed
- Counterstrike, Obsidian, FirstStrike and Castaway: 88 route simulations completed, zero per-level status changes against checked-in simulation baselines
- No new compiler warnings in changed files; existing unrelated warnings remain
- Results: android/temp/guidebot_endpoint_validation and android/temp/guidebot_live_navigation
- Build/test logs: temp/endpoint_*.log
