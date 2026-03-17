# Two-Player Multiplayer Integration Test

## Goal
Run two Android emulator instances + a localhost matchmaking server. Player1 creates
a lobby, Player2 joins, they exchange chat messages, Player1 starts the game, and both
end up in-game in a cooperative multiplayer session. Verify via introspection that both
players are connected, have correct callsigns, and are on the right level.

## Architecture

```
  Host Machine (Windows)
  +-----------------------+
  |  Matchmaking Server   |   cargo run (localhost:9000)
  |  (Rust, ws+relay)     |
  +-----------+-----------+
              |  ws://10.0.2.2:9000/ws
   +----------+----------+
   |                      |
+--+--+              +----+---+
| EMU1|              | EMU2   |
| 5554|              | 5556   |
| Host|              | Joiner |
+-----+              +--------+
```

Both emulators reach the same server via 10.0.2.2 (Android's host loopback alias).
Each gets a unique dev token (UUID per app process).

## Test Phases

### Phase 0: Infrastructure
- Create a second AVD (Pixel_6b_API_34 or clone)
- Script to start both emulators, install APK on both, push game data to both
- Start the matchmaking server in the background

### Phase 1: Matchmaking & Lobby (via broadcast commands -- NEW)
Need new broadcast-based commands in SetupActivity to drive multiplayer:
- `MP_CONNECT`: Connect MatchmakingService to server with given callsign
- `MP_CREATE_LOBBY`: Create a lobby with game/mission/mode/maxPlayers
- `MP_JOIN_LOBBY`: Join a lobby by ID (or first available)
- `MP_CHAT`: Send a chat message
- `MP_START_GAME`: Host starts the game
- `MP_INTROSPECT`: Dump matchmaking state to a JSON file for scripted checks

### Phase 2: Game Launch
The existing GAME_STARTING flow handles this:
- Server sends GAME_STARTING to both players
- LocalhostProxy sets up UDP forwarding
- MainActivity launches with mp_mode intent extras
- Engine auto_net handles join/host

### Phase 3: In-Game Verification (introspection -- NEW)
Add multiplayer fields to game_introspect.c:
- `multiplayer.active`: bool (Game_mode & GM_NETWORK)
- `multiplayer.num_players`: N_players count
- `multiplayer.num_connected`: Netgame.numconnected
- `multiplayer.game_name`: Netgame.game_name
- `multiplayer.mission`: Netgame.mission_title
- `multiplayer.gamemode`: Netgame.gamemode (int)
- `multiplayer.gamemode_name`: string (anarchy/cooperative/etc)
- `multiplayer.level_num`: Netgame.levelnum
- `multiplayer.my_player_num`: Player_num
- `multiplayer.network_status`: Network_status (0-5)
- `multiplayer.players[]`: array of {callsign, connected, slot}

## New Code Needed

### 1. Multiplayer introspection (d2/introspect/game_introspect.c + d1/)
Add multiplayer section to JSON output when Game_mode & GM_NETWORK.

### 2. Matchmaking broadcast commands (SetupActivity.kt)
New broadcast action `com.dxxredux.MP_COMMAND` with `--es command <cmd>`:
- connect: --es callsign <name>
- create_lobby: --es game d2 --es mission "Counterstrike!" --es mode cooperative --ei max_players 4
- join_first_lobby: (joins the first available lobby)
- chat: --es text "hello"
- start_game: (host starts)
- introspect: dumps matchmaking state to mp_state.json
- set_ready: toggle ready state

### 3. Two-emulator test runner (android/run_mp_test.ps1)
Orchestrates both emulators:
- Adb-ForDevice $serial <args>: runs adb -s $serial <args>
- Start server, wait for ready
- Install APK on both
- Push game data to both
- Walk through test phases with polling

### 4. Test script (not game_automate -- PowerShell driven)
The test is driven entirely from PowerShell because it orchestrates TWO devices
simultaneously. Game automation scripts only work within one device.

## Status
- [ ] Plan
- [ ] Second AVD creation
- [ ] MP introspection (d1+d2)
- [ ] MP broadcast commands (SetupActivity)
- [ ] Test runner script
- [ ] Build + deploy
- [ ] Run + fix
- [ ] Docs
