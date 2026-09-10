#!/usr/bin/env pwsh
param([switch]$NoBuild, [string]$HeadlessExecutable)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$output = Join-Path $repoRoot ('android/temp/test_crossfire_zero_countdown/' + (Get-Date -Format 'yyyyMMdd_HHmmss'))
$parameters = @{ MissionJson = 'D2Crossfire.json'; Level = 15; Repeat = 2; MaxParallel = 1; OutputRoot = $output; NoBuild = $NoBuild }
if ($HeadlessExecutable) { $parameters.HeadlessExecutable = $HeadlessExecutable }
# A prior shell failure must not leak into a successful batch's exit status
$global:LASTEXITCODE = 1
& (Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1') @parameters
if ($LASTEXITCODE -ne 0) { throw 'Crossfire zero-countdown simulation infrastructure failed' }
$files = @(Get-ChildItem -LiteralPath (Join-Path $output 'results') -Filter '*_run_*.json' |
        Where-Object Name -Match '_run_[0-9]+\.json$' | Sort-Object Name)
if ($files.Count -ne 2) { throw 'Expected two repeated physical runs' }
foreach ($file in $files) {
    $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    if ($result.status -ne 'confirmed' -or ($result.objectives.label -join '|') -cne 'blue key|gold key|red key|Reactor|Exit') {
        throw 'Crossfire L15 must complete the keys, reactor and exit'
    }
    $log = Get-Content -LiteralPath (Join-Path $output ('logs/' + $file.BaseName + '.log')) -Raw
    if ($log -notmatch 'sandbox pause expired reactor countdown ticks=0 base_seconds=0' -or $log -match 'AddressSanitizer|runtime error:') {
        throw 'Expected the authored zero timer to be sandboxed without a native memory diagnostic'
    }
}
if ((Get-FileHash $files[0].FullName).Hash -ne (Get-FileHash $files[1].FullName).Hash) {
    throw 'Crossfire repeated runs differ'
}
$normalized = Get-Content -LiteralPath (Join-Path $output 'results/D2Crossfire.simulation.json') -Raw | ConvertFrom-Json
$level = @($normalized | Where-Object target_index -eq 0 | ForEach-Object levels | Where-Object level_num -eq 15)
if ($level.Count -ne 1 -or $level[0].status -ne 'ok') { throw 'Crossfire physical completion was not recorded as ok' }
if ($level[0].reactor_escape.countdown_seconds -ne 0 -or
    $level[0].reactor_escape.simulated_seconds -lt 7 -or
    @($level[0].notes | Where-Object { $_ -like 'Likely insufficient reactor escape time:*' }).Count -ne 1) {
    throw 'Crossfire zero countdown must carry an advisory escape timing note and measured evidence'
}
Write-Host "PASS Crossfire L15: zero reactor timer, repeatable completion in $($result.frames) frames"
