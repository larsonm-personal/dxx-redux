#!/usr/bin/env pwsh
# Fixed public three-player category; experiments and two-player smoke stay separate
$ErrorActionPreference = 'Stop'
& "$PSScriptRoot/test_lan_launch_preparation.ps1" -PublicShareware -ThreePlayerStartup -RecoveryChecks:$false -MeasureTransitions:$false
