# Coop Host Only Restore Fix 2026-07-08

## Goal
- Allow Android coop resume when only the host has the selected save game

## Steps
- [x] Study the new host/client logs and identify the failing restore branch
- [x] Trace existing multiplayer restore/sync code for host-directed save transfer behavior
- [x] Patch D1 and D2 consistently with minimal Android-specific churn
- [x] Run focused native checks and build verification

## Notes
- The reported dialog is raised from the local save id mismatch path in `multi_restore_game`
- The failing client had a stale local `coopsave.mg8` with file id `1232158468`; the host restored id `1129271120`
- Android coop hosts now send the selected save bytes through the existing chunked save transfer before falling back to the legacy restore packet
- Validation passed: scoped code quality, Android touched object builds for arm64-v8a/armeabi-v7a/x86_64, full Windows D1/D2 build, and `git diff --check`
