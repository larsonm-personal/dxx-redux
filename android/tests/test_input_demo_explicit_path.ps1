#!/usr/bin/env pwsh
# An explicit recording must not parse unrelated recordings during discovery
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$output = Join-Path $repoRoot "android/temp/test_input_demo_explicit_path/run_$(Get-Date -Format 'yyyyMMdd_HHmmss_fff')"
& (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $output
New-Item -ItemType Directory -Path $output -Force | Out-Null
[IO.File]::WriteAllText((Join-Path $output 'unfinished.dximdemo'), 'incomplete unrelated recording')
$demo = Join-Path $repoRoot 'android/regression_demos/d2_descent2_level9_20260511_215654.dximdemo'
$result = & (Get-Process -Id $PID).Path -NoProfile -File (Join-Path $PSScriptRoot 'run_input_demo_replay.ps1') `
    -DemoPath $demo -SearchRoot $output -Game d2 -Mode accelerated -ResolveDataDirOnly
if ($LASTEXITCODE -ne 0 -or ($result -join "`n") -notmatch 'Resolved d2 data dir:') {
    throw 'An unrelated malformed recording prevented explicit demo selection'
}
Write-Host 'PASS: explicit demo selection ignores unrelated malformed recordings'
