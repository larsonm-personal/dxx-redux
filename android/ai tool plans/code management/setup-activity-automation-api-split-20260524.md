# SetupActivity automation API split - 2026-05-24

## Goal
Continue reducing `SetupActivity.kt` by moving automation and introspection helpers into `SetupAutomationApi.kt` while keeping receiver registration and launch orchestration in the activity.

## Plan
- [x] Confirm helper boundaries, call sites, and same-package name collisions.
- [x] Move accessibility scanning/tap/scroll helpers as `SetupActivity` extension functions without changing `SetupActivity.ButtonInfo` consumers.
- [x] Move setup, controller, and multiplayer introspection JSON writers plus pilot/save cleanup helpers.
- [x] Keep broadcast receiver fields, lifecycle registration, multiplayer launch, and game launch helpers in `SetupActivity.kt`.
- [x] Run focused diagnostics, JVM tests, scoped code-quality checks, and line-count/diff checks.
- [x] Update this plan and the survey plan with results.

## Notes
- Leave `ButtonInfo` nested in `SetupActivity` because `LauncherScriptExecutor` already references `SetupActivity.ButtonInfo`.
- Prefer explicit `this@SetupActivity.` calls from receiver objects so moved extension functions resolve clearly.
- Avoid moving receiver fields in this tranche; they close over private launch state and would require a higher-risk callback object.

## Results
- Added `SetupAutomationApi.kt` with accessibility scanning, tap injection, scroll automation, setup introspection JSON, controller introspection JSON, multiplayer introspection JSON, pilot patching, and automation save/pilot cleanup helpers.
- Left `SetupActivity.ButtonInfo` nested in `SetupActivity` so existing `LauncherScriptExecutor` type references did not need churn.
- Left broadcast receiver fields and launch orchestration in `SetupActivity.kt`; moving those would need a callback/controller object because they close over private launch and multiplayer state.
- `SetupActivity.kt` was 5079 lines before this tranche and is 4178 lines after formatting. `SetupAutomationApi.kt` is 887 lines, for about 14 net fewer lines in this tranche.
- The full SetupActivity family is now about 9799 lines versus the original 9709-line single file, keeping the overall split roughly line-count stable while making the activity much smaller.
- Validation passed with focused diagnostics, `:app:testDebugUnitTest` before and after formatting, and scoped `android\run-code-quality.ps1 -Fix -Paths ...`.
