# Plan: Remove Relay Scripts and De-duplicate Test Helpers

## Status: DONE

## Context
- Emulator 36.5+ provides shared virtual Wi-Fi LAN between emulator instances
- Tests no longer need udp_relay.ps1 + emulator console redir for game traffic
- The matchmaking server's built-in relay (port 9001) is accessible at 10.0.2.2:9001
- Many test scripts duplicate helper functions (Write-Status, Send-MpCommand, etc.)

## Task 1: Emulator Version Pinning -- DONE
- get_emulator.sh uses `sdkmanager "emulator"` without version pinning
- System image is pinned to API 34 in tool_versions.conf
- Has early-exit guard so re-running won't auto-update
- No changes needed

## Task 2: Remove Relay from Test Scripts -- DONE

### Files modified:
- [x] test_dual_emu_setup.ps1 -- Removed udp_relay.ps1 + local Setup-EmulatorRedir. Uses shared helpers from test_helpers.ps1
- [x] test_dual_emu.ps1 -- Removed udp_relay.ps1 + local helpers. Sources test_helpers.ps1. Keeps NAT simulation and unique Docker NAT functions
- [x] test_manual_lan_coop.ps1 -- Updated comments: now documents direct LAN via shared Wi-Fi

### test_mp.ps1 does NOT use udp_relay.ps1
- It uses the matchmaking server's built-in relay (port 9001) via LocalhostProxy
- This is the correct architecture for server-based MP (server mediates the connection)
- No relay removal needed; only de-duplicated helper functions

## Task 3: De-duplicate Helper Functions -- DONE

### Changes made:
1. [x] Upgraded Write-Status in test_helpers.ps1 to write to $script:LogFile when set by caller
2. [x] Added to test_helpers.ps1: Send-MpCommand, Wait-ForCondition, Wait-SetupReady (multi-device), Start-SetupActivity (multi-device), Setup-EmulatorRedir, Get-GameIntrospection (-Serial param)
3. [x] test_mp.ps1: Removed local Write-Status, Send-MpCommand, Get-GameIntrospection, Wait-ForCondition, Wait-SetupReady, Start-SetupActivity. Kept unique Get-MpIntrospection and Cleanup
4. [x] test_lan.ps1: Removed local Write-Status, Send-MpCommand, Get-GameIntrospection, Wait-ForCondition, Wait-SetupReady, Start-SetupActivity, Setup-EmulatorRedir. Kept unique Cleanup (relay log dump for -UseRelay)
5. [x] test_lan_lobby_discovery.ps1: Removed local Write-Status and Send-MpCommand
6. [x] test_dual_emu.ps1: Now sources test_helpers.ps1. Removed local Write-Status, Adb-Dev, Adb-Dev-Timeout, Test-DeviceOnline, Setup-EmulatorRedir. Kept unique Wait-ForBoot, Setup-DockerNat (STUN overrides), Show-NatMenu, Cleanup (Docker NAT teardown)
7. [x] test_dual_emu_setup.ps1: Already done in prior session. Sources test_helpers.ps1, removed local helpers
