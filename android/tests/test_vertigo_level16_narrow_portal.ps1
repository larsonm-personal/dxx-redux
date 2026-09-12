#!/usr/bin/env pwsh
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$runner = Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1'
$output = Join-Path $repoRoot "android/temp/test_vertigo_level16_narrow_portal/run_$(Get-Date -Format 'yyyyMMdd_HHmmss_fff')"
$cases = @(
    @{
        Mission = 'CD - Descent II - The Vertigo Series (USA).json'; Level = 16; File = 'd2xlvl16.rl2'
        Sequence = 'Shoot switch trigger 5|blue key|gold key|Pass through trigger 18|Shoot switch trigger 8|Shoot switch trigger 7|Fly-through trigger 6|Reactor|Exit'
    },
    @{
        Mission = 'plutonia.json'; Level = 28; File = 'map28.rl2'
        Sequence = 'Shoot switch trigger 1|Shoot switch trigger 2|blue key|Shoot switch trigger 5|red key|Reactor|Exit'
    }
)
foreach ($case in $cases) {
    $caseOutput = Join-Path $output "level_$($case.Level)"
    & (Get-Process -Id $PID).Path -NoProfile -File $runner -Mode Headless -MissionJson $case.Mission `
        -Level $case.Level -Repeat 2 -MaxParallel 1 -NoBuild:$NoBuild -OutputRoot $caseOutput
    if ($LASTEXITCODE -ne 0) { throw "Simulation runner failed: $($case.Mission) L$($case.Level)" }
    $NoBuild = $true
    $results = @(Get-ChildItem -LiteralPath (Join-Path $caseOutput 'results') `
            -Filter "*_0_$($case.Level)_$($case.File)_run_*.json" |
            Where-Object Name -Match '_run_[0-9]+\.json$' | Sort-Object Name)
    if ($results.Count -ne 2) { throw "Expected two results, found $($results.Count)" }
    $first = $null
    foreach ($file in $results) {
        $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
        if ($result.status -ne 'confirmed' -or ($result.objectives.label -join '|') -cne $case.Sequence) {
            throw "Incomplete route: $($case.Mission) L$($case.Level), $($result.status)"
        }
        $signature = $result | ConvertTo-Json -Depth 30 -Compress
        if ($null -eq $first) { $first = $signature }
        elseif ($signature -cne $first) { throw "Nondeterministic route: $($case.Mission) L$($case.Level)" }
        if ($case.Level -eq 16) {
            $log = Get-Content -LiteralPath (Join-Path $caseOutput "logs/$($file.BaseName).log") -Raw
            if ($log -notmatch '(?s)complete step=1.*label=Shoot switch trigger 5.*complete step=2.*label=blue key.*complete step=7.*label=Exit') {
                throw 'Vertigo 16 must physically activate the alternate entrance before collecting the blue key'
            }
        }
    }
    $normalizedName = [IO.Path]::GetFileNameWithoutExtension($case.Mission) + '.simulation.json'
    $normalized = Get-Content -LiteralPath (Join-Path $caseOutput "results/$normalizedName") -Raw | ConvertFrom-Json
    $level = @($normalized | Where-Object target_index -eq 0 | ForEach-Object levels | Where-Object level_num -eq $case.Level)
    if ($level.Count -ne 1 -or $level[0].status -ne 'ok') { throw 'Physical objectives disagree with metadata' }
    Write-Host "PASS $($case.Mission) L$($case.Level): $($result.frames) frames, deterministic, metadata agrees"
}
