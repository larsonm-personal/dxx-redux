# Fix test_mp Phase 8: use server relay instead of custom UDP relay

## Root Cause

Phase 8 fails because the test uses a custom UDP relay (udp_relay.ps1) with
emulator port redirections (redir), but the SLIRP NAT in the emulator does
not properly route reply packets from EMU1 back through the relay.

Specifically:
- EMU2 sends UPID_GAME_INFO_REQ to 10.0.2.2:42600 (the relay)
- Relay forwards to 127.0.0.1:42500 -> EMU1 redir -> EMU1 game engine :42424
- EMU1 replies to sender_addr, which is SLIRP-mapped to 10.0.2.2:{random}
- Reply goes to host loopback on a random SLIRP conntrack port, NOT back to
  the relay on port 42600
- Relay never sees the reply -> joiner times out

## Fix

Use the matchmaking server's built-in UDP relay instead of the custom one.
The server already has relay infrastructure and the LocalhostProxy in the
app already knows how to use it. The test just needs to configure the
server with the right relay addresses.

### Changes to test_mp.ps1 Phase 1 (server start):
- Set RELAY_LISTEN_ADDR=127.0.0.1:9001 (bind relay to localhost)
- Set RELAY_PUBLIC_ADDR=10.0.2.2:9001 (emulators reach host at 10.0.2.2)
- Wait for port 9001 to be listening in addition to port 9000

### Changes to test_mp.ps1 Phase 7 (game launch):
- Remove Setup-EmulatorRedir function and redir rules
- Remove custom UDP relay (udp_relay.ps1) startup
- Remove set_join_target override on EMU2
- The MatchmakingService's LocalhostProxy handles everything

### Changes to test_mp.ps1 Phase 8:
- Remove fallback checks that look for relay log (udp_relay.log)
- Simplify introspection-based game entry checks

### Changes to test_mp.ps1 Cleanup:
- Remove relay process cleanup
- Remove relay log dump

### Changes to Start-MatchmakingServer (test_helpers.ps1):
- Add RELAY_LISTEN_ADDR and RELAY_PUBLIC_ADDR env vars

## Status
- [x] Root cause identified
- [ ] Code changes
- [ ] Verified
