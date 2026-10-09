#!/usr/bin/env pwsh
# Exercise the real metadata worker with original Vertigo media, including worker reuse
param([switch]$NoBuild)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $repoRoot 'android/helpers/standard_game_data.ps1')
. (Join-Path $repoRoot 'android/helpers/test_host_platform.ps1')
. (Join-Path $repoRoot 'android/helpers/host_metadata_worker.ps1')
if (-not $NoBuild) { Invoke-RegressionHostBuild -RepoRoot $repoRoot -Target d2 }
$root = Join-Path $repoRoot ('android/temp/mission-movies/run_' + [guid]::NewGuid().ToString('N'))
& (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $root -DirectoryPrefix run_
$stage = Join-Path $root 'mission'
New-Item -ItemType Directory -Force $stage | Out-Null
$globalMedia = Join-Path $root 'installed-media'
New-Item -ItemType Directory -Force $globalMedia | Out-Null
$source = Join-Path $repoRoot 'game_data/extracted/VERTIGO'
foreach ($name in @('D2X.MN2', 'D2X.HOG')) {
    Copy-Item -LiteralPath (Join-Path $source "MISSIONS/$name") -Destination (Join-Path $stage $name)
}
$deps = @(Get-StandardGameDataDeps)
$names = @('descent2.hog', 'descent2.ham', 'groupa.pig', 'descent2.s22')
$data = Resolve-StandardGameDataDirectory -Candidates @(Get-StandardGameDataCandidates -RepoRoot $repoRoot -Game d2) -Dependencies @($deps | Where-Object file -in $names) -Label d2
$executable = Join-Path $repoRoot "buildd2/main/$((Get-RegressionHostExecutableNames -BaseName 'dxx-redux-d2-metadata-worker')[0])"
$worker = New-MetadataWorker -Executable $executable
try {
    foreach ($case in @('missing', 'low', 'removed', 'high', 'disabled')) {
        if ($case -eq 'low') {
            Copy-Item -LiteralPath (Join-Path $source 'D2X-L.MVL') -Destination $stage
        } elseif ($case -eq 'removed') {
            Remove-Item -LiteralPath (Join-Path $stage 'D2X-L.MVL')
        } elseif ($case -eq 'high') {
            Copy-Item -LiteralPath (Join-Path $source 'D2X-H.MVL') -Destination $globalMedia
        }
        $request = @{
            request_id = "movies-$case"; game = 'd2'; content_game = 'd2'; source_type = 'mission_files'
            source_name = 'Vertigo'; data_dir = $data.Path; extra_data_dir = $stage
            mission_name = 'd2x'; mission_filename = 'D2X.MN2'; movies_only = $true
            hog_paths = @((Join-Path $stage 'D2X.HOG'))
            movie_search_paths = @(if ($case -eq 'high') { $globalMedia })
        }
        $result = Invoke-MetadataWorker -Worker $worker -Request $request -RawOutputPath (Join-Path $root "$case.json") -LogPath (Join-Path $root "$case.log") -TimeoutSeconds 45
        if ($result.status -ne 'ok' -or $result.movies.status -ne 'ok') { throw "Movie scan failed: $case; see $root" }
        if ($result.movies.required.Count -ne 10 -or $result.levels.Count -gt 0) { throw "Expected ten movie references without level analysis: $case" }
        $expectedMissing = if ($case -in @('missing', 'removed', 'disabled')) { 10 } else { 0 }
        if ($result.movies.missing.Count -ne $expectedMissing) { throw "Expected $expectedMissing missing movies: $case; see $root" }
        if ($case -eq 'missing' -and ('rb3.mve' -notin $result.movies.missing -or 'rb9.mve' -notin $result.movies.missing)) {
            throw 'Level 1 robot movies were not detected'
        }
        Write-Host "PASS $case : $expectedMissing missing movies"
    }
} finally {
    Stop-MetadataWorkerProcess -Worker $worker
}
Write-Host "Mission movie scan evidence: $root"
