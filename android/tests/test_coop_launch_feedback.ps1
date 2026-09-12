#!/usr/bin/env pwsh

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/../helpers/test_helpers.ps1"

# Preparation can finish before asynchronous introspection samples it. Verify
# that feedback was published before preflight, then require a real game launch.
& (Get-Process -Id $PID).Path -NoProfile -File "$PSScriptRoot/../helpers/run_test.ps1" `
    -ScriptName test_coop_launch_feedback.jsonc -Game d2
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$log = Adb-Timeout -AdbArgs @('logcat', '-d', '-s', 'DXX-RouteMetadata:I', '*:S') -Seconds 10
if (-not $log -or $log -notmatch '(?s)Launcher game launch requested game=d2 kind=multiplayer.*Launcher preflight game=d2.*Launcher game launch finished game=d2 kind=multiplayer reason=activity_started') {
    throw 'Multiplayer launch feedback must precede preflight and remain tracked through activity startup'
}
Write-Host 'PASS: Co-op launch feedback precedes preflight and game startup completes'
