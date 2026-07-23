# BR-0026 HFS catalog record bounds

## Goal

Reject malformed HFS catalog nodes before decoding any offset-table, key, or
type-specific record field outside its validated record interval

## Plan

- [x] Create the remediation plan
- [x] Read repository instructions, the complete finding, and review-process requirements
- [x] Trace catalog node parsing and existing HFS test helpers
- [x] Add checked node, offset-table, record-interval, key, and payload validation
- [x] Add valid-boundary and malformed catalog-node regression tests
- [x] Run scoped code quality, focused and complete native tests, sanitizer-equivalent checks where available, and Android ABI builds
- [ ] Complete the required independent P1 verification and finalize the finding disposition

## Validation

- Focused HFS tests passed 9/9 against both known Mac discs
- The malformed-node corpus passed combined AddressSanitizer and UndefinedBehaviorSanitizer on the x86_64 Android emulator
- The complete native extraction suite passed 13/13
- Android debug native builds passed for arm64-v8a, armeabi-v7a, and x86_64
- Scoped code quality passed for the parser, header, tests, plan, and ledger
