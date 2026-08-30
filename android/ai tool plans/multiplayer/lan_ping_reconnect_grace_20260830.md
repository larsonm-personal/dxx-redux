# LAN directed ping and reconnect grace

## Goal

Detect broken bidirectional LAN lobby communication promptly while allowing a phone that briefly sleeps or loses Wi-Fi to resume the same lobby seamlessly.

## Plan

1. [x] Add identified directed PING/PONG traffic to the existing three-second joined-lobby heartbeat and track round-trip host contact separately from broadcast discovery.
2. [x] Replace immediate host/client eviction with a reconnecting state after roughly three missed heartbeats and retain the lobby identity/slot for a bounded grace period.
3. [x] Surface reconnecting and disconnected state in both host and client lobby views, and restore normal state automatically when the same peer returns.
4. [x] Remove the mission-transfer-specific five-minute liveness exemption because heartbeat traffic remains independent of transfer progress.
5. [x] Add focused liveness/protocol tests and extend the two-device idle/reconnect regression coverage.
6. [x] Run scoped quality checks, relevant tests, and the Android debug build.

## Verification

- Scoped Kotlin and PowerShell quality checks passed.
- All 991 Android debug unit tests passed.
- `:app:assembleDebug` passed, including all configured Android native ABIs.
- The two-emulator LAN regression passed discovery, host-to-client chat, ready reflection, a 70-second idle window, background/resume, and a forced client Wi-Fi outage followed by automatic reconnection with ready state restored.
