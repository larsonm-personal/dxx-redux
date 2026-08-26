# Autosave level description

## Goal

Append the current level number to every Android automatic-save description, for
example `[auto] 5min L11`, while preserving the fixed 20-byte save description
format in D1 and D2.

## Plan

- [x] Trace all automatic-save entry points and the shared description storage
      limit
- [x] Centralize level-suffixed automatic-save description formatting and use it
      for every automatic-save kind in D1 and D2
- [x] Extend focused native tests for normal and longest level values
- [x] Run scoped formatting, focused tests, and the required Windows CMake build

## Findings

- The save description payload is 20 bytes in D1, D2, and Android metadata
- `[auto] abort L11` is 16 bytes, so the example fits with four bytes to spare

## Completion

- Automatic save descriptions are formatted in the shared save path before the
  D1/D2 save body and Android metadata are written
- Manual and quick-save descriptions remain unchanged
- Scoped code quality checks passed
- `run-windows-build.ps1 -Target both` passed
- `test_android_save_meta.exe` passed for D1 and D2
