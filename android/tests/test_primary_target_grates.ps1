#!/usr/bin/env pwsh
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$runner = Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1'
$output = Join-Path $repoRoot "android/temp/test_primary_target_grates/run_$(Get-Date -Format 'yyyyMMdd_HHmmss_fff')"
$cases = @(
    @{ Mission = 'CD - Descent II - The Vertigo Series (USA)'; Level = 6; Sequence = 'blue key|gold key|red key|Reactor|Exit' },
    @{ Mission = 'plutonia'; Level = 1; Sequence = 'Shoot switch trigger 1|blue key|Shoot switch trigger 3|gold key|Shoot switch trigger 4|red key|Reactor|Shoot switch trigger 2|Exit' },
    @{ Mission = 'plutonia'; Level = 29; Sequence = 'gold key|blue key|red key|Boss robot|Exit' },
    @{ Mission = 'Vignettes'; Level = 14; Sequence = 'red key|Boss robot|Exit' },
    @{ Mission = 'Vignettes'; Level = 17; Sequence = 'red key|Reactor|Exit' }
)
foreach ($case in $cases) {
    $caseOutput = Join-Path $output "$($case.Mission)_$($case.Level)"
    & (Get-Process -Id $PID).Path -NoProfile -File $runner -MissionJson "$($case.Mission).json" `
        -Level $case.Level -Repeat 2 -MaxParallel 1 -NoBuild:$NoBuild -OutputRoot $caseOutput
    if ($LASTEXITCODE -ne 0) { throw 'Primary target simulation runner failed' }
    $NoBuild = $true
    $resultPrefix = [regex]::Replace("$($case.Mission).json|0|$($case.Level)|", '[^A-Za-z0-9_.-]+', '_')
    # Recursive mission matching may also include a separately catalogued archive
    $results = @(Get-ChildItem -LiteralPath (Join-Path $caseOutput 'results') -Filter '*_run_*.json' |
            Where-Object { $_.Name.StartsWith($resultPrefix, [StringComparison]::Ordinal) -and $_.Name -match '_run_[0-9]+\.json$' } |
            Sort-Object Name)
    if ($results.Count -ne 2) { throw 'Expected two repeated runs' }
    $first = $null
    foreach ($file in $results) {
        $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
        if ($result.status -ne 'confirmed' -or ($result.objectives.label -join '|') -cne $case.Sequence) {
            throw "$($case.Mission) L$($case.Level) did not complete its primary target route"
        }
        $log = Get-Content -LiteralPath (Join-Path $caseOutput "logs/$($file.BaseName).log") -Raw
        if ($log -notmatch 'ROUTE-CONFIRM verified primary shot actor_seg=\d+ target_seg=\d+ object=\d+') {
            throw 'The proof must verify the primary shot from the actual actor position'
        }
        $signature = $result | ConvertTo-Json -Depth 30 -Compress
        if ($null -eq $first) { $first = $signature }
        elseif ($signature -cne $first) { throw 'Nondeterministic primary target result' }
    }
    $normalized = Get-Content -LiteralPath (Join-Path $caseOutput "results/$($case.Mission).simulation.json") -Raw | ConvertFrom-Json
    $entry = @($normalized | Where-Object target_index -eq 0 | ForEach-Object levels | Where-Object level_num -eq $case.Level)
    if ($entry.Count -ne 1 -or $entry[0].status -ne 'ok') { throw 'Physical completion disagrees with metadata' }
    Write-Host "PASS $($case.Mission) L$($case.Level): primary target shot, $($result.frames) frames"
}
