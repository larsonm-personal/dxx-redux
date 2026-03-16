# Phase C4: Game Launch from Lobby

## Goal
When host clicks "Start Game" in the lobby, all clients set up localhost UDP proxies
and launch the game engine with auto-join/auto-host parameters, bypassing the engine's
manual multiplayer menus.

## Architecture

```
LobbyScreen -> MatchmakingService -> WebSocket -> Server (StartGame)
                                                      |
                                              GAME_STARTING (per-player)
                                                      |
                                  MatchmakingService.handleGameStarting()
                                              |
                              LocalhostProxy.start(peers)
                                              |
                              Launch MainActivity with intent extras
                                              |
                              JNI: nativeSetAutoJoin/Host
                                              |
                              Engine: check_auto_join() / check_auto_host()
                                              |
                              Engine: normal MP flow (join/host)
```

## Components

### 1. Server: Enhanced GAME_STARTING (protocol.rs + ws_handler.rs)
- Add PeerAssignment struct {slot, addr, is_relay, relay_token, relay_dest_slot}
- Enhance GameStarting with your_slot, peers, difficulty, level_num, max_players
- Reorder StartGame handler: allocate relay BEFORE building GAME_STARTING
- Send per-player GAME_STARTING (not broadcast) so each client gets their own view

### 2. Client: LocalhostProxy.kt
- UDP socket forwarding between game engine and remote peers
- Engine binds to 127.0.0.1:42424, proxy binds per-peer to 127.0.0.1:42430+N
- Direct mode: forward raw UDP
- Relay mode: wrap [token:4LE][dest:1] on send, strip header on receive
- NAT keepalive: 1-byte ping every 15s for direct connections
- Coroutine per peer (2 sub-coroutines: local->real, real->local)

### 3. Client: MatchmakingService GAME_STARTING handler
- Create LocalhostProxy with peer assignments from GAME_STARTING
- Launch game Intent with mp_mode, mp_host_addr, etc.
- Host gets mp_mode=host, joiners get mp_mode=join

### 4. Engine: auto_net.c/h (d2/main/ and d1/main/)
- Globals: auto_join_pending, auto_host_pending, auto_host_addr, auto_host_port, etc.
- check_auto_join(): called from main_menu_handler EVENT_WINDOW_ACTIVATED
  -> net_udp_init(), set ports, fill direct_join, net_udp_manual_join_game_auto()
- check_auto_host(): similar for hosting
- Must bind engine socket to 127.0.0.1 when auto_* pending (loopback only)

### 5. JNI: jni_auto_net.c (in android/app/src/main/cpp/)
- nativeSetAutoJoin(hostAddr, hostPort, myPort)
- nativeSetAutoHost(myPort, mission, mode, maxPlayers, levelNum, difficulty)
- Sets C globals via extern declarations
- Called from MainActivity.startGame() after reading intent extras

### 6. MainActivity: intent extras -> JNI
- Read mp_mode, mp_host_addr, mp_host_port, mp_mission etc. from intent extras
- Call nativeSetAutoJoin or nativeSetAutoHost before engine starts

### 7. Engine: loopback binding in net_udp.c (d1+d2)
- In udp_open_socket(): when auto_join_pending or auto_host_pending,
  bind to INADDR_LOOPBACK instead of INADDR_ANY
- Prevents accidental direct internet connections

## Port Constants
- ENGINE_PORT = 42424 (engine's UDP socket)
- PROXY_PORT_BASE = 42430 (first proxy socket, +1 per peer)

## Status
- [x] Server GAME_STARTING enhancement (per-player PeerAssignment, game field)
- [x] Server allocate_relay_session refactor (returns Option<u32>)
- [x] LocalhostProxy.kt
- [x] MatchmakingService GAME_STARTING handler (proxy setup + GameLaunchInfo)
- [x] Client protocol updated (GameStartingMsg, PeerAssignment, LobbyInfo with game)
- [x] auto_net.c/h d2
- [x] auto_net.c/h d1
- [x] net_udp_auto_join/auto_host d2 + d1
- [x] menu.c hooks d2 + d1
- [x] JNI bridge (nativeSetAutoJoin, nativeSetAutoHost in jni_main.c)
- [x] MainActivity intent extras reading
- [x] LobbyScreen game launch trigger (LaunchedEffect, callback chain)
- [x] Mode string-to-int mapping (NetworkConstants.gameModeToInt)
- [x] Game field threaded through server+client protocol
- [x] Loopback binding (udp_bind_loopback in d1+d2 net_udp.c)
- [x] CMakeLists.txt updates d1 + d2
- [x] Android/Gradle build verification (assembleDebug passes)
- [x] Server build + tests (74 tests: 5 unit + 58 integration + 11 NAT sim)
- [x] Linters (clippy, ktlint, clang-format)
