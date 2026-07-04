# Guidebot Wheel Next And Release Cleanup

Goal: make Guidebot Next a normal radial slice instead of a center-only action,
and hide Guidebot Release from the touch guidebot wheel because abdication already
covers the useful ownership transfer path.

## Plan
- [x] Locate the radial guidebot action filtering and default layout source.
- [x] Change the guidebot wheel so Next appears as a regular segment.
- [x] Hide Release from the guidebot wheel where it is exposed.
- [x] Add or update focused tests for the guidebot wheel behavior.
- [x] Run scoped formatting and focused tests.

## Verification
- `.\android\run-code-quality.ps1 -Fix -Paths @(...)`
- `.\gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.GuidebotLockedWheelTest --tests com.dxxredux.app.RemainingKeyTouchActionsTest --tests com.dxxredux.app.GyroToggleConfigTest --no-daemon`
