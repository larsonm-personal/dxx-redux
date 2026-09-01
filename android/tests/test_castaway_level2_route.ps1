#!/usr/bin/env pwsh

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$metadataPath = Join-Path $repoRoot 'game_data\mission_files\castaway_redux.json'
$mission = @(Get-Content -LiteralPath $metadataPath -Raw | ConvertFrom-Json)
$level = @($mission[0].levels | Where-Object { $_.level_num -eq 2 })

if ($level.Count -ne 1) {
    throw "Expected exactly one Castaway level 2 record, found $($level.Count)"
}
if ($level[0].route_status -ne 'ok' -or
    -not [string]::IsNullOrEmpty([string]$level[0].route_problem)) {
    throw "Castaway level 2 route is $($level[0].route_status): $($level[0].route_problem)"
}
if ([int]$level[0].route_required_key_mask -ne 7) {
    throw "Castaway level 2 required key mask is $($level[0].route_required_key_mask), expected 7"
}
if ([int]$level[0].route_completing_key_mask_set -ne 128) {
    throw "Castaway level 2 completing key mask set is $($level[0].route_completing_key_mask_set), expected only mask 7"
}

$objectives = @(
    $level[0].route_steps | ForEach-Object {
        if ($_.kind -eq 'trigger') {
            "trigger:$($_.trigger)"
        } else {
            [string]$_.kind
        }
    }
)
$expected = @(
    'start'
    'trigger:1'
    'trigger:6'
    'trigger:3'
    'trigger:4'
    'key'
    'key'
    'key'
    'trigger:9'
    'trigger:10'
    'trigger:32'
    'trigger:18'
    'trigger:17'
    'trigger:19'
    'trigger:24'
    'trigger:16'
    'trigger:21'
    'trigger:20'
    'reactor'
    'exit'
)
if (($objectives -join ',') -ne ($expected -join ',')) {
    throw "Castaway level 2 route is $($objectives -join ','), expected $($expected -join ',')"
}

$keys = @(
    $level[0].route_steps |
        Where-Object { $_.kind -eq 'key' } |
        ForEach-Object { [string]$_.key }
)
$expectedKeys = @('blue', 'gold', 'red')
if (($keys -join ',') -ne ($expectedKeys -join ',')) {
    throw "Castaway level 2 key route is $($keys -join ','), expected $($expectedKeys -join ',')"
}

Write-Host 'PASS: Castaway level 2 derives blue, gold, and red prerequisites'
