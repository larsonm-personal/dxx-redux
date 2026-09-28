# LAN restart diagnostics and hosting UI

- Correlate both supplied logs with discovery, save validation, and native startup
- Preserve the host IP and mission download setting name while hosting/base content is selected
- Publish host save incompatibility to discovered and joined clients without hiding the lobby
- Retire an orphaned game process before multiplayer launch, preserving returnable sessions
- Run scoped formatting, Kotlin tests, Android native build, and emulator regression coverage

## Evidence

- Both devices run build 23390 / 771ab942
- Host selects Descent co-op slot 6 at 21:42:33, 21:43:27, and 21:44:14
- Host sends announcements and receives its own broadcasts; client receives zero discovery packets during this interval
- No save incompatibility rejection appears; save selection already checks native compatibility
- Client direct engine join at 21:44:24 times out after 29 requests
- Client activity is destroyed at 21:44:49 while native auto-join continues
- All three later starts at 21:48:59, 21:49:30, and 21:50:00 hit the native already-running guard

## Validation

- Scoped code quality checks passed
- Android x86_64 debug APK built, including both D1 and D2 CMake targets
- LobbyProtocolStartOptionsTest and LobbyDiagnosticsTest: 14 tests passed
- test_coop_save_compatibility.ps1 passed on emulator-5554: host blocks invalid save, actual UDP discovery reply includes the client warning, Start fresh clears both
- test_lan_orphan_restart.ps1 passed on emulator-5554: CLEAR_TOP destroys the waiting client Activity while native auto-join survives; the next D1-in-D2 LAN join retires that PID and reaches native auto-join with a new PID
- Original physical-network discovery outage cannot be reproduced from these logs alone; added broadcast destination logging for a future capture
- APK used for validation: android/app/build/intermediates/apk/debug/app-debug.apk (the injected-ABI build leaves the older outputs/apk copy untouched)
