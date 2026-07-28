# BR-0015 remove the superseded disc-import JNI implementation

## Goal

Remove the obsolete duplicate disc-import JNI implementation while preserving
the canonical launcher bridge and all native extraction behavior.

## Plan

- [x] Read repository instructions and the complete BR-0015 finding
- [x] Compare both frozen and live JNI implementations, symbols, and build wiring
- [x] Identify the canonical implementation and safe deletion scope
- [x] Remove the superseded source and add a duplicate-wiring regression
- [x] Run scoped code quality, native tests, and Android ABI builds
- [x] Finalize BR-0015 and move its complete finding and disposition entry to
      the done ledger

## Verification

- The obsolete root implementation was deleted and the retained extract
  implementation is the only source definition
- Both D1 and D2 use one canonical CMake source variable, with a configure-time
  rejection if the obsolete root path reappears
- All ten JNI symbols have exactly one source definition and match Kotlin
- All six built D1/D2 shared libraries export exactly ten bridge symbols
- Scoped CMake quality and all 15 native support suites passed
- `:app:assembleDebug` with JDK 21 built arm64-v8a, armeabi-v7a, and x86_64
