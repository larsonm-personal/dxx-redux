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

- The smoke runner now uses a lean replay launch (`-hogdir`, title-skip,
  `-window`, `-nomusic`, `-nosound`, `-inputdemo-replay`) instead of adding
  pilot or players-dir arguments
- The smoke runner launches a disposable copy of the desktop binary from a
  temp sandbox and writes a minimal local `descent.cfg` there so the test does
  not inherit fullscreen, resolution, or other persistent settings from the
  normal build output directory
- The smoke runner now prints the active sandbox exe, data dir, and fixture dir
  before launch, and any failure repro points at the sandboxed executable path
- The smoke runner treats `result.actual.json` creation as completion and then
  terminates the process if it is still running, so no tester-specific replay
  exit plumbing is required in `game.c`
- D1 and D2 replay startup keep a one-shot skip-level-intro flag because the
  desktop replay smoke still stalls in first-level briefings without it
- The smoke runner launches desktop binaries through ProcessStartInfo with
  UseShellExecute disabled because Start-Process stalled while direct/manual
  invocation completed normally
