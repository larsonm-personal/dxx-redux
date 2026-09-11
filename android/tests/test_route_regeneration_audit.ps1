#!/usr/bin/env pwsh
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$output = Join-Path $repoRoot ('android/temp/test_route_regeneration_audit/' + (Get-Date -Format 'yyyyMMdd_HHmmss'))
$archives = @('d2xxl_downloads/saturn.7z', 'vignett2.zip', 'Hydro.ZIP', 'Chasm.zip', 'bitesize.zip', 'Lostlvls.zip', 'plutonia.zip', 'entropy_v.1.0.zip') |
    ForEach-Object { Join-Path $repoRoot ('game_data/mission_files/' + $_) }
& (Join-Path $repoRoot 'android/helpers/regenerate_all_mission_metadata_host.ps1') `
    -ArchivePaths $archives -NoBuild:$NoBuild -NoRegressionCopy -MaxParallel 3 -OutputRoot (Join-Path $output 'metadata_run')
if ($LASTEXITCODE -ne 0) { throw 'Audit mission metadata infrastructure failed' }
$inputs = Join-Path $output 'inputs'
New-Item -ItemType Directory -Path $inputs -Force | Out-Null
foreach ($record in (Get-Content (Join-Path $output 'metadata_run/summary.json') -Raw | ConvertFrom-Json)) {
    if ($record.status -ne 'passed') { throw "Metadata failed: $($record.name)" }
    $base = [IO.Path]::GetFileNameWithoutExtension($record.zip)
    Copy-Item -LiteralPath $record.metadata_json -Destination (Join-Path $inputs ($base + '.json'))
    Copy-Item -LiteralPath $record.zip -Destination (Join-Path $inputs ([IO.Path]::GetFileName($record.zip)))
}
$cases = @(
    [pscustomobject]@{ Mission = 'saturn'; Level = 4 },
    [pscustomobject]@{ Mission = 'vignett2'; Level = 15 },
    [pscustomobject]@{ Mission = 'Hydro'; Level = 17 },
    [pscustomobject]@{ Mission = 'Hydro'; Level = 18 },
    [pscustomobject]@{ Mission = 'Chasm'; Level = 3 },
    [pscustomobject]@{ Mission = 'bitesize'; Level = -1 },
    [pscustomobject]@{ Mission = 'entropy_v.1.0'; Level = 5 }
)
foreach ($case in $cases) {
    $entries = @(Get-Content (Join-Path $inputs ($case.Mission + '.json')) -Raw | ConvertFrom-Json)
    $level = @($entries.levels | Where-Object level_num -EQ $case.Level)
    if ($level.Count -ne 1 -or $level[0].route_status -ne 'ok' -or
        @($level[0].route_steps | Where-Object required_weapon -EQ 'guided_missile').Count -ne 0) {
        throw "Expected an ordinary completing route: $($case.Mission) L$($case.Level)"
    }
    if ($case.Mission -eq 'saturn') {
        $keys = @($level[0].route_steps | Where-Object kind -EQ 'key' | ForEach-Object key | Sort-Object)
        if (($keys -join ',') -ne 'blue,gold,red') { throw 'Saturn must retain its three-key route before the countdown' }
    }
}
foreach ($case in @([pscustomobject]@{ Mission = 'Lostlvls'; Level = 21 }, [pscustomobject]@{ Mission = 'plutonia'; Level = 22 })) {
    $entries = @(Get-Content (Join-Path $inputs ($case.Mission + '.json')) -Raw | ConvertFrom-Json)
    $level = @($entries.levels | Where-Object level_num -EQ $case.Level)
    if ($level.Count -ne 1 -or $level[0].route_status -ne 'ok' -or
        @($level[0].route_steps | Where-Object required_weapon -EQ 'guided_missile').Count -ne 1) {
        throw "Ordinary search must not exhaust the budget before preserving a guided certificate: $($case.Mission) L$($case.Level)"
    }
}
& (Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1') `
    -MissionMetadataRoot $inputs -LevelFileFilter @('PROM-SW.RL2', 'level15.rl2', 'Level17.rl2', 'Level18.rl2', 'lavachsm.rl2', 'level1-s.rl2', 'csparto.rl2') `
    -NoBuild -Repeat 2 -MaxParallel 3 -OutputRoot (Join-Path $output 'ordinary_sim')
if ($LASTEXITCODE -ne 0) { throw 'Audit route simulation infrastructure failed' }
foreach ($case in $cases) {
    $entries = @(Get-Content (Join-Path $output ('ordinary_sim/results/' + $case.Mission + '.simulation.json')) -Raw | ConvertFrom-Json)
    $level = @($entries.levels | Where-Object level_num -EQ $case.Level)
    if ($level.Count -ne 1 -or $level[0].status -ne 'ok') {
        throw "Ordinary route did not complete: $($case.Mission) L$($case.Level)"
    }
}
& (Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1') `
    -MissionJson @('ironstar.json', 'FFYL.json') -LevelFileFilter @('chapel.rdl', 'PDL30.RL2') `
    -NoBuild -Repeat 2 -MaxParallel 3 -OutputRoot (Join-Path $output 'door_sim')
if ($LASTEXITCODE -ne 0) { throw 'Door recovery simulation infrastructure failed' }
foreach ($run in @('ordinary_sim', 'door_sim')) {
    $files = @(Get-ChildItem -LiteralPath (Join-Path $output ($run + '/results')) -Filter '*_run_*.json' |
            Where-Object Name -Match '_run_[0-9]+\.json$')
    if ($files.Count -eq 0) { throw "No physical evidence for $run" }
    foreach ($file in $files) {
        $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
        if ($result.status -ne 'confirmed' -or $result.radius.player -le 0 -or
            $result.radius.effective -lt $result.radius.player -or
            $result.radius.effective -lt $result.radius.guidebot -or
            $result.objectives[-1].activation_kind -ne 'enter_exit') {
            throw "Expected repeated full-radius completion: $($file.Name)"
        }
    }
}
Write-Host 'PASS ordinary shots before guided fallback, alternate authored exits, and physical door recovery'
