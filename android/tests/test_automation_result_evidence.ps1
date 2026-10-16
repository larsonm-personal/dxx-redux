#!/usr/bin/env pwsh
$ErrorActionPreference = 'Stop'
$helperPath = Join-Path $PSScriptRoot '../helpers/test_helpers.ps1'
$helperSource = Get-Content -LiteralPath $helperPath -Raw
# Load the real predicate without initializing an Android SDK or device
$parseErrors = $null
$ast = [Management.Automation.Language.Parser]::ParseFile($helperPath, [ref]$null, [ref]$parseErrors)
if ($parseErrors.Count) { throw "Cannot parse automation helpers: $parseErrors" }
$definition = $ast.Find({
        param($node)
        $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Test-AutomationResultComplete'
    }, $true)
if (-not $definition) { throw 'Missing automation completion predicate' }
. ([scriptblock]::Create($definition.Extent.Text))

$completeResult = '{"result":"PASS","steps_completed":3,"total_steps":3}' | ConvertFrom-Json
if (-not (Test-AutomationResultComplete -Result $completeResult)) { throw 'Complete PASS was rejected' }
foreach ($json in @(
        '{"result":"PASS"}',
        '{"result":"PASS","steps_completed":0,"total_steps":0}',
        '{"result":"PASS","steps_completed":1,"total_steps":2}',
        '{"result":"PASS","steps_completed":3,"total_steps":2}',
        '{"result":"PASS","steps_completed":-1,"total_steps":-1}',
        '{"result":"PASS","steps_completed":"2","total_steps":2}',
        '{"result":"PASS","steps_completed":true,"total_steps":1}',
        '{"result":"PASS","steps_completed":1.5,"total_steps":1.5}',
        '{"result":"PASS","steps_completed":null,"total_steps":1}',
        '{"result":"FAIL","steps_completed":2,"total_steps":2}',
        '{"result":"LAUNCHER_CONTINUE","steps_completed":2,"total_steps":3}'
    )) {
    if (Test-AutomationResultComplete -Result ($json | ConvertFrom-Json)) { throw "Invalid terminal PASS accepted: $json" }
}
if ([regex]::Matches($helperSource, 'Test-AutomationResultComplete -Result \$(terminalResult|resultObj)').Count -ne 2) {
    throw 'Both automation PASS acceptance paths must validate completion'
}

Write-Host 'Automation completion evidence controls passed'

