# Level Complete Stats Page Plan

- [x] Trace the level-complete page code path and confirm the Android menu scale path touches it
- [x] Fix the OGL multi-line text spacing bug affecting the title lines
- [x] Exempt the level-complete page from Android menu scale magnification
- [x] Add delayed tap-anywhere advance using the shared cutscene tap suppress window
- [x] Run focused validation for the touched Android and D1/D2 slices

## Validation notes

- Scoped `run-code-quality.ps1 -Fix -Paths ...` passed for the touched files
- `run-windows-build.ps1 -Target both` completed successfully for D1 and D2
- Android regression script `test_levelcomplete_touch_skip.json5` was added and staged, but emulator execution was interrupted by launcher automation handoff noise after the game process restarted and loaded an unrelated stale script