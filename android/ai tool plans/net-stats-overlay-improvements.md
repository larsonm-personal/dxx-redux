# Plan: Net Stats Overlay Improvements + MP Fixes

## Summary
Improve the multiplayer net stats overlay (transparency, graph scrolling, packet
counters, dropped-packet dots, auto-close, IP display) and fix the join-by-IP
exit button.  Changes are mostly Kotlin (MultiplayerStatsOverlay.kt,
MainActivity.kt) with one new JNI call for C-side packet/loss data.

Also documents join-in-progress behavior (no code changes needed for that).

## Files to modify

| File | Changes |
|------|---------|
| `MultiplayerStatsOverlay.kt` | transparency, graph direction, packet display, loss dots, IP |
| `MainActivity.kt` | wire new providers, auto-hide overlay on menu return, IP |
| `jni_main.c` | new `nativeGetMultiplayerPacketStats()` JNI |
| `d2/main/net_udp.c` | fix ESC handling in `manual_join_game_handler()` |
| `d1/main/net_udp.c` | same fix as d2 |

---

## Phase 1: Overlay Visual Fixes (Kotlin only)

### 1.1 -- Increase transparency (~2x more transparent)
- `bgPaint.color`: `0xCC000000` (80% opaque) -> `0x66000000` (40% opaque)
- `graphBgPaint.color`: `0x44000000` -> `0x22000000`

### 1.2 -- Graph: always scroll right-to-left, new data at right edge
- Current: plots N points spread across full graph width starting from the left
- Change: right-align data so first sample is at the right edge, grows left
- x = graphLeft + ((GRAPH_SAMPLES - count + j) / (GRAPH_SAMPLES - 1)) * graphW

---

## Phase 2: Packet Stats via JNI

### 2.1 -- New JNI function `nativeGetMultiplayerPacketStats()`
- File: `jni_main.c`
- Returns int[18]: [UDP_num_sendto, UDP_num_recvfrom, loss[0..7], rx_loss[0..7]]
- Uses existing C globals; works for both d1 and d2

### 2.2 -- Wire to overlay
- File: `MainActivity.kt`
- Add native declaration + set `packetStatsProvider` on overlay

### 2.3 -- Update overlay to use engine packet counts
- Always show "Pkts: Ntx / Nrx" from engine counters (fixes "--")
- When proxy stats also available, show "(proxy: Ptx/Prx)" suffix
- Show "Loss: X%" when any peer has loss > 0

### 2.4 -- Red dots for loss in graph
- Parallel ring buffer `lossHistory[GRAPH_SAMPLES]` (max loss% per sample)
- Red filled circles at graph bottom at each x where loss > 0
- Dots scroll right-to-left with ping line

---

## Phase 3: Auto-Close Overlay on Menu Return
- `MainActivity.kt` overlayPoller: when `gameStarted && !inGame`, hide overlay
- Currently only hides in `!gameStarted` branch

---

## Phase 4: Show "My IP" in Overlay
- Add `localIp: String?` property to overlay
- Set from device network interfaces (LAN) or relay info (online)
- Render right-aligned on the title line

---

## Phase 5: Fix Exit Button During Join-by-IP
- Android exit button injects ESC + SDL_QUIT
- `manual_join_game_handler()` catches ESC while connecting, clears flag, returns 1
  (consumed) -- menu stays open, SDL_QUIT blocked by open menu
- Fix: also close the menu window when ESC cancels the connecting state
- Apply to both d2/main/net_udp.c and d1/main/net_udp.c

---

## Phase 6: Join-in-Progress (Research, no code changes)

Joining in progress IS supported by the engine under these conditions:
- Game is NETSTAT_PLAYING and not closed (NETGAME_FLAG_CLOSED not set)
- RefusePlayers = 0 (open game): new players join freely if slots available
- RefusePlayers = 1 (restricted): host sees HUD prompt, 12s to accept via F6
- Reconnecting: previously-connected player (matching callsign+IP) can always rejoin
- Observer: always allowed regardless of RefusePlayers
- Game full = blocked (numconnected == max_numplayers)
- Default is open

Note: the user reports join-by-IP and join-in-progress do not work in practice.
The exit button fix in Phase 5 may resolve join-by-IP issues. If
join-in-progress still fails, further investigation of the Android network
routing (proxy vs direct, port mapping, timing) is needed in a follow-up.

---

## Verification
1. Android APK build (d1 + d2) -- no compile errors
2. `android/run-code-quality.ps1 --fix`
3. Windows cmake build of d1/ and d2/ -- no regression from net_udp.c changes
4. Manual test overlay: transparency, graph direction, packets, auto-close, IP
5. Manual test: join by IP, press exit during "Connecting..." -- should quit
