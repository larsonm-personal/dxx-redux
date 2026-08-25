# About debug build marker

## Goal

Show `debug` on the launcher's About build line whenever the bundled native game is compiled as a debug build.

## Constraints

- Keep release About text unchanged.
- Treat the Play-internal build as debug because it deliberately uses CMake `Debug`, even though its Android `debuggable` flag is false.
- Do not change game behavior or the existing release/internal packaging model.

## Plan

- [x] Add explicit per-build-type native debug metadata to `BuildConfig`.
- [x] Append the debug marker to the About build line.
- [x] Add a focused formatting helper test for debug and release labels.
- [x] Run scoped code quality and Android unit/build validation.

## Validation

- `testDebugUnitTest` passed, including debug and release build-line cases.
- Debug and internal generated `NATIVE_DEBUG_BUILD = true`; release generated `false`.
- Internal and release Kotlin compilation passed.
- Scoped code quality and `git diff --check` passed.
