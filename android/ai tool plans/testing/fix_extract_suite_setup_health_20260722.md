# Fix extraction suite SetupActivity health handling

## Goal

Prevent whole-library extraction runs from recording per-source failures when
the emulator is booted but SetupActivity is not responsive.

## Plan

- [x] Create the remediation plan
- [x] Read repository instructions and trace emulator and SetupActivity health checks
- [x] Inspect live device state without interfering with the active suite
- [x] Identify startup timeout or stale-state root cause
- [x] Add bounded recovery and suite-level fail-fast behavior
- [x] Add focused regression coverage
- [x] Run scoped code quality, focused tests, build, CTest, and a live extraction
