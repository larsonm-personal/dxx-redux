#!/usr/bin/env pwsh

$ErrorActionPreference = 'Stop'
$scriptDir = Split-Path $PSScriptRoot -Parent
$helpersDir = Join-Path $scriptDir 'helpers'
. "$helpersDir/test_process_output.ps1"
. "$helpersDir/test_suite_progress.ps1"
. "$helpersDir/test_host_platform.ps1"
. "$helpersDir/test_execution_evidence.ps1"
. "$helpersDir/test_target.ps1"

# Load the real runner functions without starting the full suite
foreach ($source in @(
        @{ Path = "$scriptDir/run_all_tests.ps1"; Names = @('Invoke-SingleTest', 'Save-SuiteTestEvidence', 'ConvertTo-ArgumentText', 'Get-PassingResultNotes', 'Add-ReportSidecarLog', 'Test-SingleEmulatorFailureNeedsRecovery', 'Recover-SingleEmulatorEnvironment', 'Recover-DualEmulatorEnvironment') },
        @{ Path = "$helpersDir/test_helpers.ps1"; Names = @('Get-TestStatusFromExitCode', 'Install-ApkOnDevice') }
    )) {
    $ast = [Management.Automation.Language.Parser]::ParseFile($source.Path, [ref]$null, [ref]$null)
    foreach ($functionName in $source.Names) {
        $definition = $ast.Find({
                param($node)
                $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $functionName
            }, $true)
        if (-not $definition) { throw "Missing runner function $functionName" }
        . ([scriptblock]::Create($definition.Extent.Text))
    }
}

