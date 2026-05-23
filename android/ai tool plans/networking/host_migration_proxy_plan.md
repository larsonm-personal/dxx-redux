# Host Migration Proxy Plan

## Status: COMPLETE (pending deployment test)

## Problem
After host migration, the proxy is shut down and the engine rebinds directly to
INADDR_ANY. This breaks the proxy architecture that enables:
- Packet statistics and diagnostics
- Future STUN/TURN support
- Connection resilience
- Clean separation between engine and network

Additionally, the stale connection_statuses (CONNT_PROXY) from the pre-migration
session causes the engine to silently drop pdata via the CONNT_PROXY routing path.

## Two distinct bugs, two distinct fixes needed

### Bug 1: Proxy is destroyed after migration (Kotlin layer)
Currently, hostMigrationReceiver shuts down the proxy and the engine rebinds to
INADDR_ANY. Instead, the proxy should be REPLACED with a host-mode proxy.

### Bug 2: Stale connection_statuses for reconnecting players (C engine layer)
net_udp_welcome_player's reconnection path doesn't call net_udp_new_player(),
so connection_statuses is never reset from the old CONNT_PROXY value. This is
independent of the Kotlin proxy -- CONNT_PROXY in the engine means "route via
an intermediary player" (for 3+ player games), not "use LocalhostProxy."
The master should ALWAYS have CONNT_DIRECT for all its clients.

## Design: Host-mode proxy

### Architecture
```
Network peers  <-->  0.0.0.0:HOST_PROXY_PORT  <-->  LocalhostProxy  <-->  127.0.0.1:ENGINE_PORT
                     (real socket)                 (host mode)           (engine, unchanged)
```

### Key difference: dynamic peer registration
- Client-mode proxy: peers known upfront via addPeer()
- Host-mode proxy: peers arrive dynamically (clients connecting to join)
- On first packet from unknown address: auto-create PeerProxy with next
  available loopback port (PROXY_PORT_BASE + slot)

### Port scheme
- HOST_PROXY_PORT = 42425 (new constant, avoids conflict with ENGINE_PORT)
- Engine stays on 127.0.0.1:ENGINE_PORT (42424)
- Each dynamic peer gets 127.0.0.1:PROXY_PORT_BASE+N (42430, 42431, ...)

### Migration flow (new)
```
C Engine                    JNI                     Kotlin
1. detect host departure
   elect new master
   do NOT rebind (stay on loopback)
2. write host_migration.json
   (includes host_proxy_port)
3. android_notify_host_migration()
                            4. broadcast intent
                                                    5. hostMigrationReceiver:
                                                       - shut down old client proxy
                                                       - create host-mode proxy
                                                         on 0.0.0.0:HOST_PROXY_PORT
                                                       - start LAN broadcast
                                                         (advertising HOST_PROXY_PORT)
Joiner (old host returning):
6. discovers LAN broadcast with HOST_PROXY_PORT
7. createLanProxy(hostAddr, HOST_PROXY_PORT)
8. auto_join("127.0.0.1", PROXY_PORT_BASE, ENGINE_PORT)
9. packets flow: joiner -> joiner's proxy -> host's proxy -> engine
```

## Implementation checklist
- [x] Add HOST_PROXY_PORT to NetworkConstants.kt
- [x] Add hostMode to LocalhostProxy (dynamic peer creation in sharedReceiveLoop)
- [x] Add createHostProxy() to MatchmakingService
- [x] Update hostMigrationReceiver to create host-mode proxy
- [x] Pass HOST_PROXY_PORT in LAN broadcast (ANNOUNCE, START, in-game join)
- [x] C: skip rebind_for_hosting() in migration path (d2 + d1)
- [x] C: fix connection_statuses for reconnecting players in welcome_player (d2 + d1)
- [x] Fix duplicate field declarations in LocalhostProxy.kt
- [x] Fix extra closing brace in d1 net_udp_send_rejoin_sync
- [x] Build passes
- [x] Code quality linter passes
- [ ] Deploy and test with 2-player host migration scenario
