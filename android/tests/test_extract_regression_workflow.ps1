#!/usr/bin/env pwsh

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$tempRoot = Join-Path $repoRoot 'android\temp\test_extract_regression_workflow'
$helperPath = Join-Path $PSScriptRoot 'extract_regression_spec_helpers.ps1'
$validatorPath = Join-Path $PSScriptRoot 'validate_extract_regression_specs.ps1'
$extractPath = Join-Path $PSScriptRoot 'test_extract.ps1'
$automationTemplatePath = Join-Path $repoRoot 'android\game_scripts\test_extract_regression_template.jsonc'
$allExtractsPath = Join-Path $PSScriptRoot 'test_all_extracts.ps1'
$runAllPath = Join-Path $repoRoot 'android\run_all_tests.ps1'
. $helperPath
. (Join-Path $PSScriptRoot 'extract_regression_recovery.ps1')

if (Test-Path -LiteralPath $tempRoot) {
    Remove-Item -LiteralPath $tempRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $tempRoot | Out-Null

try {
    $extractSource = [System.IO.File]::ReadAllText($extractPath)
    $automationTemplateSource = [System.IO.File]::ReadAllText($automationTemplatePath)
    $allExtractsSource = [System.IO.File]::ReadAllText($allExtractsPath)
    $runAllSource = [System.IO.File]::ReadAllText($runAllPath)
    if ($extractSource -notmatch "(?s)function Start-ExtractSetupActivity.*?'am', 'start', '-W', '-S'.*?'pidof'.*?Wait-SetupReady" -or
        $extractSource -notmatch "(?s)Launching game from set.*?am', 'force-stop'.*?Start-ExtractSetupActivity -Context 'pre-game automation handoff'.*?Invoke-GameAutomationScript") {
        throw 'Extraction launch no longer enforces a clean SetupActivity process boundary'
    }
    if ($extractSource -notmatch "(?s)function Send-SetupCdImport.*?'am', 'broadcast', '--async'.*?'import_cd'" -or
        $extractSource -notmatch "(?s)function Send-SetupIsoImport.*?'am', 'broadcast', '--async'.*?'import_iso'") {
        throw 'Long-running direct imports no longer return host broadcast delivery promptly'
    }
    if ($extractSource -notmatch "(?s)Staging CD source files.*?Start-ExtractSetupActivity -Context 'direct CD import handoff'.*?Send-SetupCdImport" -or
        $extractSource -notmatch "(?s)Staging ISO source file.*?Start-ExtractSetupActivity -Context 'direct ISO import handoff'.*?Send-SetupIsoImport") {
        throw 'Direct imports no longer refresh the dynamic setup-command receiver after source staging'
    }
    if ($extractSource -notmatch '(?s)function Get-ExtractAutomationScriptText.*?"command": "write_music_prefs".*?"source": "midi".*?"action": "enter_game"') {
        throw 'Extraction launch automation no longer isolates itself from external CD-audio preferences'
    }
    if ($extractSource -notmatch '(?s)function Get-ExtractAutomationScriptText.*?\$whenMatch = \[regex\]::Match.*?Groups\[1\]\.Value -cne \$Game.*?\$line -replace.*?"when"') {
        throw 'Extraction launch automation no longer resolves game-specific template steps'
    }
    if ($automationTemplateSource -notmatch '(?s)"text": "Multiplayer".*?"key": "esc".*?"text": "New game"') {
        throw 'Extraction launch automation no longer normalizes the multiplayer submenu before New Game'
    }
    if ($extractSource -notmatch '(?s)\$missionSelectionText = if \(\$MissionSelectionRequired\).*?else \{ '''' \}' -or
        $automationTemplateSource -notmatch '"action": "select_mission", "text": "MISSION_NAME"') {
        throw 'Optional extraction mission selection no longer chooses the current mission or skips an absent picker'
    }
    if ($automationTemplateSource -notmatch '"action": "select", "text": "1", "when": "d1"') {
        throw 'D1 extraction automation no longer accepts the visible starting-level value through the menu-aware selector'
    }
    if ($allExtractsSource -notmatch '(?s)\$exitCode -ne 98.*?\$attempt -gt 1.*?Confirm-EmulatorHealthWithAdbRecovery.*?Invoke-LauncherStartupRecovery.*?Ensure-LauncherTestDeviceReady') {
        throw 'Extraction suite no longer recovers ADB/device infrastructure before its one complete-spec retry'
    }
    if ($extractSource -notmatch '(?m)^# TEST-SUPPORT: owner=test_all_extracts\s*$' -or
        $runAllSource -notmatch 'Get-PowerShellTestSupportOwner') {
        throw 'Single-spec extraction is no longer cataloged as support for the sampled extraction suite'
    }
    if ($extractSource -notmatch 'Get-JsonStringArray \$spec ''mission_files''' -or
        $extractSource -match '\$spec\.mission_files\s*\|\s*Where-Object') {
        throw 'Optional mission_files are no longer normalized before push planning'
    }
    if ($extractSource -notmatch 'Get-ExtractRegressionLogicalSetFiles' -or
        $extractSource -notmatch 'Get-ExtractRegressionMissingExpectedFiles') {
        throw 'Extraction verification no longer includes managed content virtual paths'
    }
    if ($extractSource -notmatch '(?s)trap \{.*?Test-ExtractRegressionAdbTransportFailure.*?exit 98.*?Unexpected extraction test runner error.*?exit 99' -or
        $allExtractsSource -notmatch '\$exitCode -in @\(98, 99\)') {
        throw 'Unexpected runner errors no longer fail fast without being mistaken for emulator failures'
    }
    if ($extractSource -notmatch '(?s)function Adb \{.*?throw "ADB timeout.*?Test-ExtractRegressionAdbTransportFailure -Reason \$stderr.*?function Adb-RunAs \{.*?throw "ADB timeout.*?Test-ExtractRegressionAdbTransportFailure -Reason \$stderr') {
        throw 'Direct ADB helpers no longer promote transport timeouts and failures to retryable infrastructure errors'
    }
    foreach ($transportFailure in @(
            'ADB timeout (30s): shell get-state',
            'ADB failed (shell get-state): * daemon still not running',
            'ADB failed (shell get-state): adb.exe: cannot connect to daemon at tcp:5037',
            'App-private staging failed after copy error: ADB staging failed: ADB failed (shell chmod): * daemon still not running',
            'ADB failed (shell get-state): error: device offline',
            'ADB failed (shell get-state): error: protocol fault (could not read status)'
        )) {
        if (-not (Test-ExtractRegressionAdbTransportFailure -Reason $transportFailure)) {
            throw "Recoverable ADB transport failure was not classified: $transportFailure"
        }
    }
    foreach ($semanticFailure in @(
            'Expected file descent.hog was missing',
            'Automation reported FAIL at launch_mission',
            'run-as: package not debuggable',
            'ADB failed (shell run-as): permission denied',
            'HTTP connection closed by peer'
        )) {
        if (Test-ExtractRegressionAdbTransportFailure -Reason $semanticFailure) {
            throw "Semantic extraction failure was misclassified as ADB transport recovery: $semanticFailure"
        }
    }
    if ($extractSource -notmatch '(?s)function Invoke-GameAutomationScript.*?Ensure-AppPrivateFile.*?gameAutomationInfrastructureFailure.*?Exit-Test 98.*?adb_staging_failed') {
        throw 'Automation staging failures are no longer verified and classified as retryable infrastructure failures'
    }
    if ($extractSource -notmatch '(?s)function Invoke-GameAutomationScript.*?\$runId = \[Guid\]::NewGuid.*?''--es'', ''run_id'', \$runId.*?\$resultRunId -cne \$runId' -or
        $extractSource -notmatch "'shared_prefs/automation_state.xml', 'shared_prefs/automation_state.xml.bak'") {
        throw 'Extraction automation no longer isolates results and persisted state by run ID'
    }
    if ($extractSource -notmatch '(?s)Send-SetupCommand ''clear_audio_sources''.*?Send-SetupCommand ''switch_set'' -Name \$TEST_SET.*?# Clean filesDir root') {
        throw 'Extraction sanitization no longer reasserts the regression set after asynchronous cleanup commands'
    }

    $stablePath = Join-Path $tempRoot 'stable.jsonc'
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
        (Read-JsoncFile $stablePath).total_extracted -ne 2) {
        throw 'Semantic spec change was not written with the new generated time'
    }

    if (-not (Test-ExtractRegressionInfrastructureFailure 'setup_timeout') -or
        -not (Test-ExtractRegressionInfrastructureFailure 'emulator_offline') -or
        -not (Test-ExtractRegressionInfrastructureFailure 'adb_staging_failed') -or
        (Test-ExtractRegressionInfrastructureFailure 'files_missing')) {
        throw 'Infrastructure failure classification is incorrect'
    }

    $gogSpecWithoutMissionFiles = [PSCustomObject]@{
        source_type = 'gog'
        expected_files = @('descent.hog', 'descent.pig')
    }
    if (@(Get-JsonStringArray $gogSpecWithoutMissionFiles 'mission_files').Count -ne 0 -or
        (@(Get-JsonStringArray $gogSpecWithoutMissionFiles 'expected_files') -join ',') -ne
        'descent.hog,descent.pig') {
        throw 'Optional JSON string arrays are not normalized safely for GOG specs'
    }

    $managedState = [PSCustomObject]@{
        set_files_recursive = @(
            'descent2.hog',
            '.content/entries/internal/payload/missions/d2x.hog',
            '.content_projection/d2/missions/d2x.hog'
        )
        content_entries = @(
            [PSCustomObject]@{ files = @('missions/d2x.hog', 'missions/d2x.mn2') }
        )
    }
    $logicalFiles = @(Get-ExtractRegressionLogicalSetFiles $managedState)
    if (($logicalFiles -join ',') -ne 'descent2.hog,missions/d2x.hog,missions/d2x.mn2') {
        throw "Managed content paths were not normalized: $($logicalFiles -join ',')"
    }
    $managedMissing = @(Get-ExtractRegressionMissingExpectedFiles `
        @('descent2.hog', 'd2x.hog', 'missions/d2x.mn2') $logicalFiles)
    if ($managedMissing.Count -ne 0) {
        throw "Managed mission files were reported missing: $($managedMissing -join ',')"
    }
    $negativeMissing = @(Get-ExtractRegressionMissingExpectedFiles @('missing.mn2') $logicalFiles)
    if (($negativeMissing -join ',') -ne 'missing.mn2') {
        throw 'Missing managed mission file was not reported'
    }

    $evidencePath = Join-Path $tempRoot 'evidence.jsonc'
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
    $preserved = (Read-JsoncFile $evidencePath).last_test_result
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
        (Read-JsoncFile $evidencePath).last_test_result.status -ne 'fail') {
        throw 'Equal-strength full evidence did not replace the prior result'
    }
    if ((Get-RegressionSpecHeader $evidencePath).Generated -ne '2000-01-01 00:00:00') {
        throw 'Test-result update unexpectedly changed the spec generation timestamp'
    }
    if (-not (Set-RegressionSpecLastTestResult $evidencePath $fileOnlyPass)) {
        throw 'Successful file-only evidence did not clear a stale full failure'
    }
    $recovered = (Read-JsoncFile $evidencePath).last_test_result
    if ($recovered.status -ne 'pass' -or $recovered.test_mode -ne 'file_only') {
        throw 'Successful file-only recovery result was not saved'
    }
    if ((Get-RegressionSpecHeader $evidencePath).Generated -ne '2000-01-01 00:00:00') {
        throw 'Recovery evidence unexpectedly changed the spec generation timestamp'
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
    Write-CanonicalRegressionSpec -path (Join-Path $emptyDisc 'extract_regression.jsonc') `
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
