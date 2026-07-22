# BR-0022 SOW Huffman length validation

## Goal

Reject malformed ARJ Huffman headers before any archive-derived length or
symbol count can index decoder tables, and propagate the failure without
producing output

## Plan

- [x] Read repository instructions, the review process, both ledgers, and the
  live SOW decoder and test registration
- [x] Validate Huffman construction inputs and return checked errors from both
  code-length readers
- [x] Add focused boundary and malformed-header regression tests
- [x] Run scoped code quality, native tests, and Android ABI builds
- [x] Audit handmade comments and update the BR-0022 disposition with exact
  verification evidence
- [ ] Complete the independent P1 verification call and move the finalized
  finding to the done ledger

## Validation record

- Scoped code quality passed for the SOW decoder, focused test, and CMake file
- All 11 registered native extraction suites passed, including the new SOW
  Huffman malformed-header suite
- Android debug native builds passed for arm64-v8a, armeabi-v7a, and x86_64
- A known valid SOW extracted 34 files totaling 52,935,956 bytes
- AddressSanitizer compilation was attempted, but the installed MSVC toolchain
  does not contain the required ASan runtime library; UndefinedBehaviorSanitizer
  is not available in the configured host toolchain
- Independent P1 verification remains pending
