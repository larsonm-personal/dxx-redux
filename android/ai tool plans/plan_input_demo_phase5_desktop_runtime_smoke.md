# Input Demo Phase 5 Desktop Runtime Smoke Tranche

## Goal

Add a small desktop replay smoke runner that launches the built D1 and D2
executables against a generated input-demo fixture and verifies that runtime
replay reaches result comparison successfully.

## Constraints

- reuse the existing desktop binaries instead of adding another engine shim
- keep the fixture tiny and source-controlled in the test runner, not as a
  checked-in output directory
- use the local copied game-data directory that already exists in this repo
- validate D1 first, then run the shared D1/D2 smoke path on the final tree

## Planned Steps

- [x] Add a PowerShell smoke runner that writes a runtime-valid fixture
- [x] Validate the D1 desktop replay path end to end
- [x] Validate the D2 desktop replay path through the same runner
- [x] Run code quality, final Windows smoke validation, and Android native validation

## Notes

- D1 and D2 replay startup now set a one-shot skip-level-intro flag so runtime
  smoke runs do not stall in briefings before gameplay begins
- The smoke runner launches desktop binaries through ProcessStartInfo with
  UseShellExecute disabled because Start-Process stalled while direct/manual
  invocation completed normally
