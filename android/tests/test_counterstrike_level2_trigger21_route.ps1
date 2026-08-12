#!/usr/bin/env pwsh
param(
    [string]$Exe,
    [string]$DataDir,
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path (Split-Path $PSScriptRoot)
if (-not $Exe) {
    $Exe = Join-Path $repoRoot 'buildd2\main\dxx-redux-d2-headless-metadata.exe'
}
if (-not $DataDir) {
    $DataDir = Join-Path $repoRoot 'game_data_to_copy_to_emulator\temp'
}
if (-not $OutputPath) {
    $OutputPath = Join-Path $repoRoot 'android\temp\counterstrike_level2_trigger21_route.json'
}

if (-not (Test-Path -LiteralPath $Exe -PathType Leaf)) {
    throw "D2 headless metadata executable not found: $Exe"
}
if (-not (Test-Path -LiteralPath $DataDir -PathType Container)) {
    throw "D2 game data directory not found: $DataDir"
}

New-Item -ItemType Directory -Force -Path (Split-Path -Parent $OutputPath) | Out-Null
& $Exe -hogdir $DataDir -secretarea-json-out $OutputPath | Out-Host
if ($LASTEXITCODE -ne 0) {
    throw "Counterstrike metadata dump failed with exit code $LASTEXITCODE"
}

$metadata = Get-Content -LiteralPath $OutputPath -Raw | ConvertFrom-Json

function Assert-ShootSwitchSegment {
    param(
        [Parameter(Mandatory)]$Metadata,
        [Parameter(Mandatory)][int]$LevelNumber,
        [Parameter(Mandatory)][int]$Trigger,
        [Parameter(Mandatory)][int]$Segment
    )

    $matchingLevel = @($Metadata.levels | Where-Object { $_.level_num -eq $LevelNumber })
    if ($matchingLevel.Count -ne 1) {
        throw "Expected exactly one Counterstrike level $LevelNumber record, found $($matchingLevel.Count)"
    }
    $matchingStep = @($matchingLevel[0].route_steps | Where-Object { $_.trigger -eq $Trigger })
    if ($matchingStep.Count -ne 1) {
        throw "Expected exactly one level $LevelNumber trigger $Trigger route step, found $($matchingStep.Count)"
    }
    if ($matchingStep[0].activation_kind -ne 'shoot_switch' -or
        $matchingStep[0].calculated -eq $false -or
        $matchingStep[0].seg -ne $Segment) {
        throw "Level $LevelNumber trigger $Trigger is $($matchingStep[0].activation_kind) at segment $($matchingStep[0].seg), expected calculated shoot_switch at segment $Segment"
    }
}

$level = @($metadata.levels | Where-Object { $_.level_num -eq 2 })
if ($level.Count -ne 1) {
    throw "Expected exactly one Counterstrike level 2 record, found $($level.Count)"
}
$step = @($level[0].route_steps | Where-Object { $_.trigger -eq 21 })
if ($step.Count -ne 1) {
    throw "Expected exactly one trigger 21 route step, found $($step.Count)"
}
if ($step[0].activation_kind -ne 'shoot_switch') {
    throw "Trigger 21 route step is $($step[0].activation_kind), expected shoot_switch"
}
if ($step[0].calculated -eq $false) {
    throw 'Trigger 21 route step is still marked not calculated'
}
if ($step[0].seg -eq $step[0].opens[0].seg) {
    throw 'Trigger 21 still routes to its opened wall instead of a firing waypoint'
}

# These firing positions require crossing ordinary, visible doors which are
# unlocked, require no key, and therefore open when hit by a player weapon.
Assert-ShootSwitchSegment -Metadata $metadata -LevelNumber 2 -Trigger 17 -Segment 65
Assert-ShootSwitchSegment -Metadata $metadata -LevelNumber 11 -Trigger 10 -Segment 476

# Preferring keys over a transparent shortcut must not replace a calculated
# objective with the planner's unresolved-trigger fallback
Assert-ShootSwitchSegment -Metadata $metadata -LevelNumber 3 -Trigger 8 -Segment 297

$keyProgressionLevel = @($metadata.levels | Where-Object { $_.level_num -eq 14 })
if ($keyProgressionLevel.Count -ne 1) {
    throw "Expected exactly one Counterstrike level 14 record, found $($keyProgressionLevel.Count)"
}
$level14Keys = @(
    $keyProgressionLevel[0].route_steps |
        Where-Object { $_.kind -eq 'key' } |
        ForEach-Object { $_.key }
)
if (($level14Keys -join ',') -ne 'blue,gold,red') {
    throw "Counterstrike level 14 key route is $($level14Keys -join ','), expected blue,gold,red"
}
if ('blue key not necessary' -in @($keyProgressionLevel[0].notes)) {
    throw 'Counterstrike level 14 still identifies its preferred blue key as unnecessary'
}
if (@($keyProgressionLevel[0].route_steps | Where-Object { $_.trigger -eq 5 }).Count -ne 0) {
    throw 'Counterstrike level 14 still prefers the trigger 5 yellow-key shortcut'
}

$mixedKeyLevel = @($metadata.levels | Where-Object { $_.level_num -eq 20 })
if ($mixedKeyLevel.Count -ne 1 -or
    'gold key not necessary' -notin @($mixedKeyLevel[0].notes)) {
    throw 'Counterstrike level 20 does not identify its unused gold key'
}
$bypassableBlue = @(
    $mixedKeyLevel[0].route_steps |
        Where-Object { $_.kind -eq 'key' -and $_.key -eq 'blue' -and $_.can_be_bypassed -eq $true }
)
if ($bypassableBlue.Count -ne 1 -or
    'blue key not necessary' -in @($mixedKeyLevel[0].notes)) {
    throw 'Counterstrike level 20 conflates its bypassable blue key with its unnecessary gold key'
}

Write-Host "PASS Counterstrike transparent and shoot-open door firing waypoints"
