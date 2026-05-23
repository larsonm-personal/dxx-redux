# Multiplayer Network Sync Logging Guide

## Problem Statement
When starting a 2-player game on Android:
- Both players visible in menus
- Player 2 (client) briefly disappears from player list
- Chime/error sound plays
- Level fails to start
- Players return to main menu

## Root Cause Analysis
The issue occurs during the `net_udp_level_sync()` process when the game transitions from menu to level start. The sync process can fail due to:
1. Network timeout - client doesn't receive sync packet from host
2. Socket issues - packets not being sent/received properly
3. Synchronization failure - host and client states not aligned
4. Player connection drops during sync

## Android-Specific Logging Added

### Logging Infrastructure
- Uses existing `net_log_comment()` function in `d2/main/net_udp.c` and `d1/main/net_udp.c`
- Controlled by `GameArg.LogNetTraffic` flag
- Log output goes to network traffic log file (available via game download)
- Wrapped in `#ifdef __android__` blocks for Android-specific debugging

### Functions with Added Logging

#### 1. `net_udp_level_sync()` (Main Sync Entry Point)
- **File**: `d2/main/net_udp.c` line ~5648, `d1/main/net_udp.c` line ~5554
- **Logs**:
  - `[ANDROID] level_sync START: N_players=X master=Y Network_status=Z Player_num=P`
    - Shows number of connected players and whether this instance is host
  - `[ANDROID] level_sync: awaiting sync as client (N_players==0)` OR
  - `[ANDROID] level_sync: host waiting for client requests` OR
  - `[ANDROID] level_sync: client waiting for sync from host`
    - Shows which path is taken based on player role
  - `[ANDROID] level_sync: wait_for_requests returned X`
    - Host-side: result from waiting for clients to load
  - `[ANDROID] level_sync: send_sync returned X`
    - Host-side: result from sending sync to clients
  - `[ANDROID] level_sync END: result=X Players_connected=Y`
    - Shows success/failure and final connection status

#### 2. `net_udp_wait_for_sync()` (Client Sync Waiting)
- **File**: `d2/main/net_udp.c` line ~5513, `d1/main/net_udp.c` line ~5424
- **Role**: Called by clients waiting to receive sync from host
- **Logs**:
  - `[ANDROID] wait_for_sync START: sending initial request to host`
    - Client begins sync process by sending request
  - `[ANDROID] wait_for_sync: net_udp_send_request failed!`
    - Error: couldn't send initial sync request
  - `[ANDROID] wait_for_sync: entering menu loop, waiting for sync from host`
    - Client now waiting in polling loop for sync packet
  - `[ANDROID] wait_for_sync: menu exited with Network_status=X (NETSTAT_PLAYING=5)`
    - Shows exit status - PLAYING (5) = success
  - `[ANDROID] wait_for_sync FAILED: not in PLAYING status, sending quit`
    - Failure: status didn't change to PLAYING before timeout
  - `[ANDROID] wait_for_sync OK: sync received and Network_status is PLAYING`
    - Success: sync completed and game starting

#### 3. `net_udp_sync_poll()` (Client Polling Loop)
- **File**: `d2/main/net_udp.c` line ~3764, `d1/main/net_udp.c` line ~3708
- **Role**: Called every frame while waiting for sync, receives packets
- **Logs**:
  - `[ANDROID] sync_poll: host disconnected!`
    - Critical error: host became unreachable
  - `[ANDROID] sync_poll: Network_status changed to PLAYING, exiting`
    - Success: sync packet received, game starting
  - `[ANDROID] sync_poll: Network_status changed to X, exiting`
    - Status changed to something other than PLAYING (error state)
  - `[ANDROID] sync_poll: timeout waiting for sync, resending request (attempt N)`
    - 2-second timeout expired, retrying sync request (attempts count up)
  - `[ANDROID] sync_poll: net_udp_send_request failed on retry!`
    - Couldn't resend request - network may be down

#### 4. `net_udp_wait_for_requests()` (Host Waiting for Clients)
- **File**: `d2/main/net_udp.c` line ~5639, `d1/main/net_udp.c` (mirrors added)
- **Role**: Host waits for all clients to load level before sending sync
- **Logs**:
  - `[ANDROID] wait_for_requests START: waiting for clients to load level`
  - `[ANDROID] wait_for_requests: reset player states for N players`
    - Sets up proper connection state tracking

