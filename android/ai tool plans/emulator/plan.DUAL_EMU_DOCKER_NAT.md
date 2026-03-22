# Plan: Dual Emulator Launch + Docker NAT Testbed

## Status: IMPLEMENTED

## Summary

Two deliverables implemented:
1. A PowerShell script that launches two Android emulators with full
   multiplayer infrastructure (APK, game data, server, relay) ready for testing.
2. A Docker Compose environment with configurable NAT containers for realistic
   STUN/NAT type testing.

## Files Created

### Part 1: Dual Emulator Launch
- [x] `android/launch_dual_emulators.ps1` -- orchestrates both emulators, server, relay
  - Flags: -NoBuild, -NoData, -NoServer, -KillOnExit
  - Launches Nexus5X_Light_1 (emulator-5554) and Nexus5X_Light_2 (emulator-5556)
  - Installs APK, pushes game data (via ANDROID_SERIAL env var), starts server + relay
  - Prints summary with quick-test commands, waits for user input

### Part 2: Docker NAT Testbed
- [x] `docker/nat-testbed/Dockerfile` -- Python 3.12 Alpine image
- [x] `docker/nat-testbed/nat_proxy.py` -- transparent UDP NAT proxy
  - Supports: full-cone, port-restricted, symmetric, symmetric-seq
  - Forwards STUN traffic to upstream server on host
- [x] `docker/nat-testbed/docker-compose.yml` -- two NAT containers
  - nat_a: host ports 13478/13479 (configurable via NAT_A env var)
  - nat_b: host ports 23478/23479 (configurable via NAT_B env var)
- [x] `android/setup_docker_nat.ps1` -- starts containers + sends STUN overrides
- [x] `android/teardown_docker_nat.ps1` -- stops containers + clears overrides

### Files Modified
- [x] `android/push_game_data.sh` -- added ANDROID_SERIAL documentation (adb natively
  respects this env var, no code change needed beyond documenting it)
- [x] `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt`
  - Added `stun_override` and `stun_override_clear` MP_COMMAND handlers
- [x] `android/app/src/main/java/com/dxxredux/app/multiplayer/MatchmakingService.kt`
  - Added `stunOverrideAddrs` volatile field + `setStunOverride()` method
  - Modified `launchStunDiscovery()` to use override when set

## Architecture

```
Emulator A (SLIRP)        Windows Host / Docker            Emulator B (SLIRP)
     |                                                          |
  WS direct ---------> Matchmaking Server :9000 <--------- WS direct
  STUN: 10.0.2.2:13478 --> [NAT A container] --> Server STUN :3478
  STUN: 10.0.2.2:13479 --> [NAT A container] --> Server STUN :3479
  STUN: 10.0.2.2:23478 --> [NAT B container] --> Server STUN :3478
  STUN: 10.0.2.2:23479 --> [NAT B container] --> Server STUN :3479
```

## Testing

### Dual emulator launch
```powershell
cd android
.\launch_dual_emulators.ps1           # full build + launch
.\launch_dual_emulators.ps1 -NoBuild  # skip builds
```

### Docker NAT testbed
```powershell
cd android
.\setup_docker_nat.ps1                                    # defaults: full-cone + symmetric
.\setup_docker_nat.ps1 -NatA port-restricted -NatB symmetric
.\setup_docker_nat.ps1 -NatA symmetric -NatB symmetric   # both symmetric

# View NAT proxy logs
docker compose -f ..\docker\nat-testbed\docker-compose.yml logs -f

# Tear down
.\teardown_docker_nat.ps1
```

## Known Limitations
- Holepunch between emulators NOT possible (SLIRP prevents routing to Docker bridge IPs)
- STUN override is volatile per-process (must re-send after app restart)
- Docker Desktop WSL2 adds small latency to container UDP traffic
