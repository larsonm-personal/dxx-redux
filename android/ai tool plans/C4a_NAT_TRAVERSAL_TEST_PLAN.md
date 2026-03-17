# C4a NAT Traversal -- Dual-Emulator Test Plan

## Status
- StunClient.kt: DONE (RFC 5389, two-port NAT classification)
- ConnectivityChecker.kt: DONE (probe + echo responder)
- LocalhostProxy.kt: DONE (per-peer UDP proxy, relay wrapping)
- MatchmakingService.kt wiring: DONE (STUN auto-trigger, connectivity check dispatch, relay fallback)
- Server STUN/relay/nat_sim: DONE (75 tests passing)

## Architecture Overview

```
Emulator A (10.0.2.x)         Host (Windows)           Emulator B (10.0.2.x)
  StunClient ----UDP----> STUN :3478/:3479 <----UDP---- StunClient
  ConnChecker --UDP--> [peer addr from server] <--UDP-- ConnChecker
  LocalhostProxy <----> Relay :42424 <--------> LocalhostProxy
       |                    |                       |
  WebSocket  ----------> WS :8080 <------------ WebSocket
```

Both emulators reach the host at 10.0.2.2. The matchmaking server
(WS + STUN + relay) runs on the host.

## Challenge: Same-Host Emulator NAT

Two Android emulators on the same Windows host both appear from the
STUN server's perspective at 127.0.0.1 (or ::1). This means:

1. Both clients get the same reflexive IP from STUN
2. NAT type detection works (different source ports -> different mapped ports)
3. Connectivity probes work IF the emulator's NAT lets inbound UDP through

The Android emulator uses QEMU user-mode networking (SLIRP) which
behaves as a port-restricted NAT. Outbound UDP creates a mapping; the
host can send back to that mapping but only from the original dest addr.

This means direct holepunch between two emulators on the same host is
NOT possible -- they'd need to send to each other's SLIRP-mapped ports,
but those ports are only open to traffic from the original dest addr
(the STUN server). The probes will timeout and fall back to relay.

## Test Scenarios

### 1. STUN Discovery (both clients)
- Start server: `cargo run` (starts WS + STUN + relay)
- Both emulators connect to WS at 10.0.2.2:8080
- Both join the same lobby, triggering STUN discovery
- Verify via introspection or logcat:
  - Each client gets reflexive addresses from both STUN ports
  - NAT type is reported (likely "symmetric" since SLIRP assigns
    different external ports per dest)
  - Server receives STUN_RESULT from both clients

### 2. Connectivity Check (fallback to relay)
- Server sends CONNECTIVITY_CHECK_GO with candidate pairs
- Both clients run ConnectivityChecker.probe()
- Probes will timeout (SLIRP NAT won't pass them through)
- Both clients send CONNECTIVITY_OK with type="relay"
- Server allocates relay session

### 3. Relay Game Launch
- Host starts game -> server sends GAME_STARTING with relay addrs
- Both clients set up LocalhostProxy with relay wrapping
- Engine connects to loopback proxy ports
- Verify relay traffic flows (this requires C4b game launch -- deferred)

### 4. Unit-Level Testing (no emulator needed)
For testing probe+echo logic without the emulator NAT problem:

```kotlin
// Loopback test: two ConnectivityChecker instances on localhost
// Each gets the other's loopback addr as a candidate pair
// Should complete with type="host" and sub-1ms RTT
```

This can be a JVM unit test or an on-device instrumented test.

## Running the Tests

### Prerequisites
- Server running: `cd server && cargo run`
- Two emulators: `emulator -avd emu1 ...` and `emulator -avd emu2 ...`
- App installed on both

### STUN + Connectivity Check Test (manual)
```powershell
# Terminal 1: start server
cd d:\local\dxx-redux\server
cargo run

# Terminal 2: verify emulators
adb devices  # should show two devices

# On each emulator:
# 1. Launch app -> Multiplayer tab
# 2. Connect (server URL should be 10.0.2.2:8080)
# 3. One creates lobby, other joins
# 4. Watch logcat for STUN and connectivity messages:
adb -s emulator-5554 logcat -s StunClient ConnectivityChecker MatchmakingService
adb -s emulator-5556 logcat -s StunClient ConnectivityChecker MatchmakingService
```

### Expected Logcat Output
```
StunClient: Querying STUN server 10.0.2.2:3478...
StunClient: Reflexive addr: 10.0.2.2:xxxxx (port varies)
StunClient: Querying STUN server 10.0.2.2:3479...
StunClient: Reflexive addr: 10.0.2.2:yyyyy (different port = symmetric)
StunClient: NAT type: symmetric, 2 candidates
MatchmakingService: STUN: symmetric, 2 candidates
MatchmakingService: Connectivity check GO: N pairs to probe
ConnectivityChecker: All N pairs timed out after 3000ms
MatchmakingService: No direct connection, will use relay
```

## Automated Bot Test (future)
Extend test_bot_client.ps1 to:
1. Connect two bots to server
2. Both join same lobby
3. Mock STUN results (send STUN_RESULT directly via WS)
4. Verify server sends CONNECTIVITY_CHECK_GO
5. Both send CONNECTIVITY_OK with type="relay"
6. Verify server sends RELAY_ASSIGNED when game starts

This exercises the full server-side NAT flow without needing real UDP.

## Server-Side Test Coverage (already exists)
- `test_connectivity_check_go_after_all_stun`: 2 clients, STUN results, verify GO
- `test_relay_assigned_on_game_start`: relay allocation on game start
- 11 `nat_sim` tests: full NAT simulator with various NAT types
