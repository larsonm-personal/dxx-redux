# Networking Audit Round 4 - Implementation Plan

## D7: Global connection cap + server config file cleanup
- Add `max_connections` field to `ServerConfig` and `ConfigFile` in config.rs (default: 500)
- Enforce in `ws_upgrade()`: if sessions.len() >= max_connections, return 503
- Add to test fixture `ServerConfig` in integration.rs
- The config file already exists and works (JSON5 + env var override). Just add the new field.

## D11: Relay token collision fix
- In `allocate_relay_session()` (ws_handler.rs ~L846), loop to generate unique token
- Check `relay_sessions.contains_key(&token)` before insert
- Limit retries to prevent infinite loop (10 attempts)

## D12: Relay session hijacking guard
- In relay.rs address learning, only accept addresses that were pre-registered
- In `allocate_relay_session()`, pre-populate expected slot addresses from connectivity results
- If a slot has a pre-registered address, reject packets from other addresses
- Address learning only fills empty slots (already done), add: only if expected_players matches

## C8: Relay keepalive
- In LocalhostProxy.kt PeerProxy.run(), launch keepalive for relay connections too
- Relay keepalive sends a minimal relay-wrapped packet periodically

## C9: Proxy cleanup on game end
- In MatchmakingService.kt onClosed/onFailure, shut down localhostProxy
- Already partially done in disconnect() but not in the WS event handlers

## C10: shutdown() close sockets first
- In LocalhostProxy.kt shutdown(), close proxies before cancelling jobs
- This unblocks receive() calls immediately

## C11: Connectivity check dedup
- Track active connectivity check job in MatchmakingService
- Cancel previous before launching new one in launchConnectivityCheck()

## C12: GPGS timeout
- Wrap getServerAuthCode in withTimeoutOrNull(5000) in sendAuthenticate()
