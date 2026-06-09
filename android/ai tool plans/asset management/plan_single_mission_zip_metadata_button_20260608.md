# Single mission ZIP metadata button

## Goal
For mission ZIPs with one metadata target, show one plain `Level metadata` button without a section header or mission-name button text.

## Plan
- [x] Re-read project instructions and inspect the current metadata button UI.
- [x] Update the UI branch for one target versus multiple targets.
- [x] Run scoped code quality and a focused compile/test check.

## Verification
- `.\android\run-code-quality.ps1 -Fix -Paths <touched files>` passed.
- `.\gradlew.bat :app:compileDebugKotlin` passed.
