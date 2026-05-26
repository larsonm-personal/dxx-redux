# Multiplayer join wait background corruption

Investigate the corrupted background behind the "waiting for signal from..." multiplayer join sequence, then restore the expected menu background in both games where applicable.

## Plan

- [x] Trace the D1/D2 waiting-for-signal drawing path
- [x] Identify where the menu background should be prepared or redrawn
- [x] Apply the smallest D1/D2 fix
- [x] Build or run focused validation

## Notes

- `net_udp_wait_for_sync()` builds the "Waiting for signal from" menu after level startup has already changed the screen state.
- The wait menus used `newmenu_do()`, which passes no fullscreen background PCX and only redraws the menu frame area.
- The fix changes the client sync wait and host request wait menus in both D1 and D2 to use `newmenu_do2(..., Menu_pcx_name)` so the normal game menu background is drawn behind the wait box.
- `run-windows-build.ps1` completed successfully for D1 and D2 after the change.
- CTest was available through `C:\local\android-sdk\cmake\3.31.6\bin\ctest.exe`, but both `buildd1` and `buildd2` reported no registered tests.
