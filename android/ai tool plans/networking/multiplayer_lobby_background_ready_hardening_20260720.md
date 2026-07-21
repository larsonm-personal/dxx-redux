# Multiplayer lobby background ready hardening

## Goal

Keep a hosted lobby usable when the host and joiners background the Android app or
remain idle long enough to cross connection timeouts. Restore authoritative ready
state after reconnecting without requiring the host to destroy and recreate the
game.

## Plan

- [x] Trace lobby ownership, ready state, heartbeat/timeouts, and Android lifecycle handling across client and server
- [x] Identify failure modes for host and joiner suspension, reconnect, and stale server-side membership
- [x] Implement bounded lifecycle and protocol hardening with diagnostic logging
- [x] Add regression tests for the affected state transitions and timeout cases
- [x] Run scoped formatting/lint, focused tests, server validation if touched, and the required build/test checks
- [x] Record findings, implementation details, and completed verification here

## Findings

- The active release is LAN-only. LAN joiners sent no periodic packets after
  becoming ready, but hosts pruned any joiner with no packet for 10 seconds.
  This made an idle lobby lose membership even without an Android lifecycle
  event.
- Background pruning was suppressed locally, but resume reopened and replaced
  the UDP socket. There was no joiner membership refresh after that replacement,
  no grace lease on the resumed host, and no stored ready intent to replay if
  the host had already pruned and re-added the joiner.
- READY and LEAVE were matched by callsign alone. A mismatched packet could
  mutate or remove another row, including the host row. The host also accepted
  inbound PLAYER_LIST packets despite being the authoritative roster owner.
- Joiners now refresh their idempotent JOIN lease every three seconds. This
  prevents idle pruning and lets a pruned member re-enter without recreating the
  lobby. JOIN_ACK replays the joiner's selected ready state.
- Resume immediately refreshes a joiner's membership and gives existing remote
  host entries a fresh lease window. Ready state is preserved while the host
  lease timestamps are refreshed.
- Stable installation IDs permit an explicit JOIN refresh to follow an address
  change. READY and LEAVE additionally require the current packet source
  address, and host-local entries can never match a remote sender.
- Hosts ignore inbound PLAYER_LIST packets.

## Verification

- `:app:assembleDebug :app:testDebugUnitTest` passed. This compiled Kotlin and
  native CMake targets for arm64-v8a, armeabi-v7a, and x86_64 and ran the full
  JVM unit test suite.
- Focused lobby protocol and lifecycle policy tests passed after the final
  changes.
- Scoped `android/run-code-quality.ps1 -Fix` passed for all changed Kotlin,
  PowerShell, and plan files.
- `android/tests/test_lan_lobby_discovery.ps1 -TimeoutSeconds 30` passed across
  two emulators.
- The optional `-ResumeCoverage` phase was run but could not reach JOIN on this
  host: the emulator shared-Wi-Fi network delivered broadcasts, while direct
  device-to-device unicast to 10.0.2.16 was unreachable. Logs confirmed the
  joiner transmitted three JOIN packets and the host received none. The phase
  remains available for a two-device or emulator setup with working unicast and
  covers ready, 12-second idle, 20-second background, resume, and restored
  two-player ready state.
