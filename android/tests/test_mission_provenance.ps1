#!/usr/bin/env pwsh
param([switch]$NoBuild)
Set-StrictMode -Version 3.0
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
. (Join-Path $repoRoot 'android/helpers/host_metadata_worker.ps1')
. (Join-Path $repoRoot 'android/helpers/test_host_platform.ps1')
. (Join-Path $repoRoot 'android/helpers/standard_game_data.ps1')
$missing = @('ROGUE.zip', 'chron10b.zip' | Where-Object {
        -not (Test-Path -LiteralPath (Join-Path $repoRoot "game_data/mission_files/$_") -PathType Leaf)
    })
if ($missing.Count) {
    Write-Host "RESULT: SKIP (provenance requires missing mission archives: $($missing -join ', '))"
    exit 2
}
Initialize-RegressionJavaEnvironment -RepoRoot $repoRoot
$java = Join-RegressionPath $env:JAVA_HOME bin (Get-RegressionHostExecutableNames -BaseName java)[0]
$dataDirs = @{}
foreach ($game in @('d1', 'd2')) {
    if (-not $NoBuild) { Invoke-RegressionHostBuild -RepoRoot $repoRoot -Target $game }
    $names = if ($game -eq 'd1') { @('descent.hog', 'descent.pig') } else { @('descent2.hog', 'descent2.ham', 'groupa.pig') }
    $dataDirs[$game] = (Resolve-StandardGameDataDirectory -Candidates @(Get-StandardGameDataCandidates -RepoRoot $repoRoot -Game $game) -Dependencies @(Get-StandardGameDataDeps | Where-Object file -in $names) -Label $game).Path
}
if (-not $NoBuild) {
    & (Resolve-RegressionGradleWrapper -AndroidDir (Join-Path $repoRoot android)) -p (Join-Path $repoRoot android) :mission-metadata-cli:installDist
    if ($LASTEXITCODE -ne 0) { throw 'Metadata CLI build failed' }
}
$cliLibraries = Join-Path $repoRoot 'android/mission-metadata-cli/build/install/mission-metadata-cli/lib/*'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$root = Join-Path $repoRoot ('android/temp/test_mission_provenance/run_' + [guid]::NewGuid().ToString('N'))
& (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $root -DirectoryPrefix run_
New-Item -ItemType Directory -Force -Path $root | Out-Null
$fixtureLock = $null
try {
    $fixtureLock = [IO.File]::Open((Join-Path $root 'producer.lock'), [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    $archive = Get-Item (Join-Path $repoRoot 'game_data/mission_files/ROGUE.zip')
    $dateJson = & $java -cp $cliLibraries com.dxxredux.metadata.cli.MainKt --archive-dates $archive.FullName
    if ($LASTEXITCODE -ne 0) { throw 'Shared archive date reader failed' }
    $dates = $dateJson | ConvertFrom-Json -AsHashtable
    $stage = Join-Path $root 'stage'
    [IO.Compression.ZipFile]::ExtractToDirectory($archive.FullName, $stage, $true)
    $descriptor = Join-Path $stage 'ROG.MSN'
    $original = [IO.File]::ReadAllText($descriptor)
    $worker = New-MetadataWorker -Executable (Join-RegressionPath $repoRoot buildd1 main (Get-RegressionHostExecutableNames -BaseName 'dxx-redux-d1-metadata-worker')[0])
    try {
        foreach ($case in @('original', 'conflicting', 'invalid', 'declared', 'missing')) {
            [IO.File]::WriteAllText($descriptor, $original + $(if ($case -eq 'declared') { "`nrelease_date=June 1997`n" } else { '' }))
            $evidence = @($dates.Values | Sort-Object { $_.file } | ForEach-Object { [ordered]@{ file = $_.file; modified = $_.modified; kind = $_.kind } })
            foreach ($date in $evidence) {
                if ($case -eq 'conflicting' -and $date.file -eq 'ROG.MSN') { $date.modified = '2017-10-03' }
                if ($case -eq 'invalid') { $date.modified = '1991-01-01' }
            }
            if ($case -eq 'missing') { $evidence = @() }
            # A bundled unrelated asset must never affect the selected mission's year
            $evidence += [ordered]@{ file = 'other.hog'; modified = '2025-01-01'; kind = 'zip_entry_mtime' }
            $request = [ordered]@{
                schema = 'dxx-level-metadata-request-v1'; request_id = [guid]::NewGuid().ToString('N')
                game = 'd1'; source_type = 'mission_files'; source_name = 'Rogue provenance test'
                data_dir = $dataDirs.d1; extra_data_dir = $stage
                mission_name = 'ROG'; mission_filename = 'ROG.MSN'
                hog_paths = @((Join-Path $stage 'ROG.HOG')); normal_level_files = @('level01.rdl'); secret_level_files = @()
                provenance_file_dates = @($evidence)
            }
            $result = Invoke-MetadataWorker -Worker $worker -Request $request -RawOutputPath (Join-Path $root "$case.json") -LogPath (Join-Path $root "$case.log") -TimeoutSeconds 120
            $p = $result.provenance
            foreach ($field in @('author', 'nickname', 'real_name', 'email')) {
                if (@($p.credits | Where-Object field -eq $field).Count -ne 1) { throw "Missing/duplicate credit $field in $case" }
            }
            if (($p.credits | Where-Object field -eq 'author').sources.Count -ne 2) { throw 'Author source attribution lost' }
            if (@($p.file_dates | Where-Object file -eq 'other.hog').Count) { throw 'Unrelated archive polluted dates' }
            switch ($case) {
                original { if ($p.date_estimate.year -ne 1998 -or $p.date_estimate.confidence -ne 'low') { throw 'Rogue vintage estimate incorrect' } }
                conflicting { if ($null -ne $p.date_estimate.year -or $p.date_estimate.basis -ne 'conflicting_dates') { throw 'Conflicting dates averaged' } }
                invalid { if ($null -ne $p.date_estimate.year -or @($p.file_dates | Where-Object excluded -eq 'invalid_or_implausible_year').Count -ne 2) { throw 'Implausible dates accepted' } }
                declared { if ($p.date_estimate.year -ne 1997 -or $p.date_estimate.basis -ne 'declared_release_or_completion') { throw 'Release declaration lost' } }
                missing { if ($null -ne $p.date_estimate.year -or $p.file_dates.Count) { throw 'Staging date or prior request leaked' } }
            }
            Write-Host "PASS provenance $case"
        }
    } finally { Stop-MetadataWorkerProcess -Worker $worker }

    # Chronolos has both an embedded descriptor and an externally revised descriptor
    $archive = Get-Item (Join-Path $repoRoot 'game_data/mission_files/chron10b.zip')
    $dateJson = & $java -cp $cliLibraries com.dxxredux.metadata.cli.MainKt --archive-dates $archive.FullName
    if ($LASTEXITCODE -ne 0) { throw 'Shared archive date reader failed' }
    $dates = $dateJson | ConvertFrom-Json -AsHashtable
    $stage = Join-Path $root 'chronolos'
    [IO.Compression.ZipFile]::ExtractToDirectory($archive.FullName, $stage, $true)
    $worker = New-MetadataWorker -Executable (Join-RegressionPath $repoRoot buildd2 main (Get-RegressionHostExecutableNames -BaseName 'dxx-redux-d2-metadata-worker')[0])
    try {
        $request = [ordered]@{
            schema = 'dxx-level-metadata-request-v1'; request_id = [guid]::NewGuid().ToString('N')
            game = 'd2'; source_type = 'mission_files'; source_name = 'Chronolos provenance test'
            data_dir = $dataDirs.d2; extra_data_dir = $stage
            mission_name = 'chron10b'; mission_filename = 'chron10b.mn2'
            hog_paths = @((Join-Path $stage 'chron10b.hog')); normal_level_files = @('dropship.rl2'); secret_level_files = @()
            provenance_file_dates = @($dates.Values)
        }
        $result = Invoke-MetadataWorker -Worker $worker -Request $request -RawOutputPath (Join-Path $root 'shadowed.json') -LogPath (Join-Path $root 'shadowed.log') -TimeoutSeconds 120
        $p = $result.provenance
        if ($null -ne $p.date_estimate.year -or ($p.date_estimate.file_year_range -join ',') -ne '1999,2016') { throw 'Shadowed external descriptor date lost' }
        if (@($p.credits | Where-Object { $_.field -eq 'author' -and $_.value -eq 'david ricci' }).Count -ne 1) { throw 'Shadowed author credit lost' }
        Write-Host 'PASS provenance shadowed descriptor (D2)'
    } finally { Stop-MetadataWorkerProcess -Worker $worker }
} finally {
    if ($fixtureLock) { $fixtureLock.Dispose() }
    Remove-Item -LiteralPath $root -Recurse -Force
}
