# Mouse edge full-rate zone plan - 2026-08-23

Goal: Add a configurable screen-anchored outer band that moves the continuous-movement full-rate point inward from the physical screen edge.

1. [done] Trace the existing edge movement model, configuration, editor, and tests
2. [done] Add the full-rate zone setting with a 7% default and integrate the screen-anchored keep-out calculation
3. [done] Extend persistence, validation, human-readable import, and editor controls
4. [done] Add focused behavior and serialization tests
5. [done] Run scoped formatting, focused Android unit tests, and debug APK assembly

The existing edge size remains a percentage of the touch-region width or height. The new percentage is measured in a different coordinate space: a fixed keep-out band measured as a percentage of the full screen width or height from the physical screen edge. The editor descriptions and tests must state those distinct bases explicitly. For a right-side touch region ending at X=98% and a 7% screen-edge zone, full rate begins at screen X=93%; the preceding ramp is still sized as a percentage of the touch-region width.

Verification completed:

- Scoped code quality and ktlint formatting passed for all changed files
- `TouchMouseEdgeMovementTest` passed
- `:app:assembleDebug` passed for arm64-v8a, armeabi-v7a, and x86_64
