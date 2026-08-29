# Argument defaults test crash

## Goal

Find and fix the standalone `test_args_defaults` access violation without changing unrelated GuideBot behavior.

## Plan

- [x] Reproduce the crash and identify the failing initialization or teardown path
- [x] Implement the smallest cross-platform-safe correction and add regression coverage if needed
- [x] Run scoped formatting, the focused test, the Windows host build, and the complete host test suite

## Validation

- The crash reproduced as Windows access violation `0xc0000005` while validating the first Android default table
- `args.c` receives one-byte project structure packing by including `pstypes.h` through `physfsx.h`, while the test previously declared `struct Arg` before that packing was active
- The test now includes `pstypes.h` before `args.h`, matching the linked library ABI for both games
- The standalone D1 and D2 `test_args_defaults` executables pass
- `run-windows-build.ps1 -Target both` completes successfully
- The complete D2 CTest suite passes 44/44 tests; the D1 build does not register a CTest suite, so its standalone shared test was run directly
- Scoped code-quality checks and `git diff --check` pass
