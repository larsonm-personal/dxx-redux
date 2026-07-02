# Unbound Settings Controller Binding Filter

## Goal
Update the unbound settings menu so it hides actions already bound by the active controller config when a working controller is in use, while preserving the existing active touch interface filtering.

## Plan
- [x] Locate the unbound settings list builder and the source of active controller binding state
- [x] Add filtering for active controller bindings only when the active controller is usable
- [x] Extend focused tests for touch-only, controller-active, and no-controller cases
- [x] Run scoped formatting and tests for the changed files

## Verification
- Passed `.\android\run-code-quality.ps1 -Fix -Paths @(...)` on the changed Kotlin files and plan file
- Passed `.\gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.RemainingKeyTouchActionsTest`
