# Phase 9: Host Migration PDATA Diagnostics

## Problem
After host migration, the joiner stops receiving PDATA from the host within ~15s.
- RetroProtocol=1, so all existing MPDIAG logging was in wrong (non-RetroProtocol) code paths
- drop_rx_packet uses con_printf (logcat) not MPDIAG (debuglog), so security check failures are invisible
- No send_pdata logging in RetroProtocol path
- No packet-type counting in listen()

## Diagnostic additions

### 1. valid_token: MPDIAG on PDATA token mismatch
- In valid_token(), when UPID_PDATA check fails, add MPDIAG with both token values
- Rate-limited to once/3s

### 2. send_pdata: RetroProtocol TX logging
- In send_pdata, RetroProtocol branch, add MPDIAG for first 3 + every 300th TX
- Show target player index and address

### 3. process_pdata: RetroProtocol entry logging
- At the point after security checks pass and Player_num is extracted
- Show Player_num, data_len, connected state
- Rate-limited

### 4. read_pdata_packet: LastPacketTime update logging
- Right at the LastPacketTime = timer_query() line
- Show TheirPlayernum and confirm update
- Rate-limited

### 5. listen: packet type breakdown
- Instead of just rx_total, count by type (PDATA, MDATA, PING, PONG, other)
- Show in heartbeat

### 6. netgame_token logging
- Log netgame_token value in SYNC send/receive
- Log in GAME_INFO receive
- Log at rebind_for_hosting

## Files to edit
- d2/main/net_udp.c
- d1/main/net_udp.c (same changes)
