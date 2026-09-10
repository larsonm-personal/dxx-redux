#!/usr/bin/env pwsh
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$runner = Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1'
$output = Join-Path $repoRoot "android/temp/test_mandrill_guidebot_clearance/run_$(Get-Date -Format 'yyyyMMdd_HHmmss_fff')"
foreach ($level in @(1, 2, 5)) {
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
        } elseif ($level -eq 1) {
            $sequence = 'blue key|Fly-through trigger 0|red key|Boss robot|Exit'
            if ($result.status -ne 'confirmed' -or ($result.objectives.label -join '|') -cne $sequence) {
                throw 'Mandrill L1 did not complete its boss route'
            }
            $log = Get-Content -LiteralPath (Join-Path $caseOutput "logs/$($file.BaseName).log") -Raw
            $shot = [regex]::Match($log, 'ROUTE-CONFIRM verified primary shot actor_seg=(\d+) target_seg=285')
            if (-not $shot.Success -or [int]$shot.Groups[1].Value -eq 285) {
                throw 'Mandrill L1 must verify the boss shot from outside its room'
            }
        } else {
            $sequence = 'blue key|Shoot switch trigger 4|Shoot switch trigger 5|gold key|Shoot switch trigger 3|Shoot switch trigger 6|red key|Shoot switch trigger 0|Boss robot|Exit'
            if ($result.status -ne 'confirmed' -or ($result.objectives.label -join '|') -cne $sequence) {
                throw 'Mandrill L5 did not complete the switch and key prerequisites'
            }
            $log = Get-Content -LiteralPath (Join-Path $caseOutput "logs/$($file.BaseName).log") -Raw
            if ($log -notmatch 'ROUTE-CONFIRM verified switch shot .* wall=10' -or
                $log -notmatch 'ROUTE-CONFIRM verified switch shot .* wall=7' -or
                $log -notmatch 'ROUTE-CONFIRM verified switch shot .* wall=4') {
                throw 'Mandrill L5 must physically verify the shots through the grates'
            }
        }
        $signature = $result | ConvertTo-Json -Depth 30 -Compress
        if ($null -eq $first) { $first = $signature }
        elseif ($signature -cne $first) { throw "Nondeterministic Mandrill L$level result" }
    }
    $normalized = Get-Content -LiteralPath (Join-Path $caseOutput 'results/Mandrill.simulation.json') -Raw | ConvertFrom-Json
    $entry = @($normalized | Where-Object target_index -eq 0 | ForEach-Object levels | Where-Object level_num -eq $level)
    if ($entry.Count -ne 1 -or $entry[0].status -ne 'ok') { throw "L$level physical completion disagrees with metadata" }
    Write-Host "PASS Mandrill L${level}: repeated clearance coverage, $($result.frames) frames"
}