#### 5. `net_udp_request_poll()` (Host Polling Loop)
- **File**: `d2/main/net_udp.c` line ~5608, `d1/main/net_udp.c` line ~5467
- **Role**: Called every frame on host while waiting for clients
- **Logs** (every 1 second):
  - `[ANDROID] request_poll: N/M players ready (states: p0=X p1=Y)`
    - Shows how many clients have checked in (0=disconnected, 1=playing, 2=waiting)
    - Useful to see if client is stuck waiting
  - `[ANDROID] request_poll: all players ready, exiting`
    - All clients have responded, proceeding to sync send

#### 6. `net_udp_send_sync()` (Host Sending Sync)
- **File**: `d2/main/net_udp.c` line ~4968, `d1/main/net_udp.c` line ~4845
- **Role**: Host sends level sync packet to all clients
- **Logs**:
  - `[ANDROID] send_sync FAILED: not enough start positions`
    - Level doesn't have enough player spawn positions for player count
  - `[ANDROID] send_sync: N_players=X, sending SYNC to all clients`
  - `[ANDROID] send_sync: sending SYNC to player N (CALLSIGN)`
    - Shows each client getting sync packet with their callsign
  - `[ANDROID] send_sync: sent sync to all clients, processing own copy`
  - `[ANDROID] send_sync: completed successfully`

## Interpreting Logs: Common Failure Scenarios

### Scenario 1: Client Never Gets Sync Packet
**Logs show**:
- Client: `[ANDROID] wait_for_sync: entering menu loop...`
- Repeated: `[ANDROID] sync_poll: timeout waiting for sync, resending...` (attempts 1,2,3...)
- Never: `[ANDROID] sync_poll: Network_status changed to PLAYING`
- Client: `[ANDROID] wait_for_sync FAILED: not in PLAYING status`

**Causes**:
- Host sync send failed - check host logs for send errors
- Network packet loss between host and client
- Android firewall blocking UDP packets to client
- Client socket not properly listening for packets

### Scenario 2: Host Never Gets Client Request
**Logs show** (Host):
- `[ANDROID] wait_for_requests START...`
- Repeated: `[ANDROID] request_poll: 0/2 players ready...`
- Never: `[ANDROID] request_poll: all players ready`
- Eventually timeout

**Causes**:
- Client request packets not reaching host
- Android firewall blocking UDP packets from client
- Network interface change before sync starts
- Client never entered wait_for_sync() state

### Scenario 3: Host Disconnects During Sync
**Logs show** (Client):
- `[ANDROID] sync_poll: host disconnected!`

**Causes**:
- Host process crashed
- Host network connection lost
- Host shut down abruptly
- Network interface change on host

### Scenario 4: Insufficient Start Positions
**Logs show** (Host):
- `[ANDROID] send_sync FAILED: not enough start positions`

**Causes**:
- Level loaded with fewer player spawn points than players in game
- Need level with at least N spawn positions for N players

## Enabling Logging

### Method 1: Command Line Flag
```bash
# Start game with net traffic logging enabled
./game --LogNetTraffic
```

### Method 2: Configuration File
Check `playsave.c` or game configuration for `LogNetTraffic` setting.

## Accessing Logs

### Android
```bash
# Download net traffic log from emulator/device
adb pull /data/data/com.dxxredux.app/files/net_traffic.log

# Or via introspection (if supported)
./android/introspect.sh console
```

### Desktop
Log files are written to game data directory, typically:
- Linux: `~/.local/share/dxx-redux/`
- Windows: `AppData\Local\dxx-redux\`
- macOS: `~/Library/Application Support/dxx-redux/`

## Log File Format
```
timestamp[ms] [ANDROID] message text
1234.567890 [ANDROID] level_sync START: N_players=2 master=1 Network_status=1 Player_num=0
1235.123456 [ANDROID] wait_for_requests START: waiting for clients to load level
```

## Development Notes

- All Android logging is wrapped in `#ifdef __android__` so it has zero overhead on non-Android builds
- Log messages use snprintf with 256-byte buffers to avoid stack overflow
- Logging uses the existing `net_log_comment()` function which is already called throughout the network code
- Changes mirror between d1/ and d2/ to maintain parity as per code style guidelines

## Next Steps for Debugging

1. Enable `LogNetTraffic` before running multiplayer test
2. Start 2-player game and attempt to launch level
3. Reproduce the disconnect/chime issue
4. Download the net_traffic.log file
5. Search logs for `[ANDROID]` entries in this order:
   - Host: `wait_for_requests`, `request_poll` state changes
   - Client: `wait_for_sync`, `sync_poll` state changes
   - Look for "FAILED" or "disconnected" messages
   - Count timeout retries - if many, indicates packet loss
6. Check for differences in log sequences between host and client
7. Verify both players' callsigns appear in send_sync messages