$ReportDir = Join-Path $scriptDir 'temp/test_runner_result'
New-Item -ItemType Directory -Force -Path $ReportDir | Out-Null
$timestamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$evidenceContext = New-TestExecutionEvidenceContext -RepositoryRoot (Split-Path $scriptDir) -ReportDir $ReportDir -ReportPath (Join-Path $ReportDir "report_$timestamp.md")
$script:evidenceFailed = $false
$TestTimeoutSeconds = 10
$StopOnFail = $false
$reportSidecarLogsByTestName = @{}
$script:results = @()
$script:passCount = 0
$script:failCount = 0
$script:skipCount = 0
$script:timeoutCount = 0
$totalSw = [Diagnostics.Stopwatch]::StartNew()
$childScript = Join-Path $ReportDir 'child.ps1'
[IO.File]::WriteAllText($childScript, @'
param([string]$Outcome)
Write-Output "Child outcome: $Outcome"
if ($Outcome -eq 'TIMEOUT') { Start-Sleep -Seconds 30 }
if ($Outcome -eq 'FAIL') { exit 1 }
exit 0
'@, [Text.UTF8Encoding]::new($false))

$executionTests = @(
    @{ Name = 'runner_timeout'; Outcome = 'TIMEOUT'; TimeoutSeconds = 2 },
    @{ Name = 'runner_failure'; Outcome = 'FAIL'; TimeoutSeconds = 10 },
    @{ Name = 'runner_pass'; Outcome = 'PASS'; TimeoutSeconds = 10 }
)
for ($index = 0; $index -lt $executionTests.Count; $index++) {
    $test = $executionTests[$index]
    $test.Type = 'ps1'
    $test.Path = $childScript
    $test.Arguments = @('-Outcome', $test.Outcome)
    $test.ProgressIndex = $index + 1
    $test.EstimatedRuntime = $test.TimeoutSeconds
}
foreach ($test in $executionTests) {
    $output = @(Invoke-SingleTest -Test $test)
    if ($output.Count -ne 1 -or $output[0] -isnot [hashtable]) {
        throw "Expected one result hashtable for $($test.Outcome), received $($output.Count) objects"
    }
    $result = $output[0]
    if ($result.Status -ne $test.Outcome) {
        throw "Expected $($test.Outcome), received $($result.Status)"
    }
    $needsRecovery = Test-SingleEmulatorFailureNeedsRecovery -Result $result
    if ($needsRecovery -ne ($test.Outcome -eq 'TIMEOUT')) {
        throw "Unexpected recovery classification for $($test.Outcome)"
    }
    if ($test.Outcome -eq 'TIMEOUT' -and
        (Read-SharedProcessOutput -Path $result.LogFile) -notmatch 'TIMEOUT: Test killed after 2s') {
        throw 'Timeout diagnostic missing from result log'
    }
}
if ($script:results.Count -ne 3 -or $script:timeoutCount -ne 1 -or
    $script:failCount -ne 1 -or $script:passCount -ne 1) {
    throw 'Runner did not retain all results and counters after timeout'
}
if ($script:evidenceFailed) { throw 'Runner failed to save execution evidence' }
$evidence = Get-Content -LiteralPath $evidenceContext.Path -Raw | ConvertFrom-Json
foreach ($test in $executionTests) {
    $observations = @($evidence.observations | Where-Object { $_.name -eq $test.Name -and $_.run_id -eq $evidenceContext.RunId })
    if ($observations.Count -ne 1 -or $observations[0].status -ne $test.Outcome) {
        throw "Runner did not finalize execution evidence for $($test.Name)"
    }
}
# Recovery must return one false result when installation fails, not false plus true
function Write-Status { param($Message, $Color) }
function Test-PhysicalTestTarget { return $false }
function Invoke-AutomaticStaleEmulatorCleanup { }
function Reconnect-AdbDevice { }
function Test-DeviceOnline { param($Serial) return $true }
function Start-SecondEmulator { return $true }
function Install-AppAndData { param($Serial) return $Serial -ne $script:failedInstallSerial }
$helpersDir = $ReportDir
[IO.File]::WriteAllText((Join-Path $helpersDir 'emu_health.ps1'), 'exit 0')
$script:PRIMARY_EMULATOR_SERIAL = 'emulator-5554'
$script:SECONDARY_EMULATOR_SERIAL = 'emulator-5556'
$script:PRIMARY_AVD_NAME = 'fixture-primary'
$script:SECONDARY_AVD_NAME = 'fixture-secondary'
$script:autoServerProc = $null
foreach ($failedSerial in @('emulator-5554', 'emulator-5556', '')) {
    $script:failedInstallSerial = $failedSerial
    $primary = 'original-primary'
    $secondary = 'original-secondary'
    $result = @(Recover-DualEmulatorEnvironment -PrimarySerialRef ([ref]$primary) -SecondarySerialRef ([ref]$secondary))
    if ($result.Count -ne 1 -or $result[0] -ne (-not $failedSerial)) { throw 'Dual recovery reported success after failed provisioning' }
    if ($failedSerial -and ($primary -ne 'original-primary' -or $secondary -ne 'original-secondary')) { throw 'Failed recovery published device state' }
    $primary = 'original-primary'
    $result = @(Recover-SingleEmulatorEnvironment -SerialRef ([ref]$primary))
    if ($result.Count -ne 1 -or $result[0] -ne ($failedSerial -ne 'emulator-5554')) { throw 'Single recovery reported success after failed provisioning' }
}

$savedSuiteApk = $env:DXX_TEST_APK
try {
    $env:DXX_TEST_APK = $childScript
    function Adb-Dev-Timeout {
        param($Serial, $AdbArgs, $Seconds, [switch]$IncludeStandardError)
        $script:installArguments = $AdbArgs
        return $script:installResponse
    }
    $script:installResponse = 'Success'
    foreach ($serial in @('emulator-5554', 'physical-fixture')) {
        if (-not (Install-ApkOnDevice -Serial $serial)) { throw 'Saved suite APK installation failed' }
        if (($script:installArguments -contains '-d') -ne ($serial -like 'emulator-*')) { throw 'APK downgrade allowance escaped emulator scope' }
        if ($script:installArguments[-1] -ne $childScript) { throw 'Installer did not use the saved suite APK' }
    }
    $script:installResponse = 'Failure [INSTALL_FAILED_VERSION_DOWNGRADE]'
    if (Install-ApkOnDevice -Serial 'emulator-5554') { throw 'Failed APK installation reported success' }
} finally {
    $env:DXX_TEST_APK = $savedSuiteApk
}
Write-Host 'Runner result and provisioning recovery regressions passed'
