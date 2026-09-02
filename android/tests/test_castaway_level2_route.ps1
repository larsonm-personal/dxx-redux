#!/usr/bin/env pwsh

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$metadataPath = Join-Path $repoRoot 'game_data\mission_files\castaway_redux.json'
$mission = @(Get-Content -LiteralPath $metadataPath -Raw | ConvertFrom-Json)
$level1 = @($mission[0].levels | Where-Object { $_.level_num -eq 1 })
$level = @($mission[0].levels | Where-Object { $_.level_num -eq 2 })
$level5 = @($mission[0].levels | Where-Object { $_.level_num -eq 5 })
$level9 = @($mission[0].levels | Where-Object { $_.level_num -eq 9 })

if ($level1.Count -ne 1) {
    throw "Expected exactly one Castaway level 1 record, found $($level1.Count)"
}
if ($level.Count -ne 1) {
    throw "Expected exactly one Castaway level 2 record, found $($level.Count)"
}
if ($level5.Count -ne 1) {
    throw "Expected exactly one Castaway level 5 record, found $($level5.Count)"
}
if ($level9.Count -ne 1) {
    throw "Expected exactly one Castaway level 9 record, found $($level9.Count)"
}
if ($level1[0].route_status -ne 'ok' -or
    -not [string]::IsNullOrEmpty([string]$level1[0].route_problem)) {
    throw "Castaway level 1 route is $($level1[0].route_status): $($level1[0].route_problem)"
}
$level1Objectives = @(
    $level1[0].route_steps | ForEach-Object {
        if ($_.kind -eq 'trigger') {
            "trigger:$($_.trigger)"
        } else {
            [string]$_.kind
        }
    }
)
$level1Trigger2 = [Array]::IndexOf($level1Objectives, 'trigger:2')
$level1Reactor = [Array]::IndexOf($level1Objectives, 'reactor')
$level1Exit = [Array]::IndexOf($level1Objectives, 'exit')
if ($level1Trigger2 -lt 0 -or $level1Reactor -le $level1Trigger2 -or
    $level1Exit -le $level1Reactor) {
    throw "Castaway level 1 route does not reopen trigger 2 before reactor and exit: $($level1Objectives -join ',')"
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
    'trigger:0'
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

if ($level5[0].route_status -ne 'partial' -or
    $level5[0].route_problem -ne 'switch activation route unresolved') {
    throw "Castaway level 5 should expose its unresolved switches, got $($level5[0].route_status): $($level5[0].route_problem)"
}
$unresolvedLevel5Triggers = @(
    $level5[0].route_steps |
        Where-Object { $_.activation_kind -eq 'unresolved_trigger' } |
        ForEach-Object { [int]$_.trigger }
)
if (($unresolvedLevel5Triggers -join ',') -ne '13,20') {
    throw "Castaway level 5 unresolved triggers are $($unresolvedLevel5Triggers -join ','), expected 13,20"
}

if ($level9[0].route_status -ne 'ok' -or
    -not [string]::IsNullOrEmpty([string]$level9[0].route_problem)) {
    throw "Castaway level 9 route is $($level9[0].route_status): $($level9[0].route_problem)"
}
if ([int]$level9[0].route_required_key_mask -ne 7 -or
    [int]$level9[0].route_completing_key_mask_set -ne 128) {
    throw "Castaway level 9 completion masks are $($level9[0].route_required_key_mask)/$($level9[0].route_completing_key_mask_set), expected 7/128"
}
$level9Objectives = @(
    $level9[0].route_steps | ForEach-Object {
        if ($_.kind -eq 'trigger') {
            "trigger:$($_.trigger)"
        } elseif ($_.kind -eq 'key') {
            "key:$($_.key)"
        } else {
            [string]$_.kind
        }
    }
)
$expectedLevel9Objectives = @(
    'start'
    'trigger:31'
    'key:blue'
    'key:gold'
    'trigger:14'
    'key:red'
    'trigger:26'
    'reactor'
    'exit'
)
if (($level9Objectives -join ',') -ne ($expectedLevel9Objectives -join ',')) {
    throw "Castaway level 9 route is $($level9Objectives -join ','), expected $($expectedLevel9Objectives -join ',')"
}

Write-Host 'PASS: Castaway levels 1, 2, 5, and 9 retain reviewed route behavior'
