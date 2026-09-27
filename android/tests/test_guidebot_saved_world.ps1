#!/usr/bin/env pwsh
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $repoRoot 'android/helpers/test_host_platform.ps1')
. (Join-Path $repoRoot 'android/helpers/standard_game_data.ps1')
. (Join-Path $repoRoot 'android/helpers/headless_process_pool.ps1')
if (-not $NoBuild) { Invoke-RegressionHostBuild -RepoRoot $repoRoot -Target d2 }
$exe = Join-RegressionPath $repoRoot buildd2 main (Get-RegressionHostExecutableNames -BaseName 'dxx-redux-d2-headless-route')[0]
$deps = @(Get-StandardGameDataDeps | Where-Object file -in @('descent2.hog', 'descent2.ham', 'groupa.pig'))
$data = Resolve-StandardGameDataDirectory -Candidates @(Get-StandardGameDataCandidates -RepoRoot $repoRoot -Game d2) -Dependencies $deps -Label d2
$hog = $data.Path

function Invoke-CheckpointRoute {
    param([string[]]$RouteArguments, [string]$LogPath)
    $routeExecution = @{ ExitCode = -1; TimedOut = $false }
    $task = [pscustomobject]@{ FilePath = $exe; Arguments = $RouteArguments; WorkingDirectory = $repoRoot; TimeoutSeconds = 300 }
    Invoke-HeadlessProcessPool -Tasks @($task) -MaxParallel 1 -OnCompleted {
        param($task, $result)
        [IO.File]::WriteAllText($LogPath, $result.StandardOutput + "`n" + $result.StandardError, [Text.UTF8Encoding]::new($false))
        $routeExecution.ExitCode = $result.ExitCode
        $routeExecution.TimedOut = $result.TimedOut
    }
    if ($routeExecution.TimedOut) { throw "Checkpoint verification timed out; see $LogPath" }
    return $routeExecution.ExitCode
}
$output = Join-Path $repoRoot ('android/temp/test_guidebot_saved_world/run_' + [guid]::NewGuid().ToString('N'))
& (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $output -DirectoryPrefix run_
New-Item -ItemType Directory -Path $output -Force | Out-Null
$fixtureLock = $null
$save = Join-Path $output 'fixture.sg0'
try {
    $fixtureLock = [IO.File]::Open((Join-Path $output 'producer.lock'), [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    $fixture = Join-Path $PSScriptRoot 'fixtures/guidebot_saved_world_checkpoint.json'
    $checkpoint = Get-Content -LiteralPath $fixture -Raw | ConvertFrom-Json
    $compressed = [IO.MemoryStream]::new([Convert]::FromBase64String($checkpoint.data))
    $decompressed = [IO.MemoryStream]::new()
    $stream = [IO.Compression.ZLibStream]::new($compressed, [IO.Compression.CompressionMode]::Decompress)
    try {
        $stream.CopyTo($decompressed)
        [IO.File]::WriteAllBytes($save, $decompressed.ToArray())
    } finally { $stream.Dispose(); $compressed.Dispose(); $decompressed.Dispose() }
    if ((Get-Item -LiteralPath $save).Length -ne $checkpoint.size -or
        (Get-FileHash -LiteralPath $save).Hash -ine $checkpoint.sha256) { throw 'Checkpoint extraction mismatch' }
    foreach ($run in 1..2) {
        $userDir = Join-Path $output "user$run"
        New-Item -ItemType Directory -Path $userDir | Out-Null
        Copy-Item -LiteralPath $save -Destination $userDir
        $resultPath = Join-Path $output "run$run.json"
        $routeExit = Invoke-CheckpointRoute -RouteArguments @('-hogdir', $hog, '-mission', 'd2', '-level', '9', '-route-confirm-user-dir', $userDir, '-route-confirm-checkpoint', 'fixture.sg0', '-route-confirm-timeout-seconds', '180', '-route-confirm-json-out', $resultPath) -LogPath (Join-Path $output "run$run.log")
        if ($routeExit -notin 0, 2 -or -not (Test-Path -LiteralPath $resultPath)) { throw 'Checkpoint verification infrastructure failed' }
        $result = Get-Content -LiteralPath $resultPath -Raw | ConvertFrom-Json
        if ($result.start_state.kind -ne 'saved_world' -or $result.start_state.segment -ne 138 -or
            $result.start_state.key_flags -ne 2 -or $result.difficulty -ne 0) {
            throw 'Checkpoint position, carried blue key or difficulty was reset'
        }
        if ($null -ne $result.route_confirmation) { throw 'Saved world incorrectly issued an authored-start certificate' }
        if ($result.status -ne 'confirmed' -or $result.frames -eq 0 -or $result.radius.player -ne $result.radius.effective) {
            throw 'Saved-world verification did not complete with the full player radius'
        }
    }
    if ((Get-FileHash (Join-Path $output 'run1.json')).Hash -ne (Get-FileHash (Join-Path $output 'run2.json')).Hash) {
        throw 'Saved-world repeats differ'
    }
    $mismatchPath = Join-Path $output 'mismatch.json'
    $routeExit = Invoke-CheckpointRoute -RouteArguments @('-hogdir', $hog, '-mission', 'd2', '-level', '10', '-route-confirm-user-dir', $userDir, '-route-confirm-checkpoint', 'fixture.sg0', '-route-confirm-json-out', $mismatchPath) -LogPath (Join-Path $output 'mismatch.log')
    if ($routeExit -ne 1 -or (Test-Path -LiteralPath $mismatchPath)) { throw 'Mismatched checkpoint level was accepted' }
    if ((Get-FileHash -LiteralPath (Join-Path $userDir 'fixture.sg0')).Hash -ine $checkpoint.sha256) {
        throw 'Verification modified its input checkpoint'
    }
    Write-Host "PASS saved-world verification: restored pose, blue key, difficulty and repeat determinism ($($result.status))"
} finally {
    try {
        Get-ChildItem -LiteralPath $output -Directory | Remove-Item -Recurse -Force
        if (Test-Path -LiteralPath $save) { Remove-Item -LiteralPath $save -Force }
    } finally { if ($fixtureLock) { $fixtureLock.Dispose() } }
}
exit 0
