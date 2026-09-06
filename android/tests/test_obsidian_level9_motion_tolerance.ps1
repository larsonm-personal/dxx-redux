#!/usr/bin/env pwsh
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$runner = Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1'
$output = Join-Path $repoRoot "android/temp/test_obsidian9_motion/run_$(Get-Date -Format 'yyyyMMdd_HHmmss_fff')"
& (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $output
if (!$NoBuild) {
    & (Get-Process -Id $PID).Path -NoProfile -File (Join-Path $repoRoot 'run-windows-build.ps1') -Target d2
    if ($LASTEXITCODE -ne 0) { throw 'Windows build failed' }
}
foreach ($speed in @(100, 120, 140, 160)) {
    $run = Join-Path $output "speed_$speed"
    & (Get-Process -Id $PID).Path -NoProfile -File $runner -MissionJson Obsidian.json `
        -Level 9 -Repeat 2 -TestSpeedPercent $speed -NoBuild -OutputRoot $run
    if ($LASTEXITCODE -ne 0) { throw "Runner failed at speed $speed" }
    $results = @(Get-ChildItem (Join-Path $run 'results') -Filter 'Obsidian.json_0_9_*_run_*.json' | Sort-Object Name)
    if ($results.Count -ne 2) { throw "Missing repeats at speed $speed" }
    $first = $null
    foreach ($file in $results) {
        $raw = Get-Content -LiteralPath $file.FullName -Raw
        $result = $raw | ConvertFrom-Json
        # This regression guards the doorway before the blue key, not later
        # independent planner/frontier failures. Never mark the whole level ok
        $blue = @($result.objectives | Where-Object label -eq 'blue key')
        if ($blue.Count -ne 1 -or $blue[0].seconds -gt 40) {
            throw "Pre-key doorway failed at speed $speed ($($result.status))"
        }
        if ($speed -ne 160 -and $result.test_speed_percent -ne $speed) {
            throw 'Engine did not honor the requested speed'
        }
        if ($null -eq $first) { $first = $raw }
        elseif ($raw -cne $first) { throw "Nondeterministic result at speed $speed" }
    }
    Write-Host "PASS speed=$speed blue=$($blue[0].seconds)s later_status=$($result.status), identical repeats"
}
