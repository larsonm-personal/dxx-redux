#!/usr/bin/env pwsh

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$descentPath = Join-Path $repoRoot "game_data\mission_files\Descent.json"
$counterstrikePath = Join-Path $repoRoot "game_data\mission_files\Counterstrike.json"

function Get-MissionLevels {
    param([Parameter(Mandatory = $true)][string]$Path)

    $missions = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
    return @($missions | ForEach-Object { $_.levels })
}

$descentFailures = @(Get-MissionLevels -Path $descentPath | Where-Object { $_.route_status -ne "ok" })
if ($descentFailures.Count -gt 0) {
    $summary = $descentFailures | ForEach-Object { "level $($_.level_num): $($_.route_status) $($_.route_problem)" }
    throw "Descent route regressions:`n$($summary -join "`n")"
}

$counterstrikeFailures = @(
    Get-MissionLevels -Path $counterstrikePath |
        Where-Object { $_.route_status -ne "ok" }
)
if ($counterstrikeFailures.Count -gt 0) {
    $summary = $counterstrikeFailures | ForEach-Object { "level $($_.level_num): $($_.route_status) $($_.route_problem)" }
    throw "Counterstrike route regressions:`n$($summary -join "`n")"
}

$level10 = Get-MissionLevels -Path $counterstrikePath | Where-Object { $_.level_num -eq 10 }
$expectedLevel10Steps = @(
    "start:none"
    "key:destroy_key_carrier:blue"
    "key:destroy_key_carrier:gold"
    "key:destroy_key_carrier:red"
    "trigger:shoot_switch:18"
    "trigger:shoot_switch:25"
    "reactor:destroy_reactor"
    "exit:enter_exit:0"
)
$actualLevel10Steps = @(
    $level10.route_steps | ForEach-Object {
        (@($_.kind, $_.activation_kind, $_.key, $_.trigger) |
            Where-Object { $null -ne $_ }) -join ':'
        }
    )
    if (-not $level10 -or (Compare-Object $expectedLevel10Steps $actualLevel10Steps)) {
        throw "Counterstrike level 10 carrier route changed:`n$($actualLevel10Steps -join "`n")"
    }

    Write-Host "PASS: base mission route statuses"
