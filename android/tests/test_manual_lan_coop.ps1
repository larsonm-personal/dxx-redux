#!/usr/bin/env pwsh
# test_manual_lan_coop.ps1 -- Launch two emulators for by-hand LAN coop testing
# (no matchmaking server).
#
# Uses emulator 36.5+ shared Wi-Fi for direct LAN connectivity.
# EMU1 hosts a game normally. EMU2 joins via LAN scan or manual IP entry
# in the Join Game menu using the host's wlan0 IP.
#
# Usage:
#   .\test_manual_lan_coop.ps1
#   .\test_manual_lan_coop.ps1 -NoBuild
#   .\test_manual_lan_coop.ps1 -NoBuild -NoData

param(
    [switch]$NoBuild,
    [switch]$NoData
)

$passthrough = @("-NoServer")
if ($NoBuild) { $passthrough += "-NoBuild" }
if ($NoData) { $passthrough += "-NoData" }

& "$PSScriptRoot\test_dual_emu_setup.ps1" @passthrough
exit $LASTEXITCODE
