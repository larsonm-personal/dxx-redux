# Cutscene video border corruption

Investigate D2 movie playback borders when the movie image does not fill the screen, then apply the smallest rendering fix that preserves desktop behavior.

## Plan

- [x] Trace D2 `.mve` frame drawing and page flip behavior
- [x] Identify whether border pixels persist from earlier frames or screens
- [x] Clear the fullscreen movie window before each presented movie frame
- [x] Build or run focused validation

## Notes

- `PlayMovie()` enters `RunMovie()` with `dx == -1` and `dy == -1`; embedded robot movies use explicit coordinates and should not clear the whole briefing screen.
- `MovieShowFrame()` scaled and centered fullscreen movie frames but only drew the video rectangle, leaving any letterbox/pillarbox area untouched.
- The fix clears the current movie canvas to palette color 0 for fullscreen movies before the frame upload, including frames skipped due to a palette update.
- `run-windows-build.ps1` completed successfully for D1 and D2. The only warnings were existing `weapon.c` return-path warnings.
- CTest was available through `C:\local\android-sdk\cmake\3.31.6\bin\ctest.exe`, but both `buildd1` and `buildd2` reported no registered tests.
