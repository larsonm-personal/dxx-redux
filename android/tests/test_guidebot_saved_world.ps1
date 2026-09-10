#!/usr/bin/env pwsh
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
if (-not $NoBuild) {
    & (Join-Path $repoRoot 'run-windows-build.ps1') -Target d2
    if ($LASTEXITCODE -ne 0) { throw 'Saved-world test build failed' }
}
$output = Join-Path $repoRoot ('android/temp/test_guidebot_saved_world/' + (Get-Date -Format 'yyyyMMdd_HHmmss'))
& (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $output
New-Item -ItemType Directory -Path $output -Force | Out-Null
$fixture = Join-Path $repoRoot 'android/regression_demos/d2_descent2_level9_20260511_193107.dximdemo'
$checkpoint = Get-Content -LiteralPath $fixture -TotalCount 2 | Select-Object -Last 1 | ConvertFrom-Json
$compressed = [IO.MemoryStream]::new([Convert]::FromBase64String($checkpoint.data))
$decompressed = [IO.MemoryStream]::new()
$stream = [IO.Compression.ZLibStream]::new($compressed, [IO.Compression.CompressionMode]::Decompress)
try { $stream.CopyTo($decompressed) } finally { $stream.Dispose(); $compressed.Dispose() }
$save = Join-Path $output 'fixture.sg0'
[IO.File]::WriteAllBytes($save, $decompressed.ToArray())
$decompressed.Dispose()
if ((Get-Item -LiteralPath $save).Length -ne $checkpoint.size -or
    (Get-FileHash -LiteralPath $save).Hash -ine $checkpoint.sha256) { throw 'Checkpoint extraction mismatch' }
$exe = Join-Path $repoRoot 'buildd2/main/dxx-redux-d2-headless-route.exe'
$hog = Join-Path $repoRoot 'game_data/CD images/Descent II (USA) (v1.1)/data_tracks/d2data'
foreach ($run in 1..2) {
    $userDir = Join-Path $output "user$run"
    New-Item -ItemType Directory -Path $userDir | Out-Null
    Copy-Item -LiteralPath $save -Destination $userDir
    $resultPath = Join-Path $output "run$run.json"
    & $exe -hogdir $hog -mission d2 -level 9 -route-confirm-user-dir $userDir `
        -route-confirm-checkpoint fixture.sg0 -route-confirm-timeout-seconds 180 `
        -route-confirm-json-out $resultPath *> (Join-Path $output "run$run.log")
    if ($LASTEXITCODE -notin 0, 2 -or -not (Test-Path -LiteralPath $resultPath)) { throw 'Checkpoint verification infrastructure failed' }
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
& $exe -hogdir $hog -mission d2 -level 10 -route-confirm-user-dir $userDir `
    -route-confirm-checkpoint fixture.sg0 -route-confirm-json-out $mismatchPath *> (Join-Path $output 'mismatch.log')
if ($LASTEXITCODE -ne 1 -or (Test-Path -LiteralPath $mismatchPath)) { throw 'Mismatched checkpoint level was accepted' }
if ((Get-FileHash -LiteralPath (Join-Path $userDir 'fixture.sg0')).Hash -ine $checkpoint.sha256) {
    throw 'Verification modified its input checkpoint'
}
Write-Host "PASS saved-world verification: restored pose, blue key, difficulty and repeat determinism ($($result.status))"
exit 0
