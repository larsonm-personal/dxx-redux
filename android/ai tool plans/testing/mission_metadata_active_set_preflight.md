# Mission metadata active-set preflight

## Goal

Prevent mission metadata regeneration from analyzing against a stale native file set, and stop before per-mission artifacts are written when standard base data is unavailable.

## Plan

- [x] Make the shared game-state reset atomically publish the default active-set path for both engines
- [x] Add a reusable device check for the active-set markers and required D1/D2 base HOGs
- [x] Run that check before the mission ZIP batch, after each reset, and after emulator recovery
- [x] Abort after two consecutive analyzer failures that report missing standard base HOGs
- [x] Add focused regression coverage for publication, validation, and failure classification
- [x] Run scoped formatting, tests, and an emulator verification

## Verification

- `android/tests/test_active_game_data_reset.ps1` passes
- Scoped `android/run-code-quality.ps1 -Fix` passes for all three changed PowerShell files
- Live emulator reset publishes both markers to `/data/user/0/com.dxxredux.app/files/imported/sets/default`
- Metadata-only `-MOON-.zip` batch smoke test passes all 8 steps with regression JSON output disabled
