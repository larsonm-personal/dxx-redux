# BR-0079 TinySoundFont render-thread tuning

## Goal

Route live TinySoundFont tuning changes through the thread that owns rendering
so synth mutation cannot race audio generation or teardown.

## Plan

- [x] Read repository instructions, review process, and the complete finding
- [x] Trace synth ownership, tuning entry points, command ordering, and teardown
- [x] Implement render-thread tuning commands with concurrency coverage
- [x] Run scoped code quality, focused tests, native suites, and Android builds
- [x] Record the resolution and move BR-0079 to the done ledger

## Verification

- Scoped code quality passed for the changed C, Python, and Markdown files
- `python -m unittest android.tests.test_tsf_render_thread_tuning`: 8 passed
- `android/tests/test_native_host_unit_tests.ps1`: 24 D1 and 28 D2 tests passed
- JDK 21 `:app:assembleDebug`: arm64-v8a, armeabi-v7a, and x86_64 passed
- ThreadSanitizer and Android AddressSanitizer runners were unavailable
