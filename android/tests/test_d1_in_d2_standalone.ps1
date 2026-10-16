#!/usr/bin/env pwsh
# Catalog owner for the isolated Android D1-only resource/gameplay scenario
param(
    [string]$D1DataDirectory,
    [string]$D2DataDirectory,
    [switch]$SharewareSmoke,
    [switch]$SharewareCampaign,
    [ValidateSet(0, 3, 7)][int]$SharewareLastLevel = 0,
    [string]$Serial,
    [string]$AdbPath = 'C:\local\android-sdk\platform-tools\adb.exe'
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
if (-not $D1DataDirectory) {
    . (Join-Path $repoRoot 'android/helpers/standard_game_data.ps1')
    $dependencies = @(Get-StandardGameDataDeps | Where-Object file -in @('descent.hog', 'descent.pig'))
    $D1DataDirectory = (Resolve-StandardGameDataDirectory `
            -Candidates @(Get-StandardGameDataCandidates -RepoRoot $repoRoot -Game d1) `
            -Dependencies $dependencies -Label 'native D1').Path
}
& (Join-Path $repoRoot 'android/helpers/test_d1_in_d2_android.ps1') `
    -D1DataDirectory $D1DataDirectory -D2DataDirectory $D2DataDirectory -Serial $Serial -AdbPath $AdbPath -SharewareSmoke:$SharewareSmoke -SharewareCampaign:$SharewareCampaign -SharewareLastLevel $SharewareLastLevel
