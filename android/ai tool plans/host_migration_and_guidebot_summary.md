# Host Migration and Guidebot Ownership Transfer

## Overview
Android coop games support seamless host swapping: when the current host
leaves, the lowest-numbered remaining player becomes the new host, the
Kotlin proxy layer reconfigures to accept incoming connections, and
disconnected players can rejoin. The guidebot's ownership automatically
transfers when its owner disconnects.

## Host Migration Flow

### 1. Detection (C engine, multi.c)
When a player disconnects and `pnum == multi_who_is_master()`:
- Elect new master: lowest-numbered `CONNECT_PLAYING` player
- Set `Multi_master_playernum = new_master`
- If this player is the new master:
  - Bootstrap master state (object_owner, powerup caps)
  - Write `host_migration.json` with game params (callsign, mission, etc)
  - Call `android_notify_host_migration()` via JNI

### 2. Kotlin reconfiguration (SetupActivity, MatchmakingService, LobbyService)
- `onHostMigration()` in MainActivity broadcasts `com.dxxredux.HOST_MIGRATION`
- SetupActivity receiver:
  - Creates a host-mode proxy via `MatchmakingService.createProxy(listenPort=42425)`
  - Reads `host_migration.json` for game details
  - Starts LAN lobby broadcast via LobbyService so other players can discover
  - Sets `LobbyService.startGame(difficulty, levelNum, proxyPort)` to mark as in-game

### 3. Proxy architecture
- Engine stays bound to `127.0.0.1:42424` (loopback) after migration
- `LocalhostProxy` with `allowDynamicPeers=true` listens on port 42425
- Incoming client connections get dynamic peer slots (42430+N)
- Each peer gets a dedicated local port so the engine sees unique addresses

### 4. Client rejoin
- Disconnected players discover the new host via LAN broadcast
- `auto_join()` connects through the proxy to `127.0.0.1:42425`
- Host receives UPID_REQUEST, calls `welcome_player` for reconnecting players
- `welcome_reconnect` sets `connection_statuses[player].type = CONNT_DIRECT`
- Object sync + SYNC packet sent to complete rejoin

### 5. isyou address collision fix
- SYNC packets mark slots as `isyou` via address comparison
- After migration, the new host's stale proxy address can match the client's
  proxy address (both 127.0.0.1:42430), making `isyou=1` for ALL slots
- Fix: on Android, `read_sync_packet` uses `i != Player_num` instead of
  `!isyou` to gate connection type setup. Desktop retains original `isyou`

### 6. Master slot in SYNC packets
- `net_udp_process_game_info` reads `master_slot` from the packet body
  (Android extension) so clients know which slot is the master even when
  the master isn't slot 0

## Guidebot Ownership Transfer

### Data
- `Escort_owner_player`: which player slot controls the guidebot
- `Objects[Buddy_objnum].ctype.ai_info.REMOTE_OWNER`: per-object owner field

### Transfer on disconnect (escort.c)
`escort_transfer_ownership_on_disconnect(gone_pnum)`:
- Called from `multi_make_player_ghost` and the host migration block
- Picks lowest-numbered connected player as new owner
- Sets both `Escort_owner_player` and `REMOTE_OWNER`
- Sends `MULTI_ESCORT_OWNER` packet to all players

### Voluntary release (escort.c)
`escort_release_control()`:
- Player releases guidebot control via menu
- Picks a random connected player and transfers ownership

### Key files
- `d2/main/multi.c`: host migration trigger, `Multi_master_playernum`
- `d2/main/escort.c`: guidebot ownership functions
- `d2/main/net_udp.c`: `auto_join`, `welcome_player`, `read_sync_packet`,
  `send_sync`, CONNTYPE fixes
- `android/app/src/main/cpp/jni_main.c`: `android_notify_host_migration` JNI
- Kotlin: `SetupActivity.kt` (migration receiver), `MatchmakingService.kt`
  (proxy creation), `LobbyService.kt` (LAN broadcast), `LocalhostProxy.kt`
  (dynamic peer proxy)
- D1 has identical C-side changes (multi.c, net_udp.c) but no guidebot
