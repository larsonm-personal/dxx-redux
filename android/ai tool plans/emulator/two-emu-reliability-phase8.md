# Plan: Two-Emulator Multiplayer Test Reliability + Phase 8 Fix

## Problem Summary

Phase 8 of run_mp_test.ps1 fails: host stays on "select players" screen while joiner
enters level loading, then times out with "missing ACKs" or similar.

Two observed failure modes:
- Run 1: 300+ relay packets (all sampled ones after #5 were EMU2->EMU1 only), test timed out
- Run 2: Only 5 relay packets (handshake only), communication died immediately

## Root Cause Analysis

### Why the host stays on select_players

The auto-start condition in net_udp_start_poll checks:
  `auto_host_pending && N_players >= Netgame.max_numplayers`

For this to fail, either:
1. The joiner's UPID_REQUEST never reaches the host (relay issue)
2. net_udp_add_player fails to increment N_players (duplicate address match?)
3. UPID_REQUEST passes through relay but is dropped by pass_security_check

Analysis of pass_security_check: UPID_REQUEST has NO token check, NO netgame_status
check, and size check is 35 bytes (matches relay log). It should pass.

Most likely cause: the initial GAME_INFO_REQ/GAME_INFO handshake overwrites the joiner's
stored host address. In net_udp_process_game_info (the joiner side), the game replaces
Netgame.players[0].protocol.udp.addr with the sender address of the GAME_INFO reply.
But INSIDE EMU2, the sender of that reply is the NAT gateway (10.0.2.2:something), NOT
the relay address (10.0.2.2:42600). So after receiving GAME_INFO, the joiner might be
sending subsequent UPID_REQUEST to a wrong address that doesn't route through the relay.

Need to verify: does net_udp_process_game_info overwrite players[0].addr?

### Relay routing

- EMU2 sends to 10.0.2.2:42600 (relay). This works.
- Relay forwards to 127.0.0.1:42500 (EMU1 redir). This works.
- EMU1 receives, sender_addr is NAT gateway address. EMU1 responds to sender_addr, which
  goes back through NAT to 127.0.0.1:42600 (relay). This works for the GAME_INFO reply.
- Relay stores emu1_redir_addr from the first host reply and forwards to emu2_nat_addr.
- When joiner sends UPID_REQUEST, relay forwards correctly -> host receives it.
- Host calls net_udp_add_player which stores the sender_addr as the joiner's address.
- After auto-start, host sends UPID_SYNC to that stored address. Should route through
  the same NAT path back to relay. But we need to verify this.

### Possible game_info overwrite issue

In the joiner's flow:
1. auto_join sets Netgame.players[0].addr = relay address (10.0.2.2:42600)
2. Joiner sends GAME_INFO_REQ to that address -> works
3. Host replies with GAME_INFO
4. net_udp_process_game_info is called on the joiner
5. Does it overwrite Netgame.players[0].addr with game_addr (sender of reply)?
   If yes, and the sender is 10.0.2.2:NAT_PORT (not 42600), then all future
   UPID_REQUESTs go to the wrong address, bypass the relay, and are lost!

Must read net_udp_process_game_info to confirm.

## Plan

### Part 1: Emulator hints in copilot-instructions.md
- Add notes about: fresh emulators lose app data, -no-snapshot-save flag,
  terminal buffer corruption with long adb sessions, killing zombie processes

### Part 2: Reliability improvements
- Add health check helper to run_mp_test.ps1 for verifying emulators before test start
- Add more diagnostic logging in the Phase 8 failure path
- Always log relay packets with direction + size + port info for debugging

### Part 3: Fix Phase 8
- Read net_udp_process_game_info to check if it overwrites players[0].addr
- If so, fix: after calling do_join_game, re-set the host address to the relay address
- OR: fix in net_udp_process_game_info to preserve the original connect address
- Add con_printf logging in auto_join/auto_host to trace address changes
- Rebuild and test
