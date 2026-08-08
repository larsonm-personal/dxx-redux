# BR-0646 Remediation Plan

## Scope

Make the GOG Redbook regression start from an empty owned audio-source state and prove that the current import publishes the tested source

## Work

- [x] Inspect the finding, reset lifecycle, GOG registration path, and focused tests
- [x] Implement test-scoped audio-source cleanup and empty-baseline assertions
- [x] Run scoped quality, focused tests, Android build, and the maintained integration test
- [x] Record exact validation and archive BR-0646 in the done ledger

## Validation

- Scoped code quality passed for the changed Kotlin source
- Focused GOG registration and audio-source persistence/artifact JVM tests passed
- `:app:assembleDebug` passed for arm64-v8a, armeabi-v7a, and x86_64 with JDK 21
- Windows EXE and Mac PKG GOG/Redbook emulator wrappers passed consecutively; the Mac run cleared the successful Windows source before proving its own registration
