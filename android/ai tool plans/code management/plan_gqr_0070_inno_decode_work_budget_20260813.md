# GQR-0070 Inno aggregate decode-work budget

## Goal

Bound aggregate solid-chunk decode work for one open Inno extraction attempt so repeated selected entries or aliases cannot restart and decode the same near-limit prefix thousands of times

## Scope

- Keep the implementation in branch-new Android extraction files
- Preserve the existing Inno metadata live-memory and version-admission fixes
- Charge actual compressed input consumed by each outer chunk decoder to one archive-owned attempt budget
- Fail before work exceeds the limit while preserving progress cancellation and normal multi-file installers
- Add focused fixtures for exact-limit and one-over admission, repeated aliases, unique chunks, cancellation, and budget reset on a fresh archive

## Phases

- [x] Record the durable plan and inspect the current extraction paths
- [x] Implement one archive-owned decode-work budget across buffered and streamed chunk decoders
- [x] Add focused regression coverage and test-only accounting hooks
- [x] Run scoped formatting and focused Windows tests
- [x] Compile Android D1 and D2 objects for available ABIs
- [x] Review the final diff for warnings, ASCII, and unrelated changes

## Validation record

- Scoped code-quality pass completed for the Inno reader files
- MSVC Release `test_gog_fd` target built successfully
- Focused test passed against the checked-in D1 and D2 GOG installers, including exact-limit, one-over, 4096 aliases, unique chunks, fresh-attempt reset, and cancellation cases
- Gradle `:app:externalNativeBuildDebug` passed for arm64-v8a, armeabi-v7a, and x86_64, building both D1 and D2 native targets
