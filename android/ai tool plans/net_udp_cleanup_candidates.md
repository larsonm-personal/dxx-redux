# net_udp.c cleanup candidates -- host migration diagnostics

File: `d2/main/net_udp.c`

## Category A: DEFINITELY REMOVE (concluded investigations, per-packet noise)

### A1. `retro_tx_count` PDATA send diagnostic (lines 7507-7523)
```c
#ifdef __ANDROID__
    /* android port: diagnose host-migration PDATA loss */
    {
        static int retro_tx_count = 0;
        retro_tx_count++;
        if (retro_tx_count <= 3 || retro_tx_count % 50 == 0) {
            MPDIAG("send_pdata: RETRO TX #%d from P%d token=%u len=%d", ...);
            for (int j = 0; j < MAX_PLAYERS; j++) {
                if (Players[j].connected && j != Player_num)
                    MPDIAG("  send_pdata: -> P%d addr=%s:%u conntype=%d", ...);
            }
        }
    }
#endif
```
Rationale: fires every 50th PDATA send in retro mode, logging destination addresses for every connected player. PDATA loss investigation is closed. Remove entire block.

### A2. Non-retro master PDATA TX counter (lines 7535-7544)
```c
#ifdef __ANDROID__
    /* android port: diagnose host-migration PDATA loss */
    {
        static int pdata_tx_count = 0;
        if (++pdata_tx_count <= 3 || pdata_tx_count % 300 == 0)
            MPDIAG("send_pdata: TX #%d to player[%d] at %s:%u", ...);
    }
#endif
```
Rationale: fires every 300th PDATA send. Investigation concluded.

### A3. Non-retro client PDATA TX counter (lines 7551-7559)
```c
#ifdef __ANDROID__
    /* android port: diagnose host-migration PDATA loss */
    {
        static int pdata_tx_count = 0;
        if (++pdata_tx_count <= 3 || pdata_tx_count % 300 == 0)
            MPDIAG("send_pdata: TX #%d to master[%d] at %s:%u", ...);
    }
#endif
```
Rationale: same as A2 but for client->master.

### A4. process_pdata RX counter (lines 7739-7748)
```c
#ifdef __ANDROID__
    /* android port: diagnose host-migration PDATA loss */
    {
        static int pdata_rx_count = 0;
        if (++pdata_rx_count <= 3 || pdata_rx_count % 300 == 0)
            MPDIAG("process_pdata: RX #%d from P%d connected=%d len=%d retro=%d sender=%s:%u", ...);
    }
#endif
```
Rationale: per-packet RX diagnostic (every 300th). Investigation concluded.

### A5. process_pdata "DROP status" diagnostic (lines 7676-7687)
```c
#ifdef __ANDROID__
    /* android port: diagnose host-migration PDATA loss */
    {
        static fix64 last_drop_log = 0;
        fix64 now = timer_query();
        if (now > last_drop_log + F1_0*3) {
            MPDIAG("process_pdata: DROP status (Network_status=%d Game_mode=0x%x)", ...);
            last_drop_log = now;
        }
    }
#endif
```
Rationale: logs when PDATA arrives during non-playing status (every 3s). Normal during level transitions, just noise.

### A6. process_pdata "DROP addr" diagnostic (lines 7719-7729)
```c
#ifdef __ANDROID__
    /* android port: diagnose host-migration PDATA loss */
    {
        static fix64 last_addr_log = 0;
        fix64 now = timer_query();
        if (now > last_addr_log + F1_0*3) {
            MPDIAG("process_pdata: DROP addr (slot=%d sender=%s:%u expected=%s:%u)", ...);
            last_addr_log = now;
        }
    }
#endif
```
Rationale: logs address mismatches on PDATA (every 3s). The investigation is concluded and the sockaddr_equal fix is in place.

### A7. process_pdata "DROP size" diagnostic (lines 7696-7700)
```c
#ifdef __ANDROID__
    MPDIAG("process_pdata: DROP size (got=%d short=%d expected=%d)", ...);
#endif
```
Rationale: fires on every size-mismatched packet. No throttling.

