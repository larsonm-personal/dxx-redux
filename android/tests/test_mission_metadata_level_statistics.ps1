#!/usr/bin/env pwsh

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$metadataPath = Join-Path $repoRoot "game_data\mission_files\legacy.json"
$missions = @(Get-Content -LiteralPath $metadataPath -Raw | ConvertFrom-Json)
$mission = @($missions | Where-Object { $_.mission_filename -eq "legacy.mn2" })
if ($mission.Count -ne 1) {
    throw "Expected one Legacy of Chaos mission in legacy.json"
}

$expected = @(
    @(1, 388, 133, 14, 189, 52, 52),
    @(2, 518, 115, 16, 229, 54, 53),
    @(3, 785, 240, 21, 289, 44, 44)
)
$levels = @($mission[0].levels)
foreach ($row in $expected) {
    $level = @($levels | Where-Object { [int]$_.level_num -eq $row[0] })
    if ($level.Count -ne 1) {
        throw "Expected Legacy of Chaos level $($row[0])"
    }
    $actual = @(
        [int]$level[0].segment_count,
        [int]$level[0].wall_count,
        [int]$level[0].trigger_count,
        [int]$level[0].object_count,
        [int]$level[0].texture_count
    )
    $wanted = @($row[1..5])
    if (($actual -join ",") -ne ($wanted -join ",")) {
        throw "Legacy level $($row[0]) statistics were $($actual -join ', '), expected $($wanted -join ', ')"
    }
    if ([Math]::Abs($actual[4] - [int]$row[6]) -gt 1) {
        throw "Legacy level $($row[0]) texture count $($actual[4]) is not close to README count $($row[6])"
    }
}

Write-Host "PASS: Legacy of Chaos level statistics match or are within one texture of the README"
