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

$allowedCounterstrikePartial = 10
$counterstrikeFailures = @(
    Get-MissionLevels -Path $counterstrikePath |
        Where-Object { $_.route_status -ne "ok" -and $_.level_num -ne $allowedCounterstrikePartial }
)
if ($counterstrikeFailures.Count -gt 0) {
    $summary = $counterstrikeFailures | ForEach-Object { "level $($_.level_num): $($_.route_status) $($_.route_problem)" }
    throw "Counterstrike route regressions:`n$($summary -join "`n")"
}

$level10 = Get-MissionLevels -Path $counterstrikePath | Where-Object { $_.level_num -eq $allowedCounterstrikePartial }
if (-not $level10 -or $level10.route_status -ne "partial") {
    throw "Counterstrike level 10 exception changed; review and update this regression intentionally"
}

Write-Host "PASS: base mission route statuses"
