#!/usr/bin/env pwsh
param(
    [ValidateSet('d1', 'd2')][string]$Game,
    [string]$HostDevice = 'emulator-5554',
    [string]$JoinDevice = 'emulator-5556',
    [string]$HostAvd = 'Nexus5X_Light_1',
    [string]$JoinAvd = 'Nexus5X_Light_2',
    [string]$MissionFile
)
$ErrorActionPreference = 'Stop'
foreach ($engine in $(if ($Game) { @($Game) } else { @('d1', 'd2') })) {
    $arguments = @('-NoProfile', '-File', (Join-Path $PSScriptRoot 'test_lan.ps1'),
        '-Game', $engine, '-HostDevice', $HostDevice, '-JoinDevice', $JoinDevice,
        '-HostAvd', $HostAvd, '-JoinAvd', $JoinAvd, '-SkipBuild', '-PauseMenus')
    if ($MissionFile) { $arguments += @('-MissionFile', $MissionFile) }
    & pwsh @arguments
    if ($LASTEXITCODE -ne 0) { throw "$engine co-op pause/menu test failed" }
}
