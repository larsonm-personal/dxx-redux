# Indicator line fade visibility plan

## Goal
Keep coop player and guidebot navigation helper lines visible deeper into the view cone, then fade them out and back in over about one second instead of snapping off/on.

## Steps
- [x] Create this plan.
- [x] Trace current helper-line target visibility checks and render alpha handling.
- [x] Loosen the on-screen/view-cone suppression threshold.
- [x] Add per-target fade state for smooth hide/show transitions.
- [x] Add focused tests or a small host-side test where practical.
- [x] Run scoped code quality and relevant build/test commands.

## Notes
- Existing unrelated launcher progress edits are present. Do not touch or revert them.
- The old suppression hid lines as soon as the target projected anywhere inside
  the viewport. The new check waits until the target is in the inner viewport.
- Player and guidebot paths now carry independent alpha, advancing with
  `FrameTime` over roughly one second.
- Validation passed:
  - `.\android\run-code-quality.ps1 -Fix -Paths ...`
  - `git diff --check -- ...`
  - `.\android\tests\test_native_host_unit_tests.ps1`
  - `.\gradlew.bat :app:externalNativeBuildDebug`
