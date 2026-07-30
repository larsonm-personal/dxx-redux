# BR-0437 typed preference import

## Goal

Make every exported app preference import with its declared JSON and
SharedPreferences type, including AcoustID web lookups and the default
headlight state.

## Plan

- [x] Confirm the finding and identify the export, import, and consumer paths.
- [x] Centralize the exported preference type declarations and use them for
  both serialization and validated import.
- [x] Add focused round-trip and wrong-type regression coverage for every
  currently exported Boolean and string preference.
- [x] Run scoped formatting, focused and complete Android unit tests, the
  debug APK/native ABI build, and a final diff audit.
- [x] Record results and mark the plan complete.

## Result

- One schema now declares every exported SharedPreferences key and its runtime
  type.
- Export uses the schema instead of the stored value's incidental type.
- Import validates the complete preference subsection before creating an
  editor, then writes each value with the schema's declared type.
- AcoustID web lookups and the default headlight state now round-trip as JSON
  and SharedPreferences Booleans.
- Wrong primitives, arrays, objects, and JSON nulls return a field-specific
  app-settings error without decoded preference values.

## Validation

- Scoped `run-code-quality.ps1 -Fix` passed for `ConfigImportExport.kt`.
- ktlint 1.8.0 passed directly for
  `ConfigImportExportPreferenceTest.kt`.
- The focused preference import/export test passed with JDK 21.
- The complete `:app:testDebugUnitTest` suite passed.
- `:app:assembleDebug` passed with CMake builds for arm64-v8a, armeabi-v7a,
  and x86_64.
- `git diff --check` passed.
