# Plan: Relay Limits, IP Privacy, Network Debug Logging

## Status: COMPLETE

## 1. Server: JSON5 Config File Support -- DONE

- Added `json5 = "=1.3.1"` to Cargo.toml
- Rewrote config.rs: `ConfigFile` struct with serde Deserialize + `ServerConfig::load()`
- Layering: JSON5 file -> env var overrides -> hardcoded defaults
- Config file path from `CONFIG_FILE` env var (default: `server_config.json5`)
- Created `server_config.json5.template` with commented defaults
- Added `server_config.json5` to server/.gitignore

## 2. Server: Max Relay Slots Limit -- DONE

- Added `max_relay_sessions: usize` config field (default: 100, 0 = unlimited)
- In `allocate_relay_session()`: checks `state.relay_sessions.len()` against limit
- On game start, if any relay alloc fails due to limit: reverts lobby to Waiting,
  cleans up partial allocations, sends RELAY_LIMIT_REACHED error to all lobby players
- 75 server tests pass, clippy clean

## 3. Server: IP Privacy Verification -- NO CHANGES NEEDED

IPs are only shared with players in the same active lobby who have progressed to
NAT traversal (PEER_CANDIDATES, CONNECTIVITY_CHECK_GO) or game start (GAME_STARTING).
LOBBY_LIST, SERVER_STATUS, ActiveGameInfo, CONNECTION_INFO, and friend messages
contain NO IPs. The architecture already keeps IPs private until the game flow
requires them for P2P connections.

## 4. Android: Network Debug Logger -- DONE

Created `NetLog.kt` (com.dxxredux.app.multiplayer):
- Singleton with enable/disable via SharedPreferences ("dxx_prefs" / "net_logging_enabled")
- Files in `filesDir/netlogs/netlog_YYYYMMDD_HHmmss.txt`
- Max 10 files, oldest pruned on new file creation
- Thread-safe synchronized writes
- Categories: SYSTEM, CONNECT, AUTH, LOBBY, STUN, HOLEPUNCH, RELAY, GAME, ERROR
- Share via FileProvider (added `netlog_exports` cache-path to file_paths.xml)

## 5. Android: Advanced Tab Logging UI -- DONE

Added "Network Logging" section to AdvancedSettingsPage.kt between Config Management
and Danger Zone:
- Toggle switch to enable/disable logging
- List of existing log files with dates and sizes
- "Export" button per file (share intent via FileProvider)
- "Delete All Logs" button with confirmation dialog

## 6. Wiring: NetLog in MatchmakingService -- DONE

Added `NetLog.log()` calls at all key network events:
- CONNECT: connect, disconnect, WebSocket open/close/failure, reconnect, maintenance
- AUTH: auth success, auth failure
- LOBBY: lobby updates with player count and host status
- STUN: discovery results, peer candidates
- HOLEPUNCH: connectivity check go, direct connection results, relay fallback
- RELAY: relay assignment
- GAME: game starting with mission/slot/peer details
- ERROR: server errors, STUN failures, WebSocket failures
