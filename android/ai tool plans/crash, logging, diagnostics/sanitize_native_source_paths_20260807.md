# Sanitize native source paths

## Goal

Prevent Android-facing native error messages from exposing absolute build-machine paths while preserving useful project-relative source locations and line numbers.

## Plan

- [x] Trace the error formatting and Android display or logging path
- [x] Implement narrowly scoped project-root path sanitization
- [x] Verify Windows and Unix-style compiler prefix remapping
- [x] Run scoped code quality, relevant tests, and the required CMake build and test checks
- [x] Record verification results and mark this plan complete

## Results

- Added an Android-only compiler prefix map so `__FILE__` omits the absolute project root
- Confirmed Windows and Unix-style absolute source paths preprocess to `temp\source_path_probe.c` and `d2\main\game.c`
- Confirmed rebuilt D1 and D2 objects contain relative runtime macro strings such as `d1/main/game.c` and `d2/main/game.c`
- Scoped code quality passed for the changed CMake file
- `:app:testDebugUnitTest` and `:app:assembleDebug` passed
- D1 and D2 native host builds and CTest suites passed
