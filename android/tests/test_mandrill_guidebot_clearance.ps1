#!/usr/bin/env pwsh
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$runner = Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1'
$output = Join-Path $repoRoot "android/temp/test_mandrill_guidebot_clearance/run_$(Get-Date -Format 'yyyyMMdd_HHmmss_fff')"
foreach ($level in @(2, 5)) {
    $caseOutput = Join-Path $output "level_$level"
    & (Get-Process -Id $PID).Path -NoProfile -File $runner -MissionJson Mandrill.json `
        -Level $level -Repeat 2 -MaxParallel 1 -NoBuild:$NoBuild -OutputRoot $caseOutput
    if ($LASTEXITCODE -ne 0) { throw "Simulation runner failed for Mandrill L$level" }
    $NoBuild = $true
    $results = @(Get-ChildItem -LiteralPath (Join-Path $caseOutput 'results') -Filter '*_run_*.json' |
            Where-Object Name -Match '_run_[0-9]+\.json$' | Sort-Object Name)
    if ($results.Count -ne 2) { throw 'Expected two repeated runs' }
    $first = $null
    foreach ($file in $results) {
        $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
        if ($result.radius.guidebot -le $result.radius.player) { throw 'Fixture must exercise a companion larger than the player' }
        if ($level -eq 2) {
            $sequence = 'Shoot switch trigger 1|Destroy robot carrying blue key|gold key|Shoot switch trigger 2|red key|Reactor|Exit'
            if ($result.status -ne 'confirmed' -or ($result.objectives.label -join '|') -cne $sequence) {
                throw 'Mandrill L2 did not complete its keyed route'
            }
        } elseif ('blue key' -cnotin $result.objectives.label) {
            # L5 retains a separate switch-shot dependency after this pickup
            throw 'Mandrill L5 could not reach the blue-key dead end'
        }
        $signature = $result | ConvertTo-Json -Depth 30 -Compress
        if ($null -eq $first) { $first = $signature }
        elseif ($signature -cne $first) { throw "Nondeterministic Mandrill L$level result" }
    }
    if ($level -eq 2) {
        $normalized = Get-Content -LiteralPath (Join-Path $caseOutput 'results/Mandrill.simulation.json') -Raw | ConvertFrom-Json
        $entry = @($normalized | Where-Object target_index -eq 0 | ForEach-Object levels | Where-Object level_num -eq 2)
        if ($entry.Count -ne 1 -or $entry[0].status -ne 'ok') { throw 'L2 physical completion disagrees with metadata' }
    }
    Write-Host "PASS Mandrill L${level}: repeated clearance coverage, $($result.frames) frames"
}
