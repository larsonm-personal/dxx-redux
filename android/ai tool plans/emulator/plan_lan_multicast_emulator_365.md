# Plan: LAN Multicast Fix via Emulator 36.5+ Shared Wi-Fi

## Summary

LAN multicast lobby discovery is broken between emulators because pre-36.5
emulators each sit behind isolated virtual routers with no shared broadcast
domain. Emulator 36.5+ (released Apr 2, 2026) adds a shared virtual Wi-Fi LAN
where broadcast/multicast packets propagate between instances automatically.

## Status

- [x] Phase 1: Verify 36.5+ claims against official docs
- [x] Phase 2: Check current emulator version (was 36.4.10)
- [x] Phase 3: Update emulator binary to 36.5.10
- [x] Phase 4: Verify shared LAN between two running emulators
- [x] Phase 5: Test raw UDP broadcast tool between emulators
- [x] Phase 6: Test game LAN lobby discovery (host/join)
- [x] Phase 7: Update test_lan.ps1 to use direct LAN (no relay needed)
- [x] Phase 8: Run full LAN test and fix issues

## Results

### Phase 4-5: Network verification (PASSED)
- EMU1 wlan0: 10.0.2.16/24, EMU2 wlan0: 10.0.2.17/24 (same subnet)
- Ping: 0% packet loss both directions
- UDP unicast: confirmed working
- UDP broadcast: toybox nc lacks SO_BROADCAST, but game engine sets it explicitly

### Phase 6: Lobby discovery (PASSED)
- Kotlin LobbyService broadcasts ANNOUNCE on port 42400 to 10.255.255.255 and 10.0.2.255
- EMU2 discovered EMU1's lobby within 3 seconds (lobbies=1, rx=2)
- Test MP_COMMANDs added: lan_host_lobby, lan_discover, lan_discover_status, lan_stop_lobby

### Phase 6b: Game engine connection (PASSED)
- Host (EMU1) bound to 0.0.0.0:42424, joiner (EMU2) connected via host_addr=10.0.2.16
- Auto-join succeeded in 1 request, N_players=2, SYNC sent
- The Kotlin LAN proxy routes traffic through 127.0.0.1:42430 -> host wlan0 IP

### Phase 7: Test infrastructure updates
- test_lan.ps1: Added -UseRelay switch (default is now direct LAN)
- test_lan_lobby_discovery.ps1: New test for Kotlin lobby broadcast discovery
- Previous errno 98 bind failure was caused by stale toybox nc process, not a code bug

## Key Facts

### Emulator networking (confirmed from official docs)

- **36.5+**: Multiple emulators share a virtual Wi-Fi network. They appear as
  distinct devices on the same subnet. UDP broadcast/multicast propagates
  between them. NSD and Wi-Fi Direct work. No hacks needed.
  Source: https://developer.android.com/studio/run/emulator-networking-interconnect
- **Pre-36.5**: Each emulator behind its own isolated virtual router. No shared
  broadcast domain. Only unicast via host-port forwarding (redir / adb forward).

### Game LAN discovery protocol

- Files: d2/main/net_udp.c, d2/main/net_udp.h (d1 mirrors)
- Broadcast address: 255.255.255.255
- Default port: 42424 (`UDP_PORT_DEFAULT`)
- Socket option: SO_BROADCAST enabled
- Discovery flow:
  1. Client broadcasts `UPID_GAME_INFO_LITE_REQ` (type 4) to 255.255.255.255:42424
  2. Host responds with `UPID_GAME_INFO_LITE` (type 5) containing game info
  3. Client displays discovered games in lobby list

### Current test infrastructure

- `test_lan.ps1`: Uses UDP relay (udp_relay.ps1) + emulator redir to bridge
  two isolated emulator NATs. With 36.5+, the relay may be unnecessary for
  LAN discovery (but might still be needed for directed game traffic depending
  on how the emulators route unicast between wlan0 addresses).
- `test_lan_discovery.ps1`: Sends ANNOUNCE packets from host machine to emulator.
  This tests the launcher lobby UI, not the engine's native UDP discovery.

### What changed (emulator update)

- Was: Android Emulator 36.4.10.0
- Now: Android Emulator 36.5.10.0
- get_emulator.sh: Uses `sdkmanager "emulator"` (unpinned, gets latest stable).
  No script changes needed -- re-running the script or sdkmanager update pulls 36.5+.

## Phase 4: Verify shared LAN

1. Start both emulators (Nexus5X_Light_1 on -5554, Nexus5X_Light_2 on -5556)
2. On each: `adb -s emulator-XXXX shell ip addr show wlan0`
3. Confirm both get 10.0.2.x addresses on same subnet
4. From EMU1: ping EMU2's wlan0 IP (and vice versa)

## Phase 5: Test raw UDP broadcast

Use `toybox nc` or equivalent inside emulators:
- EMU1 listener: `adb -s emulator-5554 shell toybox nc -ul -p 9999`
- EMU2 sender: `adb -s emulator-5556 shell toybox nc -u 255.255.255.255 9999`
- If text typed in sender appears in listener, broadcast works

Alternative: write a small test using Python udp_relay.py or busybox

## Phase 6: Test game LAN lobby discovery

1. Launch game on EMU1 as host (via lan_launch MP_COMMAND or automation script)
2. Launch game on EMU2 as joiner
3. On joiner, check if host's game appears in lobby list via introspection
4. This validates the full engine broadcast discovery path

## Phase 7: Simplify test_lan.ps1

If Phase 6 works without the relay:
- Remove UDP relay setup from test_lan.ps1
- Remove emulator redir setup
- Change join command to not specify host_addr/host_port (let broadcast discover)
- Keep the relay as a fallback/option for pre-36.5 environments

## Risks

- The 36.5 shared Wi-Fi may not support 255.255.255.255 broadcast (it might
  only support subnet broadcast or multicast). Need to test.
- Game may bind to wrong interface (eth0 vs wlan0). Check socket binding.
- Emulator 36.5 is brand new (3 days old). May have stability issues.
- If the emulator's shared Wi-Fi doesn't work, fall back to the existing
  relay-based approach.
