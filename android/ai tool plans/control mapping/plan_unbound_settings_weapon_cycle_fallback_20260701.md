# Unbound Settings Weapon Cycle Fallback

## Goal
Replace individual Weapon N overflow entries with next-primary and next-secondary fallback entries, and show each fallback only when the player has no complete weapon access path through direct weapon bindings or cycle controls.

## Plan
- [x] Locate weapon bindings, cycle bindings, and existing remaining-action tests
- [x] Update remaining-action candidate logic for primary and secondary weapon fallback
- [x] Extend tests for complete direct access, cycle access, and missing access cases
- [x] Run scoped formatting and focused tests

## Verification
- Passed `.\android\run-code-quality.ps1 -Fix -Paths @(...)` on the changed Kotlin files and plan files
- Passed `.\gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.RemainingKeyTouchActionsTest`
