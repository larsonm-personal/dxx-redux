# Fix Test Failures from report_20260406_144943

## Summary
6 test failures, all infrastructure/test-script issues (no game code changes needed).

## Fixes

### 1. test_button_discovery -- button text mismatch
- **Problem**: Step 6/7 looks for "Advanced Settings" but the launcher shows "Advanced"
- **Fix**: Change the assertion text in test_button_discovery.json5 from "Advanced Settings" to "Advanced"
- [x] Done

### 2. test_mod_loading -- Read-Host in non-interactive mode
- **Problem**: run_test.ps1 calls Read-Host for parameterized tests. When run_all_tests.ps1 launches it with `-NonInteractive`, Read-Host throws
- **Fix**: In run_test.ps1, detect non-interactive mode and auto-default to the first option instead of prompting
- [x] Done

### 3. test_lan_broadcast -- not classified as two-emulator test
- **Problem**: Requires 2 emulators but not in $twoEmuTests in run_all_tests.ps1, so it runs in the single-emu tier and fails
- **Fix**: Add "test_lan_broadcast" to $twoEmuTests
- [x] Done

### 4. test_lan_lobby_discovery -- not classified as two-emulator test
- **Problem**: Same as #3 -- hard-coded emulator serials, not in $twoEmuTests
- **Fix**: Add "test_lan_lobby_discovery" to $twoEmuTests. Also make it auto-detect emulator serials dynamically
- [x] Done

### 5. test_saf_archiver -- bare adb calls fail with multiple devices
- **Problem**: Uses bare `Adb` calls without `-s <serial>`. When a second emulator is present (from tier 2 tests), adb fails with "more than one device/emulator"
- **Fix**: Detect the target serial at startup (first emulator from `adb devices`) and inject `-s $Serial` into the Adb wrapper function
- [x] Done

### 6. test_mp -- server port conflicts (WSAEADDRINUSE)
- **Problem**: After killing processes on ports, Windows keeps ports in TIME_WAIT. The 2-second wait is insufficient. Also, run_all_tests.ps1's tier-level server may still be on the same ports
- **Fix**: (a) Wait for ports to actually be free after killing (poll instead of fixed sleep). (b) Also kill UDP listeners on port 9001 via Get-NetUDPEndpoint
- [x] Done
