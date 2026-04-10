# Plan: Fix test failures from report_20260408_230542

## Status: COMPLETE

## Failures analyzed
1. test_autoselect_crash_unified (FAIL) -- "button Launch Descent 2 is disabled"
2. test_launcher_dpad (FAIL) -- "DPAD_CENTER did not navigate away from main page"
3. test_gog_installer_redbook_unified (TIMEOUT 5:01) -- no output, process killed

## Root causes

### 1. test_autoselect_crash_unified
The `tap_button` handler in LauncherScriptExecutor.kt finds the button but immediately
fails if it's disabled. It only retries when the button is NOT FOUND (scrolling, waiting
for UI render). When the button IS found but disabled (e.g. Compose hasn't recomposed
after file detection), it fails instantly without retry.

Fix: Modify tap_button to also wait/retry when button is found but disabled, within
the existing timeout period.

### 2. test_launcher_dpad
The Multiplayer button has `enabled = canLaunch`. The test doesn't push game data, so
canLaunch is false, the button is disabled, and disabled Compose buttons can't receive
focus. The `initialFocus.requestFocus()` silently fails, leaving nothing focused.
DPAD_CENTER has no target.

Fix: Add standard game data deps to test_launcher_dpad.ps1 so canLaunch is true and
the Multiplayer button is enabled/focusable. Source test_helpers.ps1 and push files.

### 3. test_gog_installer_redbook_unified
Multiple contributing issues:
- The `runSteps()` coroutine in LauncherScriptExecutor has no top-level try/catch.
  If any step throws an unexpected exception, the coroutine crashes without writing
  automation_result.json. The test runner sees nothing and times out.
- The `automateSetupReceiver` launches the coroutine via MainScope with no error handler.
  Uncaught exceptions go to logcat (with a random tag) but are invisible to the test runner.
- The test runner's logcat filter is `DXX-Automate:*` but LauncherScriptExecutor uses
  tag `DXX-LauncherScript`. So launcher script logs (including failures) are invisible
  to the fallback logcat check.
- Process timeout (300s) is only 4s above calculated script timeout (296s). No margin.

Fixes:
a. Add try/catch in runSteps() to call fail() on unexpected exceptions
b. Add error handler to the coroutine launch in automateSetupReceiver
c. Add DXX-LauncherScript to logcat filter in Watch-AutomationResult for launcher scripts
d. Increase process timeout from 300 to 420s

## Files to edit
- android/app/src/main/java/com/dxxredux/app/LauncherScriptExecutor.kt (fixes 1, 3a, 3b)
- android/tests/test_launcher_dpad.ps1 (fix 2)
- android/test_helpers.ps1 (fix 3c)
- android/run_all_tests.ps1 (fix 3d)