### A8. read_pdata "SKIP" diagnostic (lines 7905-7915)
```c
#ifdef __ANDROID__
    /* android port: diagnose host-migration PDATA loss */
    {
        static fix64 last_skip_log = 0;
        fix64 now = timer_query();
        if (now > last_skip_log + F1_0*3) {
            MPDIAG("read_pdata: SKIP P%d connected=%d Player_num=%d", ...);
            last_skip_log = now;
        }
    }
#endif
```
Rationale: logs when a player's PDATA is skipped because they're not CONNECT_PLAYING. Throttled but concluded investigation.

### A9. valid_token PDATA/MDATA drop diagnostic (lines 1103-1118)
```c
#ifdef __ANDROID__
    /* android port: diagnose host-migration PDATA loss */
    if (pid == UPID_PDATA || pid == UPID_MDATA_PNORM) {
        static fix64 last_tok_log = 0;
        fix64 now = timer_query();
        if (now > last_tok_log + F1_0*3) {
            MPDIAG("valid_token: DROP %s pkt_token=%u local_token=%u", ...);
            last_tok_log = now;
        }
    }
#endif
```
Rationale: logs token mismatches for PDATA/MDATA (every 3s). The base `drop_rx_packet` already logs this. Redundant.

### A10. Commented-out memcpy lines (lines ~2900, ~2915)
```c
//memcpy(&Netgame.players[i].protocol.udp.addr, &p->player.protocol.udp.addr, sizeof(struct _sockaddr));
//memcpy( (struct _sockaddr *)&Netgame.players[N_players].protocol.udp.addr, ...
```
Rationale: replaced by `update_address_for_player()`. Dead code comments should be removed.

---

## Category B: CONSIDER REMOVING (verbose but has some value)

### B1. net_udp_listen rx_total/heartbeat counters (lines 6395-6490)
The entire per-socket rx_pdata/rx_mdata/rx_ping/rx_other counting + the every-5-second heartbeat:
```c
static int rx_total = 0; rx_pdata = 0; ...
...
MPDIAG("listen: rx_total=%d pdata=%d mdata=%d ...", ...);
if (rx_pdata == 0 && rx_mdata > 0 ...)
    MPDIAG("listen: WARNING pdata=0 mdata=%d -- possible proxy issue...", ...);
```
Rationale: The proxy-issue WARNING is valuable for future debugging, but the general rx counter heartbeat (every 5s) is noisy. Consider: **keep only the WARNING branch**, remove the counters and regular heartbeat.

### B2. read_sync_packet player address dump (lines 5327-5338)
```c
MPDIAG("read_sync: PLAYING Player_num=%d master=%d ...", ...);
for (int i = 0; i < N_players; i++) {
    MPDIAG("read_sync: player[%d] addr=%s:%u connected=%d%s", ...);
}
```
Rationale: fires once per level sync. Somewhat useful but dumps addresses. Consider keeping the summary line and removing the per-player loop.

### B3. send_sync [ANDROID] net_log_comment blocks (lines 5370-5438)
Many `net_log_comment("[ANDROID] send_sync: ...")` lines:
- L5355: "send_sync FAILED: not enough start positions"
- L5373: "send_sync: N_players=%d, sending SYNC to all clients"
- L5416-5420: per-player "sending SYNC to player %d"
- L5432: "sent sync to all clients, processing own copy"
- L5438: "completed successfully"

Rationale: these fire once per game start. Low volume. Consider keeping the error case (5355) and removing the success-path logging (5373, 5416, 5432, 5438).

### B4. level_sync [ANDROID] net_log_comment blocks (lines 6186-6245+)
Heavy net_log_comment blocks in level_sync:
- L6190: "level_sync START: N_players=..."
- L6197: "awaiting sync as client"
- L6204: "host waiting for client requests"
- L6208-6213: wait_for_requests returned
- L6215: "sending sync to all clients"
- L6220-6224: send_sync returned
- L6228: "client waiting for sync from host"
- L6236: "level_sync END: result=..."
- L6241: "level_sync FAILED: disconnecting"

Rationale: fires once per level transition. Low volume. Could consolidate to 2-3 lines.

