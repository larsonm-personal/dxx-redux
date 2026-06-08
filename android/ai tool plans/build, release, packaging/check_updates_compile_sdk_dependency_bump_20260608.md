# check-updates compile SDK companion bump

## Goal
- Keep AndroidX dependency bumps from leaving the Android build unable to pass AAR metadata checks

## Plan
- [x] Inspect how `check-updates.ps1` discovers Android SDK platforms and AndroidX versions
- [x] Add/update dependency metadata handling so compile SDK can be bumped with libraries that require it
- [x] Ensure install sync can fetch the newly required platform through existing SDK helpers
- [x] Run scoped code quality and a Gradle validation task that exercises AAR metadata

## Validation
- `pwsh -NoProfile -NonInteractive -Command ... [scriptblock]::Create(...)`: parse ok
- `pwsh -NoProfile -ExecutionPolicy Bypass -File .\android\get_deps\check-updates.ps1 -NoPrompt`: Compile SDK showed installed/target/latest as 37
- `pwsh -NoProfile -ExecutionPolicy Bypass -File .\android\get_deps\check-updates.ps1 -InstallSelection 1`: installed `platforms;android-37.0`
- `pwsh -NoProfile -ExecutionPolicy Bypass -File .\android\run-code-quality.ps1 -Fix -Paths ...`: passed for touched PowerShell, shell, and plan files
- `.\gradlew.bat :app:checkInternalAarMetadata`: passed
- `.\gradlew.bat :app:bundleInternal`: passed
