# Plan: Replay Manual Step Controls (2026-05-03)

## Goal
Add manual replay stepping controls for input-demo replay:
- Space toggles play/pause
- Right Arrow advances one frame while paused

## Steps
- [x] Locate replay frame stepping path and input handling hook
- [x] Add replay pause/step state and API in shared replay module
- [x] Bind Space and Right Arrow in the event/input path when replay is active
- [x] Ensure normal gameplay input behavior is unchanged when replay is not active
- [x] Build host d2 and verify no compile regressions
- [x] Update this plan with results

## Results
- Implemented in d2/main/game.c inside game window event handling
- While input-demo replay is active:
	- Space toggles replay pause/play
	- Right Arrow pauses replay and advances exactly one frame
- Replay frame stepping in EVENT_WINDOW_DRAW now honors manual pause/step state
- State resets automatically when replay is not active and on window close
- Host d2 build succeeded via run-windows-build.ps1 (existing loadgl.h macro warnings unchanged)
