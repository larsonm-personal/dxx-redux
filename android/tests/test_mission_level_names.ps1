#!/usr/bin/env pwsh

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
. (Join-Path $repoRoot 'android/helpers/powershell_compat.ps1')
$metadataDir = Join-Path $repoRoot "game_data\mission_files"

function Assert-LevelName {
    param(
        [Parameter(Mandatory = $true)][string]$MetadataFile,
        [Parameter(Mandatory = $true)][int]$LevelNum,
        [Parameter(Mandatory = $true)][string]$Expected
    )

    $missions = @(ConvertFrom-CompatibleJsonItems -Json (Get-Content -LiteralPath (Join-Path $metadataDir $MetadataFile) -Raw))
    $levels = @($missions | ForEach-Object { @($_.levels) })
    $level = @($levels | Where-Object { $_.level_num -eq $LevelNum } | Select-Object -First 1)
    if ($level.Count -ne 1) {
        throw "$MetadataFile does not contain level $LevelNum"
    }
    if ([string]$level[0].level_name -cne $Expected) {
        throw "$MetadataFile level $LevelNum name is '$($level[0].level_name)', expected '$Expected'"
    }
}

Assert-LevelName -MetadataFile "HDVBETA2.json" -LevelNum 3 -Expected "Tricerius: Gore Acterain Test Facility"
Assert-LevelName -MetadataFile "outerrch11.json" -LevelNum 3 -Expected "Defense Outpost for Orcean Militaries"
Assert-LevelName -MetadataFile "Lunar Series Revamped.json" -LevelNum 1 -Expected "lo"
Assert-LevelName -MetadataFile "Lunar Series Revamped.json" -LevelNum 2 -Expected "lsl"
Assert-LevelName -MetadataFile "Lunar Series Revamped.json" -LevelNum 3 -Expected "lmb"

Write-Host "PASS: mission level names preserve full valid names and reject binary text"
