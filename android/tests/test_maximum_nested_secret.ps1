#!/usr/bin/env pwsh
param(
    [Parameter(Mandatory)][string]$DataDir,
    [string]$D2Exe,
    [string]$OutputDir
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
if (-not $D2Exe) { $D2Exe = Join-Path $repoRoot 'buildd2/main/dxx-redux-d2-headless-metadata.exe' }
if (-not $OutputDir) { $OutputDir = Join-Path $repoRoot 'android/temp/test_maximum_nested_secret' }
& (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $OutputDir
$missions = Join-Path $OutputDir 'missions'
New-Item -ItemType Directory -Force -Path $missions | Out-Null
Expand-Archive -LiteralPath (Join-Path $repoRoot 'game_data/mission_files/descent_maximum_fixed.zip') -DestinationPath $missions -Force
$previous = $null
foreach ($run in 1..2) {
    $output = Join-Path $OutputDir "level10-$run.json"
    & $D2Exe -hogdir $DataDir -extra-dir $OutputDir -mission max_f -level 10 -secretarea-json-out $output
    if ($LASTEXITCODE -ne 0) { throw 'Maximum level 10 metadata analysis failed' }
    $dump = Get-Content -LiteralPath $output -Raw | ConvertFrom-Json
    $level = $dump.levels[0]
    $closet = @($level.secrets | Where-Object { 147 -in $_.segments })
    if ($level.secret_count -ne 3 -or $closet.Count -ne 1) { throw 'Expected the nested closet and both existing secrets' }
    $secret = $closet[0]
    if (($secret.segments -join ',') -ne '147' -or $secret.entry_seg -ne 188 -or $secret.entry_side -ne 4) {
        throw 'The closet must remain separate from the surrounding progression area'
    }
    if (($secret.items.id -join ',') -ne '2,20' -or (($secret.items | ForEach-Object count) -join ',') -ne '1,1') {
        throw 'The closet must contain one shield boost and one smart missile'
    }
    if ($secret.entrances.Count -ne 1 -or $secret.entrances[0].wall_num -ne 91) {
        throw 'The nested secret must have its real hidden-door entrance for map labeling'
    }
    if (@($level.secrets | Where-Object { 274 -in $_.segments -or 247 -in $_.segments }).Count) {
        throw 'The hostage and red-key rooms must not be secrets'
    }
    $signature = $level.secrets | ConvertTo-Json -Depth 20 -Compress
    if ($null -ne $previous -and $signature -cne $previous) { throw 'Secret inventory is not deterministic' }
    $previous = $signature
}
Write-Host 'PASS Maximum L10: nested shield/smart-missile closet detected with entrance, deterministic'