# Exercise bounded graphics failure retention with the real helper and an in-memory ADB boundary
foreach ($functionName in @('Adb-Timeout', 'Get-AppProcessId', 'Save-PublicGraphicsLifecycleFailure')) {
    $definition = $ast.Find({ param($node)
            $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $functionName
        }, $true)
    if (-not $definition) { throw "Missing diagnostic function: $functionName" }
    . ([scriptblock]::Create($definition.Extent.Text))
}
function Get-AndroidInfrastructureTimeout { param([int]$DefaultSeconds) return $DefaultSeconds }
function Get-TestPackageAdbArguments { param([string[]]$Arguments) return $Arguments }
function Write-Status { param([string]$Message, [string]$Color) }
$script:ANDROID_ROOT = Split-Path $PSScriptRoot
$script:PACKAGE = 'com.dxxredux.app'
$previousAndroidRoot = $script:ANDROID_ROOT
$previousPackage = $script:PACKAGE
$previousSerial = $env:ANDROID_SERIAL
$diagnosticRoot = Join-Path $previousAndroidRoot ('temp/graphics-failure-snapshot-control-' + [guid]::NewGuid().ToString('N'))
try {
    $script:ANDROID_ROOT = $diagnosticRoot
    $script:PACKAGE = 'com.dxxredux.app'
    $env:ANDROID_SERIAL = 'emulator-5554'
    $script:deviceApiLevels = @{ 'emulator-5554' = 35 }
    Set-StrictMode -Version Latest
    # Execute the real process wrapper against a harmless Python fixture, never ADB
    $python = @('python3', 'python') | ForEach-Object { Get-Command $_ -CommandType Application -ErrorAction SilentlyContinue } | Select-Object -First 1
    if (-not $python) { throw 'Python is required for SDK-free ADB diagnostic controls' }
    New-Item -ItemType Directory -Path $diagnosticRoot -Force | Out-Null
    $fakeAdb = Join-Path $diagnosticRoot 'fake_adb.py'
    [IO.File]::WriteAllText($fakeAdb, @'
import sys, time
mode = sys.argv[1]
if mode == 'timeout':
    time.sleep(10)
if mode == 'offline':
    print('device offline', file=sys.stderr)
    sys.exit(1)
if mode == 'missing':
    sys.exit(1)
print('123')
'@)
    $script:ADB = $python.Source
    # Python accepts -s before a script path; the production wrapper still supplies both arguments
    $env:ANDROID_SERIAL = $fakeAdb
    if ((Adb-Timeout -AdbArgs @('present') -Seconds 3) -cne '123') { throw 'Legacy ADB call changed its output' }
    foreach ($mode in @('present', 'missing', 'offline', 'timeout')) {
        $query = $null
        $actual = Adb-Timeout -AdbArgs @($mode) -Seconds 1 -DiagnosticResult ([ref]$query)
        if ($mode -eq 'present') {
            if ($actual -cne '123' -or $query.exit_code -ne 0) { throw 'Successful PID diagnostic changed' }
        } elseif ($actual) { throw 'Failed PID query produced successful output' }
        if (($mode -eq 'timeout') -ne $query.timed_out -or ($mode -eq 'offline') -ne [bool]$query.stderr) {
            throw 'Actual ADB wrapper conflated missing process, timeout and transport failure'
        }
        if ($mode -in @('missing', 'offline') -and $query.exit_code -ne 1) { throw 'ADB exit status was discarded' }
    }
    Remove-Item -LiteralPath $fakeAdb -Force
    Remove-Item -LiteralPath $diagnosticRoot -Force
    $env:ANDROID_SERIAL = 'emulator-5554'
    $script:diagnosticCalls = @()
    $script:probeMode = 'missing'
    function Adb-Timeout {
        param([string[]]$AdbArgs, [int]$Seconds, [switch]$IncludeStandardError, [ref]$DiagnosticResult)
        $script:diagnosticCalls += ,$AdbArgs
        if ($Seconds -gt 3) { throw 'Unbounded snapshot query' }
        $result = [ordered]@{ arguments = $AdbArgs; exit_code = 0; timed_out = $false; stderr = ''; error = '' }
        if ($AdbArgs -contains 'pidof') {
            if ($script:probeMode -eq 'timeout') { $result.exit_code = $null; $result.timed_out = $true }
            elseif ($script:probeMode -eq 'offline') { $result.exit_code = 1; $result.stderr = 'device offline' }
            else { $result.exit_code = 1 }
            if ($DiagnosticResult) { $DiagnosticResult.Value = $result }
            return $null
        }
        if ($DiagnosticResult) { $DiagnosticResult.Value = $result }
        if ($AdbArgs -contains 'ls') {
            return "../outside`ntombstone_a.native.xcrash`ncrash_error_b.txt`ntombstone_c.native.xcrash"
        }
        if ($AdbArgs -contains 'logcat' -and $AdbArgs -contains 'main') { return ([string][char]0xE9) * 200000 }
        if ($AdbArgs -contains 'files/.native_fatal_error') {
            $result.exit_code = 1; $result.stderr = 'No such file or directory'
            return $null
        }
        return 'bounded diagnostic fixture'
    }
    if ($null -ne (Get-AppProcessId 'com.dxxredux.app:game')) { throw 'Legacy PID call changed its result' }
    foreach ($mode in @('missing', 'timeout', 'offline')) {
        $script:probeMode = $mode
        $query = $null
        $actual = Get-AppProcessId 'com.dxxredux.app:game' -DiagnosticResult ([ref]$query)
        if ($null -ne $actual -or $null -eq $query) { throw 'PID probe lost its query diagnostics' }
        if (($mode -eq 'timeout') -ne $query.timed_out -or ($mode -eq 'offline') -ne [bool]$query.stderr) {
            throw 'PID absence was conflated with timeout or ADB transport failure'
        }
    }
    $script:graphicsLifecycleFailureProcess = @{ expected_pid = 123; observed_pid = $null; query = $query }
    $owner = Join-Path $script:ANDROID_ROOT 'game_scripts/test_graphics_lifecycle.jsonc'
    $before = $script:diagnosticCalls.Count
    Save-PublicGraphicsLifecycleFailure -ScriptPath $owner -DataEdition retail -Game d2 -RunId rejected
    Save-PublicGraphicsLifecycleFailure -ScriptPath (Join-Path $script:ANDROID_ROOT 'other/test_graphics_lifecycle.jsonc') -DataEdition shareware -Game d2 -RunId rejected
    $env:ANDROID_SERIAL = 'phone'
    Save-PublicGraphicsLifecycleFailure -ScriptPath $owner -DataEdition shareware -Game d2 -RunId rejected
    $env:ANDROID_SERIAL = 'emulator-5554'
    $script:PACKAGE = 'com.dxxredux.app.nsdtest'
    Save-PublicGraphicsLifecycleFailure -ScriptPath $owner -DataEdition shareware -Game d2 -RunId rejected
    $script:PACKAGE = 'com.dxxredux.app'
    if ($script:diagnosticCalls.Count -ne $before -or (Test-Path -LiteralPath $diagnosticRoot)) {
        throw 'Unreviewed owner, retail data or physical device triggered public collection'
    }
    Save-PublicGraphicsLifecycleFailure -ScriptPath $owner -DataEdition shareware -Game d2 -RunId current-run
    $output = Join-Path $diagnosticRoot 'temp/public-graphics-lifecycle-d2'
    $files = @(Get-ChildItem -LiteralPath $output -File)
    if ($files.Count -ne 14 -or @($files | Where-Object Length -gt 262144).Count) { throw 'Snapshot exceeded fixed file/byte bounds' }
    $manifest = Get-Content (Join-Path $output 'manifest.json') -Raw | ConvertFrom-Json
    if ($manifest.run_id -cne 'current-run' -or $manifest.process.query.stderr -cne 'device offline') { throw 'Original PID query evidence was replaced' }
    if (@($manifest.queries | Where-Object { $_.file -eq 'logcat-main.txt' -and $_.truncated }).Count -ne 1) { throw 'UTF-8 snapshot truncation was not recorded' }
    if (@($manifest.queries | Where-Object { $_.file -eq 'native-fatal.txt' -and $_.result.exit_code -eq 1 }).Count -ne 1) { throw 'Missing fatal marker was not distinguished from empty content' }
    $commands = $script:diagnosticCalls | ForEach-Object { $_ -join ' ' }
    if (@($commands | Where-Object { $_ -match 'outside|tombstone_c|/data/tombstones|bugreport|\b(root|grant)\b' }).Count) { throw 'Snapshot crossed its read-only app-owned boundary' }
    # Oversized error metadata must still leave a valid bounded manifest
    $script:graphicsLifecycleFailureProcess.query.stderr = 'x' * 300000
    Save-PublicGraphicsLifecycleFailure -ScriptPath $owner -DataEdition shareware -Game d2 -RunId oversized
    $manifestPath = Join-Path $output 'manifest.json'
    $bounded = Get-Content $manifestPath -Raw | ConvertFrom-Json
    if ((Get-Item $manifestPath).Length -gt 262144 -or $bounded.note -notmatch 'byte budget') { throw 'Manifest was dropped or exceeded its bound' }
    Save-PublicGraphicsLifecycleFailure -ScriptPath $owner -DataEdition shareware -Game d2 -Initialize
    if (@(Get-ChildItem -LiteralPath $output -File).Count) { throw 'A later passing owner could publish stale failure evidence' }
    Write-Host 'PASS: exact public owner failure snapshots, original PID query diagnostics and byte/tombstone bounds'
} finally {
    Set-StrictMode -Off
    $script:ANDROID_ROOT = $previousAndroidRoot
    $script:PACKAGE = $previousPackage
    $env:ANDROID_SERIAL = $previousSerial
    Remove-Item -LiteralPath $diagnosticRoot -Recurse -Force -ErrorAction SilentlyContinue
}