### B5. sync_poll [ANDROID] net_log_comment blocks (lines 4115-4150)
- L4115: "sync_poll: host disconnected!"
- L4124: "Network_status changed to PLAYING, exiting"
- L4140-4145: "timeout waiting for sync, resending request"
- L4150: "net_udp_send_request failed on retry!"

Rationale: fires during level sync polling. Low volume. Error cases are valuable. Consider keeping errors only.

### B6. wait_for_sync [ANDROID] net_log_comment blocks (lines 5992-6044)
- L5992: "wait_for_sync START"
- L6002: "net_udp_send_request failed!"
- L6014: "entering menu loop"
- L6025: "wait_for_sync: menu exited with Network_status=..."
- L6031: "FAILED: not in PLAYING status"
- L6044: "OK: sync received"

Rationale: fires once per level. Could keep only error path.

### B7. send_objects player/ghost count diagnostic (lines 2547-2557)
```c
// Count player/ghost objects on host for diagnostic comparison
int pg_count = 0;
for (hi = 0; hi <= Highest_object_index; hi++)
    if (Objects[hi].type == OBJ_PLAYER || Objects[hi].type == OBJ_GHOST)
        pg_count++;
MPDIAG("send_objects: finished, obj_count=%d player_ghost_on_host=%d Highest=%d\n", ...);
```
Rationale: fires once per sync. Not Android-only. Low volume but the counting loop is unnecessary overhead even if small.

---

## Category C: KEEP (useful operational logging, functional code, or state-change logging)

### C1. CONNTYPE state-change logging (10 locations)
All `MPDIAG("CONNTYPE[...]: P%d %d->DIRECT/PROXY", ...)` at:
- L5255, 5260, 5265 (read_sync_packet)
- L5426 (send_sync)
- L6539 (timeout_check)
- L8195 (p2p_pong)
- L8237 (resetProxy)
Keep: state-change logging is cheap and critical for diagnosing connection type bugs.

### C2. welcome_player MPDIAG logging (lines 2100-2130)
The MPDIAG calls in welcome_player for entry, rejection reasons, and busy:
- "welcome_player: '%s' connected=%d ..."
- "welcome_player: REJECTED endlevel"
- "welcome_player: IGNORED busy"
- "welcome_player: REJECTED level mismatch"
Keep: fires only on join attempts, essential for diagnosing join failures.

### C3. auto_join MPDIAG logging (lines 5780-5862)
MPDIAG calls in net_udp_auto_join:
- socket/bind status
- request counts (throttled: first 3 then every 10th)
- version mismatch, timeout, success
Keep: fires during join flow only, essential for debugging Android launcher.

### C4. timeout_check MPDIAG (line ~6530)
```c
MPDIAG("timeout_check: player %d timed out (%.1fs ago)\n", ...);
```
Keep: fires only when a player actually times out.

### C5. Functional Android code (NOT diagnostics)
- `sockaddr_equal()` (L183-216): essential fix for struct padding
- `find_player_by_identity()` (L247-278): essential for reconnection
- `udp_bind_loopback` logic (L607-703): essential for proxy architecture
- `net_udp_rebind_for_hosting()` (L675-703): essential for host migration
- `net_udp_auto_join()` / `net_udp_auto_host()` (L5761-5918): launcher integration
- `isyou` address collision fix (L5244-5249): critical bug fix
- master slot in packet (L3397, L3661): host migration protocol
- deferred sender address storage (L3537-3541): host migration fix
- Player_num collision fix in do_join_game (L6319-6324): host migration fix

### C6. drop_rx_packet and valid_sender
Keep all: these are security validation, not diagnostics.

### C7. add_player duplicate callsign logging (L2922-2928)
Keep: fires only on duplicate callsign, important for debugging name conflicts.

### C8. start_game net_log_comment (L5924)
Keep: fires once, very cheap.

---

## Summary counts
- **A (Definitely remove)**: 10 items (~120 lines of diagnostic code)
- **B (Consider removing)**: 7 items (~80 lines of net_log_comment/MPDIAG blocks)
- **C (Keep)**: 8+ categories of functional/useful code
