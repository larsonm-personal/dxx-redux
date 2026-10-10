#!/usr/bin/env pwsh
# Explicit benchmark for an already-running game in an isolated debug app
param(
    [Parameter(Mandatory)][string]$Serial,
    [string]$Package = 'com.dxxredux.app.nsdtest',
    [ValidateSet('d1', 'd2')][string]$Game = 'd2',
    [ValidateRange(1, 50)][int]$Rounds = 12,
    [ValidateSet('memory_blank', 'disk_blank', 'memory_thumbnail', 'disk_thumbnail', 'memory_reuse')]
    [string[]]$Modes = @('memory_blank', 'disk_blank', 'memory_thumbnail', 'disk_thumbnail', 'memory_reuse'),
    [ValidateRange(0, 5000)][int]$IntervalMilliseconds = 250,
    [string]$Output = 'temp/save-cost.json'
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
    @{ _info = @{ games = @($Game); _standalone = $false; _owner = 'measure_save_cost' } }
    @{ action = 'wait_for'; field = 'in_game'; value = 'true'; timeout_ms = 30000 }
    @{ action = 'set_debug'; field = 'save_probe'; value = 'reset' }
)
$modes = @($Modes | Select-Object -Unique)
if (-not $modes.Count) { throw 'At least one save mode is required' }
for ($round = 0; $round -lt $Rounds; $round++) {
    for ($index = 0; $index -lt $modes.Count; $index++) {
        $steps += @{ action = 'set_debug'; field = 'save_probe'; value = $modes[($index + $round) % $modes.Count] }
        $steps += @{ action = 'wait_ms'; ms = $IntervalMilliseconds }
    }
}
$steps += @{ action = 'set_debug'; field = 'save_probe'; value = 'finish' }
$steps += @{ action = 'introspect' }
$scriptPath = [IO.Path]::ChangeExtension($outputPath, '.jsonc')
[IO.File]::WriteAllText($scriptPath, (ConvertTo-Json -InputObject $steps -Depth 10) + "`n")
$runId = [guid]::NewGuid().ToString('N')
$deviceScript = "save-cost-$runId.jsonc"
try {
    Adb -AdbArgs @('push', $scriptPath, "/data/local/tmp/$deviceScript") | Out-Null
    Adb -AdbArgs @('shell', 'run-as', $Package, 'cp', "/data/local/tmp/$deviceScript", "files/$deviceScript") | Out-Null
    Adb -AdbArgs @('shell', 'run-as', $Package, 'rm', '-f', 'files/automation_result.json') | Out-Null
    Adb -AdbArgs @('logcat', '-c') | Out-Null
    Adb -AdbArgs @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.AUTOMATE', '--es', 'script', $deviceScript, '--es', 'run_id', $runId) | Out-Null
    $timer = [Diagnostics.Stopwatch]::StartNew()
    $result = $null
    $timeoutSeconds = 60 + $Rounds * $modes.Count * (1 + $IntervalMilliseconds / 1000.0)
    while ($timer.Elapsed.TotalSeconds -lt $timeoutSeconds) {
        Start-Sleep -Seconds 1
        $raw = Adb -AdbArgs @('shell', 'run-as', $Package, 'cat', 'files/automation_result.json')
        try { $candidate = $raw | ConvertFrom-Json } catch { continue }
        if ($candidate.run_id -eq $runId) { $result = $candidate; break }
    }
    $log = Adb -AdbArgs @('logcat', '-d', '-s', 'DXX-Automate', 'DXX-Redux')
    [IO.File]::WriteAllText([IO.Path]::ChangeExtension($outputPath, '.log'), [string]$log)
    if (-not $result) { throw 'Save benchmark timed out' }
    $result | ConvertTo-Json -Depth 10 | Set-Content ([IO.Path]::ChangeExtension($outputPath, '.result.json'))
    if ($result.result -ne 'PASS') { throw "Save benchmark failed: $($result | ConvertTo-Json -Compress)" }
    $report = Adb -AdbArgs @('shell', 'run-as', $Package, 'cat', "files/${Game}x-redux/save_probe.json")
    $samples = @($report | ConvertFrom-Json)
    if ($samples.Count -ne $Rounds * $modes.Count -or @($samples | Where-Object { $_.ok -ne 1 }).Count) {
        throw 'Incomplete save benchmark report'
    }
    [IO.File]::WriteAllText($outputPath, [string]$report + "`n")
    Write-Host "Saved $($samples.Count) measured saves to $outputPath"
} finally {
    Adb -AdbArgs @('shell', 'run-as', $Package, 'rm', '-f', "files/$deviceScript") | Out-Null
    Adb -AdbArgs @('shell', 'rm', '-f', "/data/local/tmp/$deviceScript") | Out-Null
}
