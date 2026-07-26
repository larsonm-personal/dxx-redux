#!/usr/bin/env pwsh

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$tempRoot = Join-Path $repoRoot 'android\temp\test_extract_regression_workflow'
$helperPath = Join-Path $PSScriptRoot 'extract_regression_spec_helpers.ps1'
$validatorPath = Join-Path $PSScriptRoot 'validate_extract_regression_specs.ps1'
$extractPath = Join-Path $PSScriptRoot 'test_extract.ps1'
$allExtractsPath = Join-Path $PSScriptRoot 'test_all_extracts.ps1'
. $helperPath

if (Test-Path -LiteralPath $tempRoot) {
    Remove-Item -LiteralPath $tempRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $tempRoot | Out-Null

try {
    $extractSource = [System.IO.File]::ReadAllText($extractPath)
    $allExtractsSource = [System.IO.File]::ReadAllText($allExtractsPath)
    if ($extractSource -notmatch "(?s)Launching game from set.*?am', 'force-stop'.*?Wait-SetupReady.*?Invoke-GameAutomationScript") {
        throw 'Extraction launch no longer enforces a clean SetupActivity process boundary'
    }
    if ($allExtractsSource -notmatch '(?s)\$exitCode -ne 98.*?\$attempt -gt 1.*?Ensure-LauncherTestDeviceReady') {
        throw 'Extraction suite no longer bounds infrastructure recovery to one complete-spec retry'
    }

    $stablePath = Join-Path $tempRoot 'stable.json5'
    $spec = [ordered]@{
        source_type = 'cd'
        expected_files = @('descent.hog')
        total_extracted = 1
    }
    Write-CanonicalRegressionSpec -path $stablePath -spec $spec -sourceName 'stable' -generated '2000-01-01 00:00:00'
    $initial = [System.IO.File]::ReadAllText($stablePath)
    Write-CanonicalRegressionSpec -path $stablePath -spec $spec -sourceName 'stable' -generated '2099-01-01 00:00:00'
    $unchanged = [System.IO.File]::ReadAllText($stablePath)
    if ($unchanged -cne $initial -or $unchanged -notmatch 'Generated: 2000-01-01 00:00:00') {
        throw 'Semantically unchanged spec was rewritten'
    }

    $spec.total_extracted = 2
    Write-CanonicalRegressionSpec -path $stablePath -spec $spec -sourceName 'stable' -generated '2099-01-01 00:00:00'
    $changed = [System.IO.File]::ReadAllText($stablePath)
    if ($changed -ceq $initial -or $changed -notmatch 'Generated: 2099-01-01 00:00:00' -or
        (Read-Json5File $stablePath).total_extracted -ne 2) {
        throw 'Semantic spec change was not written with the new generated time'
    }

    if (-not (Test-ExtractRegressionInfrastructureFailure 'setup_timeout') -or
        -not (Test-ExtractRegressionInfrastructureFailure 'emulator_offline') -or
        -not (Test-ExtractRegressionInfrastructureFailure 'adb_staging_failed') -or
        (Test-ExtractRegressionInfrastructureFailure 'files_missing')) {
        throw 'Infrastructure failure classification is incorrect'
    }

    $evidencePath = Join-Path $tempRoot 'evidence.json5'
    $fullPass = [ordered]@{
        status = 'pass'
        failure_step = ''
        level_reached = 'Lunar Outpost'
        files_verified = 2
        classification_confirmed = $true
        test_mode = 'full'
    }
    $evidenceSpec = [ordered]@{
        source_type = 'cd'
        expected_files = @('descent.hog', 'descent.pig')
        total_extracted = 2
        last_test_result = $fullPass
    }
    Write-CanonicalRegressionSpec -path $evidencePath -spec $evidenceSpec `
        -sourceName 'evidence' -generated '2000-01-01 00:00:00'
    $fileOnlyPass = [ordered]@{
        status = 'pass'
        failure_step = ''
        level_reached = $null
        files_verified = 2
        classification_confirmed = $true
        test_mode = 'file_only'
    }
    if (Set-RegressionSpecLastTestResult $evidencePath $fileOnlyPass) {
        throw 'File-only evidence unexpectedly replaced a full result'
    }
    $preserved = (Read-Json5File $evidencePath).last_test_result
    if ($preserved.test_mode -ne 'full' -or $preserved.level_reached -ne 'Lunar Outpost') {
        throw 'Full result was not preserved'
    }
    $fullFail = [ordered]@{
        status = 'fail'
        failure_step = 'launch_failed'
        level_reached = $null
        files_verified = 2
        classification_confirmed = $true
        test_mode = 'full'
    }
    if (-not (Set-RegressionSpecLastTestResult $evidencePath $fullFail) -or
        (Read-Json5File $evidencePath).last_test_result.status -ne 'fail') {
        throw 'Equal-strength full evidence did not replace the prior result'
    }

    $emptyRoot = Join-Path $tempRoot 'cds'
    $emptyDisc = Join-Path $emptyRoot 'empty-oracle'
    New-Item -ItemType Directory -Path $emptyDisc | Out-Null
    Set-Content -LiteralPath (Join-Path $emptyDisc 'source.iso') -Value 'fixture' -NoNewline
    $emptySpec = [ordered]@{
        source_type = 'cd'
        disc_image_type = 'iso'
        source_files = @(@{ name = 'source.iso'; sha256 = 'fixture' })
        expected_files = @()
        total_extracted = 0
        import_mode = 'setup_iso'
    }
    Write-CanonicalRegressionSpec -path (Join-Path $emptyDisc 'extract_regression.json5') `
        -spec $emptySpec -sourceName 'empty-oracle' -generated '2000-01-01 00:00:00'

    $pwsh = Get-ExtractRegressionPwshPath
    $validationOutput = & $pwsh -NoProfile -NonInteractive -File $validatorPath -CdRoot $emptyRoot *>&1
    if ($LASTEXITCODE -eq 0 -or
        "$validationOutput" -notmatch 'expected_files must contain at least one extraction oracle') {
        throw 'Empty expected_files oracle did not fail validation'
    }

    Write-Host 'Extract regression workflow tests passed'
} finally {
    if (Test-Path -LiteralPath $tempRoot) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force
    }
}

exit 0
