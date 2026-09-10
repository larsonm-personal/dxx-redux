#!/usr/bin/env pwsh
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$output = Join-Path $repoRoot "android/temp/test_guided_shot_annotations/run_$(Get-Date -Format 'yyyyMMdd_HHmmss_fff')"
$metadataOutput = Join-Path $output 'metadata_run'
$metadataRunner = Join-Path $repoRoot 'android/helpers/regenerate_all_mission_metadata_host.ps1'
$simulationRunner = Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1'
& $metadataRunner -ArchiveNames @('plutonia.zip', 'bitesize.zip') -MaxParallel 1 -NoBuild:$NoBuild -NoRegressionCopy -OutputRoot $metadataOutput
if ($LASTEXITCODE -ne 0) { throw 'Guided annotation metadata generation failed' }

$cases = @(
    @{ Mission = 'plutonia'; Level = 4; Kind = 'hidden_door' },
    @{ Mission = 'bitesize'; Level = -3; Kind = 'trigger' },
    @{ Mission = 'bitesize'; Level = -1; Kind = 'hidden_door'; ReclosedDoor = $true }
)
foreach ($case in $cases) {
    $metadata = Get-Content -LiteralPath (Join-Path $metadataOutput "metadata/$($case.Mission).json") -Raw | ConvertFrom-Json
    $level = @($metadata | Where-Object target_index -eq 0 | ForEach-Object levels | Where-Object level_num -eq $case.Level)
    if ($level.Count -ne 1) { throw 'Expected one level metadata entry' }
    $guided = @($level[0].route_steps | Where-Object required_weapon -eq 'guided_missile')
    if ($guided.Count -eq 0 -or $case.Kind -notin $guided.kind) { throw "$($case.Mission) L$($case.Level) lacks its guided shot annotation" }
    foreach ($step in $guided) {
        if ($step.label -cne 'Shoot using guided missile') { throw 'Guided metadata label lost its weapon requirement' }
    }
    $caseOutput = Join-Path $output "$($case.Mission)_$($case.Level)"
    & $simulationRunner -MissionJson "$($case.Mission).json" -Level $case.Level -Repeat 2 -MaxParallel 1 -NoBuild -OutputRoot $caseOutput
    if ($LASTEXITCODE -ne 0) { throw 'Guided annotation simulation runner failed' }
    $logs = @(Get-ChildItem -LiteralPath (Join-Path $caseOutput 'logs') -Filter '*_run_*.log')
    if ($logs.Count -ne 2) { throw 'Expected two repeated simulation logs' }
    foreach ($file in $logs) {
        $log = Get-Content -LiteralPath $file.FullName -Raw
        if ($case.ReclosedDoor -and $log -notmatch '(?s)implicit step=.*?replan closed remote door wall=19 remote=23 actor_seg=69.*?guided wall=23') {
            throw 'Reclosed reverse door did not restore its previously completed guided action'
        }
        if ($log -notmatch 'ROUTE-CONFIRM guided .*instruction=shoot using guided missile') {
            throw "$($case.Mission) L$($case.Level) did not publish the guided missile instruction"
        }
    }
    Write-Host "PASS $($case.Mission) L$($case.Level): required weapon metadata and Guide-Bot instruction"
}
# These checks verify annotations, not complete level routes or missile flight
Write-Host "Guided annotation reports: $output"
