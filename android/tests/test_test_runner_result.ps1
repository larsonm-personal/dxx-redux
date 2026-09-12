#!/usr/bin/env pwsh

$ErrorActionPreference = 'Stop'
$scriptDir = Split-Path $PSScriptRoot -Parent
$helpersDir = Join-Path $scriptDir 'helpers'
. "$helpersDir/test_process_output.ps1"
. "$helpersDir/test_suite_progress.ps1"
. "$helpersDir/test_host_platform.ps1"

# Load the real runner functions without starting the full suite
foreach ($source in @(
        @{ Path = "$scriptDir/run_all_tests.ps1"; Names = @('Invoke-SingleTest', 'ConvertTo-ArgumentText', 'Get-PassingResultNotes', 'Add-ReportSidecarLog', 'Test-SingleEmulatorFailureNeedsRecovery') },
        @{ Path = "$helpersDir/test_helpers.ps1"; Names = @('Get-TestStatusFromExitCode') }
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
Write-Host 'Runner result regression passed'
