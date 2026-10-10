#!/usr/bin/env pwsh
# Explicit detached metadata capture experiment for an already-running D2 game
param(
    [Parameter(Mandatory)][string]$Serial,
    [string]$Package = 'com.dxxredux.app.nsdtest',
    [ValidateRange(1, 50)][int]$Rounds = 12,
    [ValidateRange(250, 5000)][int]$IntervalMilliseconds = 500,
    [string]$Output = 'temp/metadata-snapshot.json'
)

$ErrorActionPreference = 'Stop'
$env:ANDROID_SERIAL = $Serial
$env:DXX_TEST_PACKAGE = $Package
. (Join-Path $PSScriptRoot 'test_helpers.ps1')
if ((Test-PhysicalTestTarget) -and $Package -ne 'com.dxxredux.app.nsdtest') {
    throw 'Use the isolated diagnostic package on physical devices'
}
$outputPath = [IO.Path]::GetFullPath($Output)
& (Join-Path $PSScriptRoot 'retain-recent-artifacts.ps1') -Artifacts @($outputPath) | Out-Null
New-Item -ItemType Directory -Force (Split-Path $outputPath) | Out-Null
$steps = @(
    @{ _info = @{ games = @('d2'); _standalone = $false; _owner = 'measure_metadata_snapshot' } }
    @{ action = 'wait_for'; field = 'in_game'; value = 'true'; timeout_ms = 30000 }
    @{ action = 'set_debug'; field = 'metadata_snapshot_probe'; value = 'reset' }
)
for ($round = 0; $round -lt $Rounds; $round++) {
    $steps += @{ action = 'set_debug'; field = 'metadata_snapshot_probe'; value = 'capture' }
    $steps += @{ action = 'wait_ms'; ms = $IntervalMilliseconds }
    $steps += @{ action = 'set_debug'; field = 'metadata_snapshot_probe'; value = 'verify' }
    $steps += @{ action = 'wait_ms'; ms = $IntervalMilliseconds }
}
$steps += @{ action = 'set_debug'; field = 'metadata_snapshot_probe'; value = 'finish' }
$steps += @{ action = 'introspect' }
$scriptPath = [IO.Path]::ChangeExtension($outputPath, '.jsonc')
[IO.File]::WriteAllText($scriptPath, (ConvertTo-Json -InputObject $steps -Depth 10) + "`n")
$runId = [guid]::NewGuid().ToString('N')
$deviceScript = "metadata-snapshot-$runId.jsonc"
try {
    Adb -AdbArgs @('push', $scriptPath, "/data/local/tmp/$deviceScript") | Out-Null
    Adb -AdbArgs @('shell', 'run-as', $Package, 'cp', "/data/local/tmp/$deviceScript", "files/$deviceScript") | Out-Null
    Adb -AdbArgs @('shell', 'run-as', $Package, 'rm', '-f', 'files/automation_result.json') | Out-Null
    Adb -AdbArgs @('logcat', '-c') | Out-Null
    Adb -AdbArgs @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.AUTOMATE', '--es', 'script', $deviceScript, '--es', 'run_id', $runId) | Out-Null
    $timer = [Diagnostics.Stopwatch]::StartNew()
    $result = $null
    $timeoutSeconds = 60 + $Rounds * (2 + 2 * $IntervalMilliseconds / 1000.0)
    while ($timer.Elapsed.TotalSeconds -lt $timeoutSeconds) {
        Start-Sleep -Seconds 1
        $raw = Adb -AdbArgs @('shell', 'run-as', $Package, 'cat', 'files/automation_result.json')
        try { $candidate = $raw | ConvertFrom-Json } catch { continue }
        if ($candidate.run_id -eq $runId) { $result = $candidate; break }
    }
    $log = Adb -AdbArgs @('logcat', '-d', '-s', 'DXX-Automate', 'DXX-Redux')
    [IO.File]::WriteAllText([IO.Path]::ChangeExtension($outputPath, '.log'), [string]$log)
    if (-not $result) { throw 'Snapshot benchmark timed out' }
    $result | ConvertTo-Json -Depth 10 | Set-Content ([IO.Path]::ChangeExtension($outputPath, '.result.json'))
    if ($result.result -ne 'PASS') { throw "Snapshot benchmark failed: $($result | ConvertTo-Json -Compress)" }
    $report = Adb -AdbArgs @('shell', 'run-as', $Package, 'cat', "files/d2x-redux/metadata_snapshot_probe.json")
    $samples = @($report | ConvertFrom-Json)
    if ($samples.Count -ne $Rounds -or @($samples | Where-Object { $_.ok -ne 1 }).Count) {
        throw 'Incomplete snapshot benchmark report'
    }
    [IO.File]::WriteAllText($outputPath, [string]$report + "`n")
    Write-Host "Saved $($samples.Count) snapshot comparisons to $outputPath"
} finally {
    Adb -AdbArgs @('shell', 'run-as', $Package, 'rm', '-f', "files/$deviceScript") | Out-Null
    Adb -AdbArgs @('shell', 'rm', '-f', "/data/local/tmp/$deviceScript") | Out-Null
}
