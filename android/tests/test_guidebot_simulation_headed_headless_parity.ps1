#!/usr/bin/env pwsh

[CmdletBinding()]
param(
    [switch]$Build,
    [string]$HeadlessRun,
    [string]$HeadedRun
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$runner = Join-Path $repoRoot 'android\helpers\regenerate_all_guidebot_simulations.ps1'
$runsRoot = Join-Path $repoRoot 'android\temp\guidebot_simulation_regression'

function Invoke-ParityRun {
    param([ValidateSet('Headless', 'Headed')][string]$Mode)
    $started = [DateTime]::Now
    $pwsh = (Get-Process -Id $PID).Path
    if ($Build) {
        & $pwsh -NoProfile -File $runner -Mode $Mode -MissionJson Counterstrike.json -Level 1 -Repeat 2 |
            ForEach-Object { Write-Host $_ }
    } else {
        & $pwsh -NoProfile -File $runner -Mode $Mode -MissionJson Counterstrike.json -Level 1 -Repeat 2 -NoBuild |
            ForEach-Object { Write-Host $_ }
    }
    if ($LASTEXITCODE -ne 0) { throw "$Mode parity run failed" }
    $run = Get-ChildItem -LiteralPath $runsRoot -Directory | Where-Object { $_.LastWriteTime -ge $started } |
        Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if (-not $run) { throw "Could not find $Mode parity artifacts" }
    return $run
}

function Get-OnlyLevelResult {
    param([IO.DirectoryInfo]$Run)
    $path = Join-Path $Run.FullName 'results\Counterstrike.simulation.json'
    $record = Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
    return @($record.levels | Where-Object { $_.level_num -eq 1 })[0]
}

function Get-SimulationDocument {
    param([IO.DirectoryInfo]$Run)
    return Get-Content -LiteralPath (Join-Path $Run.FullName 'results\Counterstrike.simulation.json') -Raw | ConvertFrom-Json
}

function Test-MonotonicTimes {
    param([object]$Result, [string]$Mode)
    $previous = -1.0
    foreach ($objective in @($Result.objectives)) {
        $seconds = $objective.s
        if ([double]$seconds -ne [Math]::Truncate([double]$seconds)) { throw "$Mode objective time is not an integer" }
        if ([double]$seconds -lt $previous) { throw "$Mode objective times are not monotonic" }
        $previous = [double]$seconds
    }
    if ([int]$Result.total_frames -le 0) { throw "$Mode total frames are invalid" }
}

$headlessRunDir = if ($HeadlessRun) { Get-Item -LiteralPath $HeadlessRun } else { Invoke-ParityRun -Mode Headless }
$headedRunDir = if ($HeadedRun) { Get-Item -LiteralPath $HeadedRun } else { Invoke-ParityRun -Mode Headed }
$headlessDocument = Get-SimulationDocument -Run $headlessRunDir
$headedDocument = Get-SimulationDocument -Run $headedRunDir
$headless = Get-OnlyLevelResult -Run $headlessRunDir
$headed = Get-OnlyLevelResult -Run $headedRunDir

foreach ($property in @('schema', 'generation', 'fixed_hz', 'seed')) {
    if ([string]$headlessDocument.$property -cne [string]$headedDocument.$property) { throw "Parity mismatch: $property" }
}

foreach ($property in @('route_input_sha256', 'status')) {
    if ([string]$headless.$property -cne [string]$headed.$property) { throw "Parity mismatch: $property" }
}
if ($headless.status -ne 'ok') { throw 'Counterstrike level 1 did not confirm in both modes' }
if (($headless.rng_start | ConvertTo-Json -Compress) -cne ($headed.rng_start | ConvertTo-Json -Compress)) {
    throw 'Parity mismatch: starting RNG boundary'
}
if ((@($headless.objectives.n) -join "`0") -cne (@($headed.objectives.n) -join "`0")) {
    throw 'Parity mismatch: compact objective names'
}
Test-MonotonicTimes -Result $headless -Mode Headless
Test-MonotonicTimes -Result $headed -Mode Headed

$headlessRawFile = @(Get-ChildItem (Join-Path $headlessRunDir.FullName 'results') -Filter '*_run_1.json')[0]
$headedRawFile = @(Get-ChildItem (Join-Path $headedRunDir.FullName 'results') -Filter '*_headed_1.json')[0]
$headlessRaw = Get-Content -LiteralPath $headlessRawFile.FullName -Raw | ConvertFrom-Json
$headedRaw = Get-Content -LiteralPath $headedRawFile.FullName -Raw | ConvertFrom-Json
$headlessObjectives = @($headlessRaw.objectives)
$headedObjectives = @($headedRaw.objectives)
foreach ($property in @('seed', 'fixed_hz', 'difficulty')) {
    if ($null -eq $headlessRaw.PSObject.Properties[$property] -or
        $null -eq $headedRaw.PSObject.Properties[$property] -or
        [string]$headlessRaw.$property -cne [string]$headedRaw.$property) {
        throw "Parity mismatch: detailed $property"
    }
}
if ($headlessObjectives.Count -ne $headedObjectives.Count) { throw 'Parity mismatch: objective count' }
for ($index = 0; $index -lt $headlessObjectives.Count; $index++) {
    foreach ($property in @('route_step_index', 'kind', 'activation_kind')) {
        if ([string]$headlessObjectives[$index].$property -cne [string]$headedObjectives[$index].$property) {
            throw "Parity mismatch: objective $index $property"
        }
    }
}

$report = [ordered]@{
    schema = 'dxx-guidebot-route-simulation-parity-v1'
    mission = 'Counterstrike'
    level = 1
    seed = 1
    fixed_hz = 60
    status = 'ok'
    headless_frames = [int]$headless.total_frames
    headed_frames = [int]$headed.total_frames
    frame_delta = [int]$headed.total_frames - [int]$headless.total_frames
    headless_objectives = @($headless.objectives)
    headed_objectives = @($headed.objectives)
    headless_run = $headlessRunDir.FullName
    headed_run = $headedRunDir.FullName
}
$reportPath = Join-Path $headedRunDir.FullName 'headed_headless_parity.json'
[IO.File]::WriteAllText($reportPath, (($report | ConvertTo-Json -Depth 8) + "`n"), [Text.UTF8Encoding]::new($false))
Write-Host "GuideBot headed/headless semantic parity passed: $reportPath"
