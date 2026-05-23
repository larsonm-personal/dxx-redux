# Relay Multiplayer Fix Plan

## Problem 1: RELAY_PUBLIC_ADDR not set
The `RELAY_PUBLIC_ADDR` env var wasn't set when launching the matchmaking
server in `test_dual_emu_setup.ps1`. When empty (the default),
`allocate_relay_session()` returns `None`, so no relay is available.

### Fix
Set `RELAY_PUBLIC_ADDR=10.0.2.2:9001` in the server's environment.

## Problem 2: Server pipe buffer deadlock (DONE)
The server was launched via `ProcessStartInfo` with `RedirectStandardOutput`
and `RedirectStandardError` set to `$true`, creating OS pipes. Nobody read
these pipes. Once the ~64KB buffer filled up with tracing output, every
`info!`/`warn!` call in the server blocked, deadlocking all tokio tasks.
The server stopped processing messages AND WebSocket pings/pongs.

Evidence from logcat:
- Multiple `TX: READY` messages sent by client, zero `RX` responses
- WebSocket ping timeout: "after 0 successful ping/pongs"
- Server process still alive (`Get-NetTCPConnection` shows it listening)

### Fix
Changed all three test scripts to use `Start-Process` with file-based
`-RedirectStandardOutput`/`-RedirectStandardError` instead of pipe-based
`ProcessStartInfo`. Logs go to `temp/server.log` and `temp/server_err.log`.
