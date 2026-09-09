#!/usr/bin/env pwsh
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$runner = Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1'
$output = Join-Path $repoRoot "android/temp/test_entropy2_level5_key_carrier/run_$(Get-Date -Format 'yyyyMMdd_HHmmss_fff')"
$cases = @(
    @{
        Mission = 'Entropy2.json'; Level = 5
        Sequence = 'Destroy robot carrying red key|Pass through trigger 20|Shoot switch trigger 24|Shoot switch trigger 25|Shoot switch trigger 26|Shoot switch trigger 27|Shoot switch trigger 28|Boss robot|Exit'
    },
    @{
        Mission = 'Counterstrike.json'; Level = 10
        Sequence = 'Destroy robot carrying blue key|Destroy robot carrying gold key|Destroy robot carrying red key|Shoot switch trigger 18|Open hidden door|Shoot switch trigger 25|Reactor|Exit'
    }
)
foreach ($case in $cases) {
    $caseOutput = Join-Path $output "level_$($case.Level)"
    & (Get-Process -Id $PID).Path -NoProfile -File $runner -Mode Headless -MissionJson $case.Mission `
        -Level $case.Level -Repeat 2 -MaxParallel 1 -NoBuild:$NoBuild -OutputRoot $caseOutput
    if ($LASTEXITCODE -ne 0) { throw "Simulation runner failed: $($case.Mission)" }
    $NoBuild = $true
    $results = @(Get-ChildItem -LiteralPath (Join-Path $caseOutput 'results') -Filter '*_run_*.json' |
            Where-Object Name -Match '_run_[0-9]+\.json$' | Sort-Object Name)
    if ($results.Count -ne 2) { throw "Expected two results, found $($results.Count)" }
    $first = $null
    foreach ($file in $results) {
        $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
        if ($result.status -ne 'confirmed' -or ($result.objectives.label -join '|') -cne $case.Sequence) {
            throw "Incomplete carrier route: $($case.Mission) L$($case.Level), $($result.status)"
        }
        $signature = $result | ConvertTo-Json -Depth 30 -Compress
        if ($null -eq $first) { $first = $signature }
        elseif ($signature -cne $first) { throw "Nondeterministic carrier route: $($case.Mission)" }
    }
    $normalizedName = [IO.Path]::GetFileNameWithoutExtension($case.Mission) + '.simulation.json'
    $normalized = Get-Content -LiteralPath (Join-Path $caseOutput "results/$normalizedName") -Raw | ConvertFrom-Json
    $level = @($normalized | Where-Object target_index -eq 0 | ForEach-Object levels | Where-Object level_num -eq $case.Level)
    if ($level.Count -ne 1 -or $level[0].status -ne 'ok') { throw 'Physical objectives disagree with metadata' }
    Write-Host "PASS $($case.Mission) L$($case.Level): $($result.frames) frames, deterministic carrier drop and completion"
}
