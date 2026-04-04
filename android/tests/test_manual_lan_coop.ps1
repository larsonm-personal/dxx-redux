#!/usr/bin/env pwsh
# test_manual_lan_coop.ps1 -- Launch two emulators for by-hand LAN coop testing
# via direct UDP relay (no matchmaking server).
#
# A UDP relay bridges the two emulators' isolated networks so they can
# communicate. EMU1 hosts a game normally. EMU2 joins via manual IP entry
# in the Join Game menu: address 10.0.2.2, port 42600.
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
