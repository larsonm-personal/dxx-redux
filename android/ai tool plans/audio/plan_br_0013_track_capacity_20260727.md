# BR-0013 check track capacity before writing parsed records

## Goal

Reject CUE inputs whose parsed track count exceeds the caller-provided storage
before writing any out-of-capacity track record.

## Plan

- [x] Read repository instructions and the complete BR-0013 finding
- [x] Compare the frozen and live parser, all callers, and existing coverage
- [x] Define the smallest fail-closed capacity contract
- [x] Implement the parser fix and add boundary regressions
- [x] Run scoped code quality, focused and native tests, and Android ABI builds
- [x] Finalize BR-0013 and move its complete finding and disposition entry to
      the done ledger

## Verification

- Scoped code quality passed for the parser, native test, plan, and ledgers
- Focused CUE/ISO tests passed with exact-capacity and guarded overflow cases
- All 15 tests in `android/tests/test_cue_iso.ps1` passed
- `run-windows-build.ps1 -Target both` built D1 and D2 successfully
- `:app:assembleDebug` with JDK 21 built arm64-v8a, armeabi-v7a, and x86_64
