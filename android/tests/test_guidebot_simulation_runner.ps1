#!/usr/bin/env pwsh

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$runner = Join-Path $repoRoot 'android\helpers\regenerate_all_guidebot_simulations.ps1'
$tempRoot = Join-Path $repoRoot 'android\temp\guidebot_simulation_runner_test'
New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null

function Invoke-DryRun {
    param(
        [string]$Name,
        [string[]]$MissionJson,
        [int[]]$Level,
        [double]$SampleFraction = 1,
        [int]$SampleSeed = 0,
        [string]$SampleStatePath,
        [ValidateSet('Headless', 'Headed', 'Desktop')][string]$Mode = 'Headless'
    )
    $output = Join-Path $tempRoot "$Name.json"
    $parameters = @{ DryRun = $true; NoBuild = $true; DryRunJsonOut = $output; Mode = $Mode }
    if ($MissionJson) { $parameters.MissionJson = $MissionJson }
    if ($Level) { $parameters.Level = $Level }
    if ($SampleFraction -lt 1) { $parameters.SampleFraction = $SampleFraction; $parameters.SampleSeed = $SampleSeed }
    if ($SampleStatePath) { $parameters.SampleStatePath = $SampleStatePath }
    & $runner @parameters | Out-Null
    return @(Get-Content -LiteralPath $output -Raw | ConvertFrom-Json)
}

try {
    $runnerSource = Get-Content -LiteralPath $runner -Raw
    if ($runnerSource -notmatch '(?s)function Invoke-GuidebotHeadlessLevel.*?Start-Process.*?-NoNewWindow' -or
        $runnerSource -match '(?s)function Invoke-GuidebotDesktopLevel.*?Start-Process.*?-NoNewWindow') {
        throw 'Headless runs must reuse the parent console while Desktop runs retain their visible game window'
    }
    $filtered = @(Invoke-DryRun -Name filtered -MissionJson Counterstrike.json -Level 1, 3)
    if ($filtered.Count -ne 2 -or ($filtered.identity -join ',') -notmatch '\|1\|' -or
        ($filtered.identity -join ',') -notmatch '\|3\|') {
        throw 'Mission and level dry-run filtering is incorrect'
    }
    $desktop = @(Invoke-DryRun -Name desktop -MissionJson Counterstrike.json -Level 1 -Mode Desktop)
    $expectedDesktop = @($filtered | Where-Object { $_.identity -match '\|1\|' })
    if (($desktop.identity -join ',') -cne ($expectedDesktop.identity -join ',')) {
        throw 'Desktop dry-run discovery differs from the canonical runner'
    }
    $multiDescriptor = @(Invoke-DryRun -Name multi -MissionJson '-MOON-.json')
    if ($multiDescriptor.Count -ne 14 -or @($multiDescriptor.identity | Select-Object -Unique).Count -ne 14) {
        throw 'Multi-descriptor mission work item expansion is incorrect'
    }
    $sampleA = @(Invoke-DryRun -Name sample_a -MissionJson Counterstrike.json -SampleFraction 0.25 `
            -SampleSeed 717 -SampleStatePath (Join-Path $tempRoot 'sample_a_state.json'))
    $sampleB = @(Invoke-DryRun -Name sample_b -MissionJson Counterstrike.json -SampleFraction 0.25 `
            -SampleSeed 717 -SampleStatePath (Join-Path $tempRoot 'sample_b_state.json'))
    if ($sampleA.Count -eq 0 -or ($sampleA.identity -join ',') -cne ($sampleB.identity -join ',')) {
        throw 'Hash-ring dry-run selection is not deterministic from equal fresh state'
    }
    Write-Host 'GuideBot simulation runner discovery and sampling passed'
} finally {
    Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
}
