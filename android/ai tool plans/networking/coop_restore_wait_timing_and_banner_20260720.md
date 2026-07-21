# Coop restore wait timing and banner

## Goal

Make server-started coop save restoration occur at a predictable synchronization
point, show a fixed waiting banner on every peer until restoration completes, and
show a timed error banner if restoration fails.

## Plan

- [x] Trace server launch options, native lobby readiness, restore scheduling, completion, and failure paths in D1 and D2
- [x] Identify the source of variable restore delay and define a bounded synchronization rule
- [x] Implement shared wait/error status state and fixed top-of-screen rendering in D1 and D2
- [x] Add or extend high-level regression coverage for waiting, success, timeout, and failure transitions
- [x] Run scoped formatting/lint, focused tests, D1/D2 or Android builds, and relevant integration checks
- [x] Record findings and verification here

## Findings

- The pre-restore delay used rendered frames, not elapsed time: both games waited
  30 frames, then D1 and D2 used different 150-frame and 300-frame readiness
  deadlines. Frame rate and the different deadlines made the delay variable.
- The readiness condition is `multi_all_players_alive()`. It requires every peer
  to be connected and no player to remain in the killed/initializing state. This
  is distinct from launcher lobby readiness.
- After that gate, the authoritative save transfer has two deliberate peer
  acknowledgments: buffer allocation readiness before chunks are sent, and
  apply readiness before the host restores. The transfer retains its 60-second
  wall-clock fault timeout.
- The initial gate now uses a shared 250 ms wall-clock settle period and starts
  as soon as every peer is alive. If this does not happen within 10 seconds, the
  restore fails visibly instead of silently disarming.
- The host broadcasts restore waiting/failure state separately from the save
  payload, so clients see the waiting banner even while the host is still at the
  all-players-alive gate. A failed authoritative transfer no longer falls back
  to a local-only restore that could leave the peers divergent.
- D1 and D2 reserve a fixed top HUD row for `Waiting to restore save`. Normal HUD
  messages are shifted below it. Successful restore clears the row; readiness,
  transfer, checksum, allocation, file, and restore failures show
  `Save restore failed` in red for 10 seconds.
- Debug introspection now reports `coop_restore.status` as `idle`, `waiting`, or
  `error`, plus the displayed message. The slot-remapped two-peer restore test
  checks for synchronization errors and requires the state to return to idle on
  both peers.

## Verification

- Scoped `android/run-code-quality.ps1 -Fix`: passed.
- `gradlew :app:assembleDebug :app:testDebugUnitTest` with JDK 21: passed after
  the final source changes.
- `run-windows-build.ps1`: passed for D1 and D2 after the final source changes.
- `android/tests/test_lan.ps1 -Game d2 -GuidebotSlotRemapRestore -SkipBuild
  -TimeoutSeconds 120`: attempted, but the second AVD did not appear in adb after
  both the harness's hardware-rendered and software-rendered 90-second starts.
  A separate logged 55-second software-rendered boot also failed to register.
  The integration scenario did not reach game launch, so this is an emulator
  availability failure rather than a restore assertion failure.
