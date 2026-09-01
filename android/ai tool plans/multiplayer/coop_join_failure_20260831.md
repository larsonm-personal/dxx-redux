# Cooperative game join failure

## Goal

Identify and fix the client-side cooperative game join failure captured in the 2026-08-31 host and client debug logs, and improve the net-events viewer so long messages remain readable.

## Plan

1. [x] Correlate both peer logs and identify the failed protocol transition and peer roles.
2. [x] Trace the failing messages through the launcher, native bridge, and game networking code.
3. [x] Implement the smallest safe protocol or state fix and add diagnostic coverage where needed.
4. [x] Wrap and indent long net-event messages and reduce their display font size.
5. [x] Add or extend regression tests for the failure and viewer formatting.
6. [x] Run scoped quality checks, relevant tests, and the Android debug build.

## Verification

- The client entered level sync at 15:38:23.965 and began retrying its request, but never accepted the host's initial sync packet. After the host entered gameplay, the client dropped authenticated gameplay data because no peer route had been admitted. The host timed the client out at 15:38:43.010, and the client received dump reason 8 before the later full reconnect completed at 15:38:44.865.
- Added an authenticated initial-sync retry window. A repeated signed request resends the compact sync packet until the host receives that player's first position packet; later reconnects continue to use the normal full object-transfer path.
- Removed the misleading master-slot `illegal player num 0` diagnostic and bounds-checked P2P ping player slots before indexing them.
- Passed scoped `android/run-code-quality.ps1 -Fix`.
- Passed `NetworkEventsOverlayTest` through `:app:testDebugUnitTest` with JDK 21.
- Passed `test_net_udp_reconnect_auth` and `test_net_udp_initial_sync_retry` through CTest.
- Passed `:app:assembleDebug` for arm64-v8a, armeabi-v7a, and x86_64.
- Passed Windows D1 and D2 builds through `run-windows-build.ps1`.
