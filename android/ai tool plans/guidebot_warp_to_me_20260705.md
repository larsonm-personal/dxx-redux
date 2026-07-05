# Guidebot Warp To Me Plan

## Goal
Add a guidebot control command that recalls the guidebot to the player so it can recover from being clipped, stuck, or unreachable in odd level geometry.

## Tasks
- [x] Read project instructions and locate existing guidebot command/menu plumbing.
- [x] Add a player-facing guidebot command named `Warp to me` in the existing guidebot control surface.
- [x] Implement the recall behavior using existing object/segment placement helpers where possible.
- [x] Verify the change builds or passes the narrowest available quality checks.
- [x] Update this plan with implementation notes and validation results.

## Notes
- The command should be a recovery affordance rather than a normal pathing goal.
- Prefer moving the existing guidebot object near the player over respawning or duplicating it.
- Added meta action `META_GUIDE_WARP_TO_ME` / `GB: Warp to Me` and the Guide radial segment label `Warp Me`.
- Existing and bundled Guide wheels are updated through touch layout version 7 migration plus the Advanced/Claw preset JSON files.
- The engine command recalls an existing released guidebot, tries legal positions around the player, resets stale motion/path state, preserves coop ownership, and does not create a new bot.
- Input demos record/replay the command as `guidebot_warp_to_me`.
- Validation passed:
  - `.\android\run-code-quality.ps1 -Fix -Paths ...`
  - `.\gradlew.bat testDebugUnitTest --tests com.dxxredux.app.GuidebotLockedWheelTest assembleDebug --no-daemon --console=plain`
  - `git diff --check`
