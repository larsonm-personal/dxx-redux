# Generous touch layout recovery

## Goal

Preserve and repair on-device customized touch layouts across upgrades instead of rejecting the entire layout and silently substituting a bundled preset.

## Plan

- [x] Confirm the regression and separate trusted on-device storage from untrusted imports
- [x] Define tolerant geometry and numeric normalization for app-authored persisted layouts
- [x] Add actionable logging when storage cannot be recovered instead of swallowing the cause
- [x] Add upgrade regression tests for crossed zones, float boundary noise, and repairable slots
- [x] Run scoped formatting, the complete Android JVM suite, and a debug APK build

## Result

- Crossed zone edges are reordered rather than rejected.
- Collapsed or sub-two-percent zones expand around their center to the editor's minimum usable span.
- Persisted float maximums are compared against the exact serialized `Float` boundary.
- Irrecoverable storage remains unchanged and now records the actual load error before using an in-memory default.
- Strict rejection remains for malformed structure, non-finite values, and unsupported bindings or axes.
- Scoped code quality, `:app:testDebugUnitTest`, and `:app:assembleDebug` passed. The APK build configured and built all three Android ABIs.

## Constraints

- Keep external configuration imports strict and transactional.
- Preserve as much user-authored state as possible.
- Do not modify unrelated concurrent reticle-preview work or the existing fixture change.
