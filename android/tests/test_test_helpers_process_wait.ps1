#!/usr/bin/env pwsh

$ErrorActionPreference = 'Stop'

. "$PSScriptRoot\..\helpers\test_helpers.ps1"

$script:TimeoutCalls = 0
$script:GameProcessAlive = $true

function Adb-Timeout {
    param([string[]]$AdbArgs, [int]$Seconds = 8)
    $script:TimeoutCalls++
    $name = $AdbArgs[-1]
    if ($name -eq "$($script:PACKAGE):game" -and $script:GameProcessAlive) {
        return '12345'
    }
    return ''
}

if (Wait-ProcessDead -TimeoutMs 250) {
    throw 'Wait-ProcessDead returned true while the game process was still alive'
}
if ($script:TimeoutCalls -lt 2) {
    throw 'Wait-ProcessDead did not query both app processes'
}

$script:GameProcessAlive = $false
if (-not (Wait-ProcessDead -TimeoutMs 250)) {
    throw 'Wait-ProcessDead did not return true after both processes were gone'
}

Write-Host 'PASS'
