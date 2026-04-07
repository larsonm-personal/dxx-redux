#!/usr/bin/env pwsh
# test_lan_lobby_discovery.ps1 -- Test LAN lobby broadcast discovery between
# two emulators on shared Wi-Fi (emulator 36.5+).
#
# Uses the lan_host_lobby / lan_discover / lan_discover_status MP_COMMANDs
# to verify that the Kotlin LobbyService broadcast mechanism works across
# emulators without any relay.
#
# Prerequisites:
#   - Two emulators running (emulator-5554, emulator-5556) with emulator 36.5+
#   - APK installed on both
#
# Usage:
#   .\test_lan_lobby_discovery.ps1
#   .\test_lan_lobby_discovery.ps1 -TimeoutSeconds 30

param(
    [int]$TimeoutSeconds = 30
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

. "$PSScriptRoot\..\test_helpers.ps1"

$REPO_ROOT = Split-Path (Split-Path $PSScriptRoot)
$DEP_BASE = (Get-Content (Join-Path $REPO_ROOT "dependency_base.txt") -First 1).Trim()
$ADB = "$DEP_BASE\android-sdk\platform-tools\adb.exe"
$PACKAGE = "com.dxxredux.app"
$ACTIVITY = "com.dxxredux.app.SetupActivity"

# Dynamically detect two emulators instead of hard-coding serials
$devices = & $ADB devices 2>&1 | Out-String
$emus = [regex]::Matches($devices, "emulator-(\d+)\s+device")
if ($emus.Count -lt 2) {
    Write-Host "SKIP: Need 2 emulators, found $($emus.Count)"
    exit 0
}
$EMU1 = "emulator-$($emus[0].Groups[1].Value)"
$EMU2 = "emulator-$($emus[1].Groups[1].Value)"

$script:LogFile = Join-Path $REPO_ROOT "temp\lan_discovery_test_log.txt"
try { if (Test-Path $script:LogFile) { Remove-Item $script:LogFile -Force -ErrorAction SilentlyContinue } } catch { }
"" | Set-Content -Path $script:LogFile -Encoding utf8 -ErrorAction SilentlyContinue

try {
    Write-Status "=== LAN Lobby Discovery Test ===" "White"

    # Verify both emulators are online
    foreach ($emu in @($EMU1, $EMU2)) {
        $state = Adb-Dev-Timeout -Serial $emu -AdbArgs @("get-state") -Seconds 5
        if ($state -notmatch "device") {
            Write-Status "FAIL: $emu not online (state: $state)" "Red"; exit 1
        }
    }
    Write-Status "Both emulators online"

    # Force-stop and restart apps
    foreach ($emu in @($EMU1, $EMU2)) {
        Adb-Dev -Serial $emu -AdbArgs @("shell", "am", "force-stop", $PACKAGE) | Out-Null
        Start-Sleep 1
        Adb-Dev -Serial $emu -AdbArgs @("logcat", "-c") | Out-Null
        Adb-Dev -Serial $emu -AdbArgs @("shell", "am", "start", "-n", "$PACKAGE/$ACTIVITY") | Out-Null
    }
    Start-Sleep 6

    # Check wlan0 IPs
    $emu1Ip = (Adb-Dev-Timeout -Serial $EMU1 -AdbArgs @("shell", "ip", "addr", "show", "wlan0") -Seconds 5 |
            Select-String 'inet (\d+\.\d+\.\d+\.\d+)').Matches[0].Groups[1].Value
    $emu2Ip = (Adb-Dev-Timeout -Serial $EMU2 -AdbArgs @("shell", "ip", "addr", "show", "wlan0") -Seconds 5 |
            Select-String 'inet (\d+\.\d+\.\d+\.\d+)').Matches[0].Groups[1].Value
    Write-Status "EMU1 wlan0: $emu1Ip, EMU2 wlan0: $emu2Ip"

    if (-not $emu1Ip -or -not $emu2Ip) {
        Write-Status "FAIL: No wlan0 IP -- emulator 36.5+ shared Wi-Fi not available" "Red"; exit 1
    }

    # Start hosting on EMU1
    Write-Status "Starting LAN lobby host on $EMU1..."
    Send-MpCommand -Serial $EMU1 -Command "lan_host_lobby" -Extras @(
        "--es", "callsign", "DiscoveryHost",
        "--es", "game", "d2",
        "--es", "mission", "Counterstrike!",
        "--es", "mode", "coop",
        "--ei", "max_players", "4"
    )

    # Start discovery on EMU2
    Write-Status "Starting LAN discovery on $EMU2..."
    Send-MpCommand -Serial $EMU2 -Command "lan_discover" -Extras @(
        "--es", "callsign", "DiscoveryJoin"
    )

    # Poll for discovery
    $found = $false
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    $pollN = 0
    while ((Get-Date) -lt $deadline -and -not $found) {
        Start-Sleep 3
        $pollN++
        Send-MpCommand -Serial $EMU2 -Command "lan_discover_status"
        Start-Sleep 1
        $lines = Adb-Dev-Timeout -Serial $EMU2 -AdbArgs @("logcat", "-d", "-s", "DXX-MP:*") -Seconds 5
        $statusLine = $lines | Where-Object { $_ -match 'lan_discover_status:' } | Select-Object -Last 1
        if ($statusLine) {
            Write-Status "  [poll $pollN] $statusLine" "Gray"
            if ($statusLine -match 'lobbies=(\d+)' -and [int]$Matches[1] -gt 0) {
                $lobbyLine = $lines | Where-Object { $_ -match '  lobby:' } | Select-Object -Last 1
                if ($lobbyLine) { Write-Status "  $lobbyLine" "Green" }
                $found = $true
            }
        }
    }

    # Cleanup
    Send-MpCommand -Serial $EMU1 -Command "lan_stop_lobby"
    Send-MpCommand -Serial $EMU2 -Command "lan_stop_lobby"

    if ($found) {
        Write-Status ""
        Write-Status "=== LAN LOBBY DISCOVERY TEST PASSED ===" "Green"
        exit 0
    } else {
        Write-Status ""
        Write-Status "=== LAN LOBBY DISCOVERY TEST FAILED ===" "Red"
        Write-Status "EMU1 LobbyService logs:" "Gray"
        Adb-Dev-Timeout -Serial $EMU1 -AdbArgs @("logcat", "-d", "-s", "LobbyService:*") -Seconds 5 |
            Select-Object -Last 10 | ForEach-Object { Write-Status "  $_" "Gray" }
        Write-Status "EMU2 LobbyService logs:" "Gray"
        Adb-Dev-Timeout -Serial $EMU2 -AdbArgs @("logcat", "-d", "-s", "LobbyService:*") -Seconds 5 |
            Select-Object -Last 10 | ForEach-Object { Write-Status "  $_" "Gray" }
        exit 1
    }
} finally {
    # Ensure lobby services are stopped
    try { & $ADB -s $EMU1 shell "am broadcast -a com.dxxredux.MP_COMMAND --es command lan_stop_lobby" 2>&1 | Out-Null } catch {}
    try { & $ADB -s $EMU2 shell "am broadcast -a com.dxxredux.MP_COMMAND --es command lan_stop_lobby" 2>&1 | Out-Null } catch {}
}
