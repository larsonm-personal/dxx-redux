# Dormancy resume profiling suppression

## Plan

- [x] Trace the lifecycle resume and profiling frame boundaries
- [x] Add a minimal one-frame slowdown-detector suppression flag for resume
- [x] Add regression coverage for an intentional resume discontinuity
- [x] Run scoped code quality, relevant tests, and Windows build verification

## Verification

- Scoped `run-code-quality.ps1 -Fix` passed
- D1 and D2 slowdown-detector test executables passed
- `run-windows-build.ps1 -Target both` passed
- Android `:app:assembleDebug` passed for arm64-v8a, armeabi-v7a, and x86_64
