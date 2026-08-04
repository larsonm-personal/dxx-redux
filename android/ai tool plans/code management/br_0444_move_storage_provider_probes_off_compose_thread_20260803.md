# BR-0444 move storage and provider probes off Compose thread

## Plan

- [x] Read repository instructions, BR-0444, related findings, and the remediation process
- [x] Trace storage and provider probes executed during composition and their state consumers
- [x] Move probes to cancellable background work with explicit loading and failure state
- [x] Run scoped code quality and compile-focused validation for this tranche
- [x] Archive BR-0444 after the shared end-to-end SAF and emulator validation pass

## Validation

- Scoped Kotlin formatting and lint passed for all twelve touched production files
- `:app:compileDebugKotlin` passed with JDK 21
- All JVM unit tests and debug APK assembly for all three Android ABIs passed
- Focused SAF emulator validation reached launcher readiness and native game startup, and proved the mounted manifest descriptor was released
- The broader SAF gameplay script twice stalled later in `skip_briefing`, after the launcher, game startup, mount, and descriptor checks had succeeded
