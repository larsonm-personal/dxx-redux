# Fix BR-0431 Configuration Replacement Quoting

## Goal

Prevent imported or edited configuration values containing regex replacement metacharacters from crashing or being rewritten during configuration updates.

## Plan

- [x] Inspect BR-0431, the frozen evidence, current replacement code, callers, and existing tests
- [x] Add focused regression coverage that reproduces dollar-sign and backslash replacement failures
- [x] Implement the narrow replacement-quoting fix without changing configuration matching behavior
- [x] Run scoped formatting, focused and full relevant tests, Android builds, native CMake validation, and whitespace checks
- [x] Record the tested resolution, move BR-0431 to the done ledger, and verify ledger integrity

## Validation

- Pre-fix focused regression: failed as expected with `IndexOutOfBoundsException` for `$9`
- Post-fix `GraphicsConfigHelpersTest`: passed
- Full `:app:testDebugUnitTest`: 534 tests, 0 failures, 0 errors, 1 skipped
- `:app:assembleDebug`: passed
- Native CMake debug builds: passed for arm64-v8a, armeabi-v7a, and x86_64
- Scoped code quality: passed for `SetupConfigFiles.kt`
- Direct ktlint check: passed for `GraphicsConfigHelpersTest.kt`
- BR-0431 finding count: active 0, done 1; disposition-log count: 1
- Duplicate finding IDs across both ledgers: 0
- Pending resolutions in the done ledger: 0
- `git diff --check`: passed
