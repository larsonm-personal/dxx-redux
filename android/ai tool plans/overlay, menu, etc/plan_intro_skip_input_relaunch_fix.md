# Intro skip relaunch fix

## Status

- Completed: confirmed the second phase failure was in the launcher automation relaunch path after `LAUNCHER_CONTINUE`
- Completed: moved the fix into the test harness by force-stopping the app and relaunching `SetupActivity` cleanly before resuming launcher automation
- Completed: rebuilt, reinstalled, and reran the affected D2 intro regression successfully

## Plan

1. Inspect the launcher resume path after `LAUNCHER_CONTINUE`
2. Apply the smallest relaunch fix in the correct layer so the second phase starts from a clean process
3. Rebuild/install the debug APK and rerun the affected D2 intro regression
4. Update this plan with the verification result

## Verification

- `android\run-code-quality.ps1 -Fix`: pass for C/C++ and Kotlin, local PowerShell lint still blocked by missing `PSScriptAnalyzer`
- `android\gradlew.bat installDebug` with `JAVA_HOME=c:\local\jdk-21`: pass
- `android\run_test.ps1 -ScriptName test_intro_skip_inputs_unified.json5 -Game d2 -TimeoutSeconds 120`: pass after the harness clean restart on `LAUNCHER_CONTINUE`