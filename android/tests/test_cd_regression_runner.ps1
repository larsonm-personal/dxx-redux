#!/usr/bin/env pwsh

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$runnerPath = Join-Path $repoRoot 'game_data\run_all_cd_regressions.ps1'
. $runnerPath
. (Join-Path $repoRoot 'game_data\disc_track_manifest.ps1')

function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) {
        throw $Message
    }
}

$defaultStages = @(Get-CdRegressionStages -RepoRoot $repoRoot)
Assert-True ($defaultStages.Count -eq 4) 'Default workflow should contain four stages'
Assert-True ($defaultStages[0].Arguments -contains '-Force') 'CD extraction should be forced by default'
Assert-True ($defaultStages[1].Script -eq (Join-Path $repoRoot 'game_data\extract_all_gog.ps1')) `
    'GOG extraction should refresh before validating shared extraction specs'
Assert-True ($defaultStages[1].Arguments -contains '-Force') 'GOG extraction should be forced by default'
Assert-True ($defaultStages.Name -notcontains 'Refresh regression specs') 'Default workflow must not rewrite its oracle'
Assert-True ($defaultStages[3].Arguments -contains '-All') 'Regression suite should run all specs'
Assert-True ($defaultStages[3].Arguments -contains '-BuildAndInstall') 'Regression suite should build and install the current APK'
Assert-True ($defaultStages[3].Arguments -contains '-RestartDevice') 'Regression suite should start from a clean emulator'
Assert-True ($defaultStages[3].Arguments -notcontains '-SkipLaunch') 'Regression suite should default to full launch mode'

$fileOnlyStages = @(Get-CdRegressionStages -RepoRoot $repoRoot -SkipLaunch)
Assert-True ($fileOnlyStages[3].Arguments -contains '-SkipLaunch') 'Explicit file-only mode should skip game launches'

$refreshStages = @(Get-CdRegressionStages -RepoRoot $repoRoot -RefreshOracle)
Assert-True ($refreshStages.Count -eq 5) 'Oracle refresh workflow should contain five stages'
Assert-True ($refreshStages[2].Name -eq 'Refresh regression specs') 'Oracle refresh stage should be explicit'
Assert-True ($refreshStages[2].Arguments -contains '-Force') 'Explicit oracle refresh should regenerate all specs'

$sampleListPath = Join-Path $repoRoot 'android\temp\cd_sample_specs.txt'
$sampleStages = @(Get-CdRegressionStages -RepoRoot $repoRoot -RefreshOracle -SpecListPath $sampleListPath)
Assert-True ($sampleStages[0].Arguments -contains $sampleListPath -and
    $sampleStages[1].Arguments -contains $sampleListPath -and
    $sampleStages[2].Arguments -contains $sampleListPath -and
    $sampleStages[4].Arguments -contains $sampleListPath) `
    'Sampled CD workflow should use one spec list for extraction, oracle refresh, and tests'
$sampledSpecs = @(New-CdRegressionSampleList -RepoRoot $repoRoot -Fraction 0.1 -Seed 123)
$sampledTypes = @($sampledSpecs | ForEach-Object { (Read-JsoncFile $_).source_type } | Sort-Object -Unique)
Assert-True ($sampledTypes -contains 'cd' -and $sampledTypes -contains 'gog' -and $sampledTypes -contains 'combined') `
    'CD sampling should preserve every source type'
Assert-True (($sampledSpecs -join ',') -eq ((New-CdRegressionSampleList `
                -RepoRoot $repoRoot -Fraction 0.1 -Seed 123) -join ',')) `
    'CD sampling should be reproducible'

$extractSuiteText = Get-Content -LiteralPath (Join-Path $repoRoot 'android\tests\test_all_extracts.ps1') -Raw
Assert-True ($extractSuiteText.Contains("Join-Path `$ReportDir 'summary.json'")) `
    'Extraction suite should save a machine-readable result summary'
Assert-True ($extractSuiteText.Contains("'logcat', '-d', '-v', 'time'")) `
    'Extraction suite should preserve logcat for failed sources'

$tempRoot = Join-Path $repoRoot 'android\temp\cd_regression_runner_test'
if (Test-Path -LiteralPath $tempRoot) {
    Remove-Item -LiteralPath $tempRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null

try {
    $logPath = Join-Path $tempRoot 'stages.log'
    $scripts = @()
    foreach ($definition in @(
            @{ Name = 'first'; ExitCode = 0 },
            @{ Name = 'second'; ExitCode = 7 },
            @{ Name = 'third'; ExitCode = 0 }
        )) {
        $scriptPath = Join-Path $tempRoot "$($definition.Name).ps1"
        $content = @"
param([string]`$LogPath)
Add-Content -LiteralPath `$LogPath -Value '$($definition.Name)'
exit $($definition.ExitCode)
"@
        [System.IO.File]::WriteAllText($scriptPath, $content, [System.Text.UTF8Encoding]::new($false))
        $scripts += [pscustomobject]@{
            Name = $definition.Name
            Script = $scriptPath
            Arguments = @($logPath)
        }
    }

    $failed = $false
    try {
        Invoke-CdRegressionStages -Stages $scripts
    } catch {
        $failed = $_.Exception.Message -match "stage 'second' failed with exit code 7"
    }
    Assert-True $failed 'Runner should report the failing stage and exit code'
    $executed = @(Get-Content -LiteralPath $logPath)
    Assert-True (($executed -join ',') -eq 'first,second') 'Runner should stop before the stage after a failure'
} finally {
    Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
}

$manifestTemp = Join-Path $repoRoot 'android\temp\disc_track_manifest_test'
Remove-Item -LiteralPath $manifestTemp -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path $manifestTemp -Force | Out-Null
try {
    $cue = Join-Path $manifestTemp 'disc.cue'
    @'
FILE "disc.bin" BINARY
  TRACK 01 MODE1/2352
  TRACK 02 AUDIO
'@ | Set-Content -LiteralPath $cue -Encoding UTF8
    $valid = @(
        [pscustomobject]@{ track = 2; type = 'audio'; sha1 = ('b' * 40) },
        [pscustomobject]@{ track = 1; type = 'data'; sha1 = ('a' * 40) }
    )
    $manifest = @(Get-ValidatedDiscTrackManifest -Manifest $valid -CuePath $cue)
    Assert-True ($manifest.Count -eq 2 -and $manifest[0].track -eq 1 -and $manifest[1].track -eq 2) `
        'Valid manifest should be returned in physical track order'

    foreach ($case in @(
            @($valid[0]),
            @($valid[0], $valid[0]),
            @([pscustomobject]@{ track = 1; type = 'data'; sha1 = ('A' * 40) }, $valid[0]),
            @([pscustomobject]@{ track = 1; type = 'audio'; sha1 = ('a' * 40) }, $valid[0]),
            @([pscustomobject]@{ track = 1; type = 'data'; sha1 = ('a' * 40); error = 'failed' }, $valid[0])
        )) {
        $failed = $false
        try { Get-ValidatedDiscTrackManifest -Manifest $case -CuePath $cue | Out-Null } catch { $failed = $true }
        Assert-True $failed 'Invalid track manifest was accepted'
    }
} finally {
    Remove-Item -LiteralPath $manifestTemp -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host 'CD regression runner tests passed' -ForegroundColor Green
exit 0
