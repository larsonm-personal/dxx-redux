# BR-0052 use bounded views when traversing PE resources

## Goal

Traverse PE resource directories, names, entries, and data records only through
checked views bounded by the declared resource section and source file.

## Plan

- [x] Read repository instructions and the complete BR-0052 finding
- [x] Perform the required P1 frozen-to-live disproof attempt and trace every
      PE resource offset, recursion edge, string, and data-entry consumer
- [x] Introduce one bounded resource view with checked relative-offset,
      multiplication, addition, depth, and cycle handling
- [x] Add truncated table, entry-count overflow, bad name/data offsets,
      cycles, excessive depth, exact-boundary, and valid-resource regressions
- [x] Run scoped code quality, focused PE/Inno tests, all native extraction
      suites, sanitizer validation where available, and Android ABI builds
- [x] Finalize BR-0052 and move its complete finding and disposition entry to
      the done ledger

## Verification

- P1 frozen-to-live disproof: CONFIRMED, the live parser retained the frozen
  attacker-sized allocation, 16-section truncation, incomplete directory and
  data-entry bounds, and unchecked RVA subtraction
- Focused `test_gog_fd`: PASS, including synthetic malformed PE boundaries,
  valid PE32 and PE32+, section index 17, and the real D1/D2 installers
- Scoped code quality: PASS for the reader, public test hooks, regression test,
  and this plan
- Native extraction suite: PASS, 13 of 13 registered tests
- Android native build: PASS for arm64-v8a, armeabi-v7a, and x86_64
- Sanitizers: unavailable because neither the Windows PATH nor WSL contains a
  Clang or GCC toolchain; the bounded traversal has no resource-sized
  allocation, and the oversized declaration regression rejects before access
