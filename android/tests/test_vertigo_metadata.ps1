#!/usr/bin/env pwsh
# Exercise real Vertigo data through a reused native metadata worker
param(
    [string]$DataDir = '',
    [string]$VertigoDir = '',
    [string]$Worker = '',
    [switch]$NoBuild
)
Set-StrictMode -Version 3.0
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $repoRoot 'android/helpers/test_host_platform.ps1')
. (Join-Path $repoRoot 'android/helpers/standard_game_data.ps1')
if (-not $DataDir) {
    $candidates = @(Get-StandardGameDataCandidates -RepoRoot $repoRoot -Game d2)
    $dependencies = @(Get-StandardGameDataDeps | Where-Object file -in @('descent2.hog', 'descent2.ham', 'groupa.pig'))
    $DataDir = (Resolve-StandardGameDataDirectory -Candidates $candidates -Dependencies $dependencies -Label D2).Path
}
if (-not $VertigoDir) { $VertigoDir = Join-Path $repoRoot 'game_data/CD images/Descent II - The Vertigo Series (USA)/data_tracks/vertigo' }
if (-not $Worker) {
    if (-not $NoBuild) { Invoke-RegressionHostBuild -RepoRoot $repoRoot -Target d2 }
    $Worker = Join-RegressionPath $repoRoot buildd2 main (Get-RegressionHostExecutableNames -BaseName 'dxx-redux-d2-metadata-worker')[0]
}
$fixture = Join-Path $repoRoot ('android/temp/vertigo_metadata_fixture/run_' + [guid]::NewGuid().ToString('N'))
& (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $fixture -DirectoryPrefix run_
New-Item -ItemType Directory -Path $fixture -Force | Out-Null
$fixtureLock = $null
$process = $null
try {
    $fixtureLock = [IO.File]::Open((Join-Path $fixture 'producer.lock'), [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    Copy-Item -LiteralPath (Join-Path $VertigoDir 'd2x.mn2') -Destination $fixture -Force
    [IO.File]::WriteAllBytes((Join-Path $fixture 'd2x.hog'), [Text.Encoding]::ASCII.GetBytes('DHF'))
    $collection = Join-Path $fixture 'collection'
    New-Item -ItemType Directory -Path $collection -Force | Out-Null
    foreach ($name in @('d2x.mn2', 'd2x.hog')) {
        Copy-Item -LiteralPath (Join-Path $VertigoDir $name) -Destination $collection -Force
    }
    [IO.File]::WriteAllText((Join-Path $collection 'unrelated.hog'), 'Unsupported unrelated archive')
    $start = [Diagnostics.ProcessStartInfo]::new($Worker)
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardInput = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    $process = [Diagnostics.Process]::Start($start)
    $stderr = $process.StandardError.ReadToEndAsync()
    foreach ($stage in @($fixture, $VertigoDir, $collection, $fixture, $VertigoDir)) {
        $request = @{
            schema = 'dxx-level-metadata-request-v1'; request_id = [guid]::NewGuid().ToString('N')
            game = 'd2'; source_type = 'mission_files'; source_name = 'Vertigo regression'
            data_dir = $DataDir; extra_data_dir = $stage; mission_name = 'd2x'
            mission_filename = 'd2x.mn2'; hog_paths = @()
            normal_level_files = @('d2xlvl01.rl2'); secret_level_files = @()
        }
        $process.StandardInput.WriteLine(($request | ConvertTo-Json -Compress))
        $result = $null
        while ($null -eq $result) {
            $read = $process.StandardOutput.ReadLineAsync()
            if (-not $read.Wait(120000)) { throw 'Vertigo metadata worker timed out' }
            $line = $read.Result
            if ($null -eq $line) { throw "Worker closed output: $($stderr.GetAwaiter().GetResult())" }
            if ($line.StartsWith("DXXMETA`t")) { $result = $line.Substring(8) | ConvertFrom-Json }
        }
        if ($stage -eq $fixture) {
            if ($result.status -ne 'failed' -or ($result.problems -join ' ') -notmatch 'd2x.ham') {
                throw "Missing HAM must fail cleanly: $($result | ConvertTo-Json -Depth 4 -Compress)"
            }
        } elseif ($result.status -ne 'ok' -or @($result.levels).Count -ne 1 -or $result.levels[0].status -ne 'ok') {
            throw "Valid descriptor-only Vertigo request failed: $($result | ConvertTo-Json -Depth 4 -Compress)"
        }
    }
    $process.StandardInput.Close()
    if (-not $process.WaitForExit(10000) -or $process.ExitCode -ne 0) { throw 'Worker did not exit cleanly' }
    Write-Output 'PASS: Vertigo descriptor-only loading, unrelated HOG isolation, missing HAM rejection, and worker mount cleanup'
} finally {
    try {
        if ($process) {
            if (-not $process.HasExited) { $process.Kill($true); $process.WaitForExit() }
            $process.Dispose()
        }
    } finally {
        if ($fixtureLock) { $fixtureLock.Dispose() }
        Remove-Item -LiteralPath $fixture -Recurse -Force
    }
}
