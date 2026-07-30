# BR-0466 custom-audio import preservation

## Goal

Prevent an empty or failed add operation from deleting an existing copied
custom-audio set, while preserving cleanup for a newly created empty set.

## Plan

- [x] Trace the finding through destination selection, import cleanup, metadata
  publication, and existing focused tests.
- [x] Isolate import-attempt cleanup ownership from the selected set directory
  and implement the smallest safe fix.
- [x] Add focused regression coverage for empty imports into new and existing
  set destinations.
- [x] Run scoped formatting, focused tests, and the relevant Android build
  verification.
- [x] Record validation results and mark the plan complete.

## Result

- Copied custom-audio imports now use a unique sibling staging directory.
- Empty, failed, and storage-exhausted attempts remove only staging bytes.
- Existing set bytes and metadata are not touched until at least one supported
  file is ready to publish.
- Locale-stable, case-insensitive preflight rejects collisions with retained
  tracks, destination files, or another file in the same attempt; final
  publication does not overwrite.
- Focused tests cover failed cleanup against an existing set, an empty new-set
  attempt, successful append publication, and collision rejection.

## Validation

- Scoped `run-code-quality.ps1 -Fix` passed for the production Kotlin file.
- ktlint 1.8.0 passed directly for the focused test file.
- `:app:testDebugUnitTest` passed with JDK 21.
- `:app:assembleDebug` passed with CMake builds for arm64-v8a, armeabi-v7a,
  and x86_64.
- `git diff --check` passed.
