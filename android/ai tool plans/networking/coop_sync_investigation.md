# Coop Sync Investigation

## Status: IN PROGRESS

## Summary of Findings

### Networking layer: WORKING
- Relay forwards packets continuously (log caps at count<100, actual throughput ~40+ pkt/sec)
- Connection sustained 90s in automated test (3 WS ping intervals), PASSED
- Manual test server log shows 2.8 minutes of gameplay before user-initiated disconnect
- Proxy header handling verified correct (strips 5-byte header on receive, prepends on send)
- TLS (wss://) works, pipe deadlock fix confirmed

### Callsign corruption bug: CONFIRMED
- At ~88s idle coop gameplay, EMU2's Players[0].callsign changes from "HostPlt" to "JoinPlt"
- Persists at 94s -- not transient
- EMU1 stays correct throughout
- This suggests broader player data corruption on the joiner's machine

#### Most likely cause
- `net_udp_read_sync_packet()` copies Netgame.players[].callsign -> Players[].callsign for ALL players
- If `Netgame.players[0].callsign` gets corrupted (e.g. via misaligned parse in
  `net_udp_process_game_info()`), the corruption propagates to `Players[0]`
- `net_udp_resend_sync_due_to_packet_loss()` sends repeated SYNCs if `VerifyPlayerJoined` doesn't clear
- RetroProtocol conditional parsing adds sizeof(struct _sockaddr) per player -- if send/receive
  disagree on RetroProtocol state, parse offsets shift and callsign data reads from wrong position

### Shield/energy sync in coop: OBSERVER-ONLY (by design)
- `multi_send_ship_status()`, `multi_send_damage()`, `multi_send_repair()` are observer-only
- Only `multi_send_score()` syncs between regular coop peers
- "No shield/energy updates" across clients IS expected behavior in 2-player coop
- Weapon fire (MULTI_FIRE) SHOULD still sync between all peers -- needs verification

## Next Steps
- [ ] Add con_printf in net_udp_read_sync_packet() to log when it runs during NETSTAT_PLAYING
- [ ] Log Netgame.players[0].callsign after net_udp_process_game_info() returns
- [ ] Check VerifyPlayerJoined / SYNC resend timing
- [ ] Verify MULTI_FIRE works in coop (weapon fire visible on other screen)
- [ ] Check RetroProtocol value consistency between host and joiner
