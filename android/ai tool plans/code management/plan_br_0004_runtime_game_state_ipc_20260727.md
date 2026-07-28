# BR-0004 runtime game state IPC

## Goal

Bridge authoritative native game state and matchmaking diagnostics between the
separate `:game` process and the default-process matchmaking owner without
duplicating the WebSocket or proxy ownership.

## Plan

- [x] Read repository instructions and the complete finding
- [x] Trace process ownership, lifecycle, state polling, and diagnostic reads
- [x] Implement a narrow same-UID IPC bridge with explicit disconnected state
- [x] Add two-process lifecycle and state-publication integration coverage
- [x] Run scoped quality, focused tests, native suites, and Android builds
- [x] Record the resolution and move BR-0004 to the done ledger

## Verification

- Focused JVM tests passed for complete network-state encoding, incomplete and
  non-network rejection, proxy and connection diagnostic round trips,
  host-disconnect policy, and single-use IPC session teardown.
- The complete Android debug JVM test suite passed.
- An emulator smoke test launched a LAN host through `SetupActivity`, confirmed
  the foreground service in the default process and `MainActivity` in `:game`,
  and confirmed an active Binder connection from `:game` to the service.
- Killing only the `:game` process removed its service binding. Force-stopping
  the app then removed the foreground service without residue.
- Scoped Kotlin formatting and quality checks passed.
- All 17 native extraction tests passed.
- Both Windows host game builds passed.
- The JDK 21 debug APK build passed for arm64-v8a, armeabi-v7a, and x86_64.
- A live five-minute online match and late join were not available in the local
  smoke environment. Independent P1 verification remains a campaign closure
  action.
