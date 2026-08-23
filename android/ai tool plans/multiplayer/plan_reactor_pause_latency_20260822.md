# Fix multiplayer reactor pause latency

- [x] Trace the overlay, JNI, engine, and network paths for reactor pause and reproduce the delayed state transition from code or tests
- [x] Implement the smallest shared D1/D2 fix so pause and resume are applied and propagated promptly
- [x] Add or extend focused regression coverage for immediate multiplayer reactor pause behavior
- [x] Run scoped formatting, focused tests, and the required build/test verification
- [x] Record verification results and mark the plan complete

## Result

- Root cause: the automap tray queued the reactor action, but only the covered gameplay window consumed it, so the request waited until automap closed
- Added a shared reactor-pause pending consumer and called it from the D1 and D2 automap idle handlers
- Preserved the existing host-authoritative, reliable multiplayer request and state packets
- Added paired source-contract coverage for both automap handlers and the gameplay handler

## Verification

- Scoped code quality checks passed
- `python -m unittest android.tests.test_multiplayer_source_contracts` passed, 4 tests
- `run-windows-build.ps1 -Target both` passed for D1 and D2
- Native CTest runner passed; D2 ran 43 tests, while the current D1 build registered no CTest cases
- Android `:app:assembleDebug` passed for arm64-v8a, armeabi-v7a, and x86_64
