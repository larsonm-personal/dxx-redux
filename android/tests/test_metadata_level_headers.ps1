#!/usr/bin/env pwsh
param(
    [switch]$NoBuild,
    [ValidateSet('both', 'd1', 'd2')][string]$Game = 'both',
    [ValidateSet('shareware', 'retail')][string]$DataEdition = 'shareware',
    [string]$D1DataDir,
    [string]$D2DataDir,
    [string]$D1Exe,
    [string]$D2Exe,
    [string]$OutputDirectory,
    [switch]$KeepArtifacts
)
Set-StrictMode -Version 3.0
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $repoRoot 'android/helpers/host_metadata_worker.ps1')
. (Join-Path $repoRoot 'android/helpers/test_host_platform.ps1')
. (Join-Path $repoRoot 'android/helpers/standard_game_data.ps1')
. (Join-Path $repoRoot 'android/helpers/shareware_test_data.ps1')
$root = if ($OutputDirectory) { [IO.Path]::GetFullPath($OutputDirectory) } else { Join-Path $repoRoot ('android/temp/metadata-level-header-test/run_' + [guid]::NewGuid().ToString('N')) }
if ($OutputDirectory) {
    if (Test-Path -LiteralPath $root) { throw "Use a new output directory: $root" }
    & (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $root
} else {
    & (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $root -DirectoryPrefix run_
}
New-Item -ItemType Directory -Path $root -Force | Out-Null
$fixtureLock = $null
$passed = $false
try {
    $fixtureLock = [IO.File]::Open((Join-Path $root 'producer.lock'), [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    $engines = if ($Game -eq 'both') { @('d1', 'd2') } else { @($Game) }
    foreach ($engine in $engines) {
        if (-not $NoBuild) { Invoke-RegressionHostBuild -RepoRoot $repoRoot -Target $engine }
        $names = if ($engine -eq 'd1') { @('descent.hog', 'descent.pig') } else { @('descent2.hog', 'descent2.ham', 'groupa.pig') }
        $requestedData = if ($engine -eq 'd1') { $D1DataDir } else { $D2DataDir }
        if ($DataEdition -eq 'shareware') {
            $dataPath = Resolve-SharewareTestData -RepoRoot $repoRoot -Game $engine -DataDir $requestedData
            $validLevel = (Get-SharewareTestGameConfig -RepoRoot $repoRoot -Game $engine).FirstLevel
        } else {
            $candidates = if ($requestedData) { @($requestedData) } else { @(Get-StandardGameDataCandidates -RepoRoot $repoRoot -Game $engine) }
            $data = Resolve-StandardGameDataDirectory -Candidates $candidates -Dependencies @(Get-StandardGameDataDeps | Where-Object file -in $names) -Label $engine
            $dataPath = $data.Path
            $validLevel = if ($engine -eq 'd1') { 'level01.rdl' } else { 'd2leva-1.rl2' }
        }
        $extension = if ($engine -eq 'd1') { 'rdl' } else { 'rl2' }
        $executable = if ($engine -eq 'd1') { $D1Exe } else { $D2Exe }
        if (-not $executable) {
            $executable = Join-RegressionPath $repoRoot "build$engine" main (Get-RegressionHostExecutableNames -BaseName "dxx-redux-$engine-metadata-worker")[0]
        }
        $worker = New-MetadataWorker -Executable $executable
        try {
            foreach ($case in @('future', 'truncated', 'signature', 'zero')) {
                $file = "$case.$extension"
                $bytes = [Text.Encoding]::ASCII.GetBytes('LVLP') + [BitConverter]::GetBytes([int]23)
                if ($case -eq 'truncated') { $bytes = [Text.Encoding]::ASCII.GetBytes('LVLP') }
                if ($case -eq 'signature') { $bytes[0] = 0 }
                if ($case -eq 'zero') { $bytes = [Text.Encoding]::ASCII.GetBytes('LVLP') + [BitConverter]::GetBytes([int]0) }
                [IO.File]::WriteAllBytes((Join-Path $root $file), $bytes)
                $request = @{
                    schema = 'dxx-level-metadata-request-v1'; request_id = [guid]::NewGuid().ToString('N')
                    game = $engine; source_type = 'level'; source_name = $case
                    data_dir = $dataPath
                    extra_data_dir = $root; level_file = $file; level_num = 1
                }
                $result = Invoke-MetadataWorker -Worker $worker -Request $request `
                    -RawOutputPath (Join-Path $root "$engine-$case.json") -LogPath (Join-Path $root "$engine-$case.log") -TimeoutSeconds 10
                $healthyPid = $worker.Process.Id
                $expected = if ($case -eq 'future') { 'unsupported_format' } else { 'invalid_input' }
                if ($result.status -ne 'failed' -or @($result.levels).Count -ne 1 -or $result.levels[0].failure_kind -ne $expected) {
                    throw "Incorrect $engine result for ${case}: $($result | ConvertTo-Json -Depth 6 -Compress)"
                }
                if ($case -eq 'future' -and ($result.levels[0].problems -join ' ') -notmatch 'unsupported level version 23') {
                    throw 'Unsupported format must identify the rejected version'
                }
                # Invalid level data must not poison or retire the initialized worker
                $request.request_id = [guid]::NewGuid().ToString('N')
                $request.source_name = "valid-after-$case"
                $request.level_file = $validLevel
                $request.extra_data_dir = ''
                $recovered = Invoke-MetadataWorker -Worker $worker -Request $request `
                    -RawOutputPath (Join-Path $root "$engine-valid-after-$case.json") -LogPath (Join-Path $root "$engine-valid-after-$case.log") -TimeoutSeconds 120
                if ($recovered.status -ne 'ok' -or @($recovered.levels).Count -ne 1 -or $worker.Process.Id -ne $healthyPid) {
                    throw "$engine worker did not remain healthy after $case"
                }
                Write-Host "PASS $DataEdition $engine $case header rejected; valid level recovered in same worker"
            }
        } finally { Stop-MetadataWorkerProcess -Worker $worker }
    }
    $passed = $true
    Write-Host "PASS native metadata $DataEdition $($engines -join '/') rejects malformed headers and preserves worker state"
} finally {
    if ($fixtureLock) { $fixtureLock.Dispose() }
    if ($passed -and -not $KeepArtifacts) { Remove-Item -LiteralPath $root -Recurse -Force }
    else { Write-Host "Metadata header diagnostics: $root" }
}
