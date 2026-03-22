# Plan: Dual Emulator Launch + Docker NAT Testbed

## Status: IMPLEMENTED (reorganized)

## Summary

Two deliverables implemented:
1. A PowerShell script that launches two Android emulators with full
   multiplayer infrastructure (APK, game data, server, relay) ready for testing,
   including an interactive NAT configuration menu.
2. A Docker Compose environment with configurable NAT containers for realistic
   STUN/NAT type testing.

### Test reorganization (phase 2)
- All .ps1 test scripts moved from `android/` to `android/tests/`
- `android/Run-TestMenu.ps1` rewritten to discover both json5 and ps1 tests
- `android/tests/test_dual_emu.ps1` is the new unified script that replaces
  the old `launch_dual_emulators.ps1` + `setup_docker_nat.ps1` workflow

## Files Created

### Primary: Dual Emulator + NAT Test
- [x] `android/tests/test_dual_emu.ps1` -- all-in-one dual emulator + NAT menu
  - Flags: -NoBuild, -NoData, -KillOnExit
  - Launches Nexus5X_Light_1 (emulator-5554) and Nexus5X_Light_2 (emulator-5556)
  - Installs APK, pushes game data, starts server + relay
  - Interactive NAT menu with 8 presets (no NAT, full-cone, symmetric, etc.)
  - Cleanup tears down Docker containers + relay + server

### Legacy (kept but superseded by test_dual_emu.ps1)
- [x] `android/tests/test_dual_emu_setup.ps1` -- moved from launch_dual_emulators.ps1

### Docker NAT Infrastructure
- [x] `docker/nat-testbed/Dockerfile` -- Python 3.12 Alpine image
- [x] `docker/nat-testbed/nat_proxy.py` -- transparent UDP NAT proxy
  - Supports: full-cone, port-restricted, symmetric, symmetric-seq
  - Forwards STUN traffic to upstream server on host
- [x] `docker/nat-testbed/docker-compose.yml` -- two NAT containers
  - nat_a: host ports 13478/13479 (configurable via NAT_A env var)
  - nat_b: host ports 23478/23479 (configurable via NAT_B env var)
- [x] `android/setup_docker_nat.ps1` -- standalone NAT setup (used by test_dual_emu.ps1)
- [x] `android/teardown_docker_nat.ps1` -- standalone NAT teardown

### Moved Tests (from android/ to android/tests/)
- run_mp_test.ps1 -> test_mp.ps1
- run_lan_test.ps1 -> test_lan.ps1
- run_extract_test.ps1 -> test_extract.ps1
- run_all_extract_tests.ps1 -> test_all_extracts.ps1
- run_cue_iso_tests.ps1 -> test_cue_iso.ps1
- run_controller_compare.ps1 -> test_controller_compare.ps1
- test_resolution.ps1 -> test_resolution.ps1 (same name)
- test_saf_archiver.ps1 -> test_saf_archiver.ps1 (same name)
- test_bot_client.ps1 -> test_bot_client.ps1 (same name)

### Files Modified
- [x] `android/Run-TestMenu.ps1` -- rewritten to discover json5 + ps1 tests
- [x] `android/push_game_data.sh` -- added ANDROID_SERIAL documentation
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

### Unified dual emulator + NAT test
```powershell
cd android/tests
.\test_dual_emu.ps1           # full build + launch + NAT menu
.\test_dual_emu.ps1 -NoBuild  # skip APK build
```

### Test menu (discovers all tests)
```powershell
cd android
.\Run-TestMenu.ps1   # shows json5 + ps1 tests, run interactively
```

### Standalone Docker NAT (for advanced use)
```powershell
cd android
.\setup_docker_nat.ps1 -NatA full-cone -NatB symmetric
.\teardown_docker_nat.ps1
```

## NAT Presets (in test_dual_emu.ps1 menu)
| # | Name | NAT A | NAT B |
|---|------|-------|-------|
| 1 | No NAT (direct STUN) | none | none |
| 2 | Full-Cone / Full-Cone | full-cone | full-cone |
| 3 | Full-Cone / Symmetric | full-cone | symmetric |
| 4 | Port-Restricted / Symmetric | port-restricted | symmetric |
| 5 | Symmetric / Symmetric | symmetric | symmetric |
| 6 | Full-Cone / Port-Restricted | full-cone | port-restricted |
| 7 | Port-Restricted / Port-Restricted | port-restricted | port-restricted |
| 8 | Symmetric-Seq / Symmetric-Seq | symmetric-seq | symmetric-seq |

## Known Limitations
- Holepunch between emulators NOT possible (SLIRP prevents routing to Docker bridge IPs)
- STUN override is volatile per-process (must re-send after app restart)
- Docker Desktop WSL2 adds small latency to container UDP traffic
