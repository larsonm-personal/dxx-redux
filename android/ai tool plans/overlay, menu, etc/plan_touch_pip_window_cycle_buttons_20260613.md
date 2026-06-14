# Touch PIP Window Cycle Buttons

Goal: expose Descent II's built-in `Shift-F1` and `Shift-F2` camera window cycling through the Android touch overlay without adding new game behavior.

Design:
- Add two D2-only touch meta actions:
  - `Cycle Left View`: dispatches the same engine key command as `Shift-F1`.
  - `Cycle Right View`: dispatches the same engine key command as `Shift-F2`.
- Make the actions available in the overlay editor's extra action picker, and in the in-game remaining-actions menu when not already bound.
- Keep them out of D1 layouts, matching existing D2-only action filtering.
- Do not alter the base engine camera cycle order or guided missile override behavior.

Implementation:
- [x] Add Kotlin binding IDs and labels.
- [x] Add native meta action dispatch for the two key commands.
- [x] Add/adjust tests for D2-only exposure.
- [x] Run focused formatting/tests.

Validation:
- `.\gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.RemainingKeyTouchActionsTest` passed.
- `.\android\run-code-quality.ps1 -Fix -Paths @(...)` passed for the touched Kotlin/C files.
- `.\gradlew.bat :app:assembleDebug` passed, including Android native builds for arm64-v8a, armeabi-v7a, and x86_64.

Follow-up:
- [x] Move the left/right view cycle controls from remaining-actions into the overlay settings tray.
- [x] Keep the controls D2-only and routed through the same base `Shift-F1`/`Shift-F2` meta actions.
- [x] Update settings-tray and remaining-actions tests.
- [x] Run focused formatting/tests.

Follow-up validation:
- `.\android\run-code-quality.ps1 -Fix -Paths @(...)` passed for the touched Kotlin/C/plan files.
- `.\gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.RemainingKeyTouchActionsTest --tests com.dxxredux.app.AdminTrayUiTest` passed.
- `.\gradlew.bat :app:assembleDebug` passed, including Android native builds for arm64-v8a, armeabi-v7a, and x86_64.
