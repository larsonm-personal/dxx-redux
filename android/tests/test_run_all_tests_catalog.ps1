#!/usr/bin/env pwsh
# Exercise real catalog discovery without provisioning or running the discovered tests
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $repoRoot 'android/helpers/headless_process_pool.ps1')
. (Join-Path $repoRoot 'android/helpers/test_host_platform.ps1')
. (Join-Path $repoRoot 'android/helpers/test_suite_coverage.ps1')
$unusedReportDir = Join-Path $repoRoot ('android/temp/catalog_must_not_exist_' + [guid]::NewGuid().ToString('N'))
$fixtureRoot = Join-Path $repoRoot ('android/temp/test_catalog/run_' + [guid]::NewGuid().ToString('N'))
& (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $fixtureRoot -DirectoryPrefix run_
New-Item -ItemType Directory -Path $fixtureRoot -Force | Out-Null
$producerLock = [IO.File]::Open((Join-Path $fixtureRoot 'producer.lock'), [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
$catalogs = @()
try {
    foreach ($number in 1..6) {
        [IO.File]::WriteAllText((Join-Path $fixtureRoot ('report_2000010{0}_000000.md' -f $number)), 'catalog retention sentinel')
    }
    foreach ($extended in @($false, $true)) {
        $execution = @{ Result = $null }
        $arguments = @('-NoProfile', '-NonInteractive', '-File', (Join-Path $repoRoot 'android/run_all_tests.ps1'), '-ListTests', '-ReportDir', $(if ($extended) { $fixtureRoot } else { $unusedReportDir }), '-HostOnly', '-Filter', 'matches_no_test')
        if ($extended) { $arguments += '-ExtendedGraphics' }
        $task = [pscustomobject]@{ FilePath = Get-RegressionCurrentPwshPath; Arguments = $arguments; WorkingDirectory = [IO.Path]::GetTempPath(); TimeoutSeconds = 45 }
        Invoke-HeadlessProcessPool -Tasks @($task) -MaxParallel 1 -OnCompleted {
            param($task, $result)
            $execution.Result = $result
        }
        if ($execution.Result.TimedOut -or $execution.Result.ExitCode -ne 0) { throw "Catalog invocation failed: $($execution.Result.StandardError)" }
        if (Test-Path -LiteralPath $unusedReportDir) { throw 'Catalog mode created an execution report directory' }
        if (@(Get-ChildItem -LiteralPath $fixtureRoot -Filter 'report_*.md').Count -ne 6) { throw 'Catalog mode pruned prior reports' }
        $catalog = $execution.Result.StandardOutput | ConvertFrom-Json
        if ($catalog.schema -ne 1 -or $catalog.extended_graphics -ne $extended) { throw 'Incorrect catalog schema or variant' }
        if (@($catalog.tests | Group-Object name | Where-Object Count -gt 1).Count) { throw 'Duplicate top-level entries' }
        foreach ($test in $catalog.tests) {
            if ($test.timeout_seconds -le 0 -or $test.requires -notin @('none', 'emulator', 'two_emulators', 'extract', 'server')) { throw "Incomplete scheduling declaration: $($test.name)" }
            if ([IO.Path]::IsPathRooted($test.path) -or -not (Test-Path -LiteralPath (Join-Path $repoRoot $test.path) -PathType Leaf)) { throw "Invalid checkout-relative path: $($test.path)" }
        }
        foreach ($support in $catalog.support) {
            if ($support.owner -notin @($catalog.tests.base_name)) { throw "Support owner absent from catalog: $($support.name)" }
        }
        $fovOwner = @($catalog.tests | Where-Object name -eq 'test_fov_demo_compatibility')
        $fovSupport = @($catalog.support | Where-Object { $_.type -eq 'jsonc' -and $_.name -eq 'test_fov_demo_compatibility' })
        if ($fovOwner.Count -ne 1 -or $fovOwner[0].type -ne 'ps1' -or
            $fovOwner[0].requires -ne 'emulator' -or $fovOwner[0].timeout_seconds -lt 480 -or
            $fovSupport.Count -ne 1 -or $fovSupport[0].owner -ne $fovOwner[0].base_name) {
            throw 'FOV demo support must run through its two-engine integration owner'
        }
        $packageProbe = @($catalog.tests | Where-Object name -eq 'test_combined_package_identity')
        if ($packageProbe.Count -ne 1 -or $packageProbe[0].requires -ne 'none' -or
            'test_combined_package_identity' -notin (Get-TestSuiteCoveragePolicy).explicit) {
            throw 'Combined package inspection must remain an explicit host probe with caller-selected artifacts'
        }
        $recoverySupport = @($catalog.support | Where-Object owner -eq 'test_graphics_recovery')
        if ($recoverySupport.Count -ne 10 -or @($catalog.tests | Where-Object name -like 'test_graphics_*_trigger').Count) {
            throw 'Graphics fault scripts must run through their recovery owner'
        }
        foreach ($kind in @('ps1', 'jsonc')) {
            $directory = if ($kind -eq 'ps1') { $PSScriptRoot } else { Join-Path $repoRoot 'android/game_scripts' }
            foreach ($file in Get-ChildItem -LiteralPath $directory -Filter "test_*.$kind" -File) {
                $owners = @($catalog.tests | Where-Object { $_.type -eq $kind -and [IO.Path]::GetFileName($_.path) -eq $file.Name })
                $support = @($catalog.support | Where-Object { $_.type -eq $kind -and $_.name -eq $file.BaseName })
                if (-not $owners.Count -and -not $support.Count) { throw "Discovered file missing from catalog: $($file.Name)" }
                if ($owners.Count -and $support.Count) { throw "Support also listed standalone: $($file.Name)" }
            }
        }
        foreach ($name in @('test_download_verification', 'test_d2xxl_sound_format', 'test_d2xxl_tga_layout', 'test_d2xxl_tga_pixels', 'test_cd_regression_runner', 'test_extract_all_cds_batch', 'test_extract_all_gog_batch', 'test_generate_regression_specs', 'test_extract_suite_device_preflight', 'test_extract_regression_workflow', 'test_extraction_cache_provenance', 'test_extraction_publication',
                'test_fingerprint_audio_enumeration', 'test_fingerprint_manifest_publication', 'test_fingerprint_mission_zip_budgets', 'test_fingerprint_music_pack_build_guard', 'test_fingerprint_source_identity', 'test_fingerprint_threshold',
                'test_dos_midi_parity', 'test_run_all_tests_catalog',
                'test_input_demo_explicit_path', 'test_input_demo_host_build_guard', 'test_repository_artifact_policy', 'test_mission_zip_batch_publication', 'test_mission_zip_batch_recovery', 'test_regression_process_lifetime', 'test_powershell_51_compatibility')) {
            if (@($catalog.tests | Where-Object { $_.name -eq $name -and $_.requires -eq 'none' }).Count -ne 1) { throw "Host classification missing: $name" }
        }
        if (-not @($catalog.tests | Where-Object manual).Count -or -not @($catalog.tests | Where-Object requires -eq 'two_emulators').Count) { throw 'Execution filters incorrectly narrowed the catalog' }
        $unattended = @($catalog.tests | Where-Object { -not $_.manual } | ForEach-Object {
                [pscustomobject]@{ Name = $_.name; BaseName = $_.base_name }
            })
        $fullCoverage = @(Select-TestSuiteCoverage -Tests $unattended -Seed 0 -AllScenarios)
        $explicitNames = (Get-TestSuiteCoveragePolicy).explicit
        if (@($fullCoverage | Where-Object BaseName -in $explicitNames).Count) { throw 'Full unattended coverage included an explicit investigation' }
        $expectedNames = @($unattended | Where-Object BaseName -notin $explicitNames | ForEach-Object name | Sort-Object)
        $selectedNames = @($fullCoverage | ForEach-Object name | Sort-Object)
        if (Compare-Object $expectedNames $selectedNames) { throw 'Full unattended coverage dropped a supported scenario or replay variant' }
        if ('test_graphics_recovery' -notin $selectedNames) { throw 'Full coverage omitted the provisioned graphics recovery owner' }
        if ('test_emulator_recovery' -notin $selectedNames) { throw 'Full coverage omitted the provisioned two-emulator recovery test' }
        if ('test_distribution_build_info' -notin $selectedNames) { throw 'Full coverage omitted installed APK build information' }
        $catalogs += $catalog
    }
    if (-not @($catalogs[0].tests | Where-Object name -like '*graphics_canary').Count -or @($catalogs[1].tests | Where-Object name -like '*graphics_canary').Count) { throw 'Extended replay matrix did not replace canaries' }

    # Execute the real replay wrapper chain with only the native build and replay mocked
    $batchRoot = Join-Path $fixtureRoot 'replay_batch_repository'
    $batchTests = Join-Path $batchRoot 'android/tests'
    $batchHelpers = Join-Path $batchRoot 'android/helpers'
    $batchDemos = Join-Path $batchRoot 'android/regression_demos'
    New-Item -ItemType Directory -Path $batchTests, $batchHelpers, $batchDemos -Force | Out-Null
    foreach ($file in @('test_input_demo_regressions.ps1', 'test_input_demo_regressions_graphics.ps1', 'run_input_demo_regressions.ps1', 'input_demo_host_build_guard.ps1')) {
        Copy-Item -LiteralPath (Join-Path $PSScriptRoot $file) -Destination $batchTests
    }
    foreach ($file in @('test_host_platform.ps1', 'powershell_compat.ps1')) {
        Copy-Item -LiteralPath (Join-Path $repoRoot "android/helpers/$file") -Destination $batchHelpers
    }
    Add-Content -LiteralPath (Join-Path $batchTests 'input_demo_host_build_guard.ps1') -Encoding utf8NoBOM -Value @'
function Ensure-InputDemoGameBuild {
    param([string]$RepoRoot, [string]$GameName, [switch]$PreferHeadlessConsole)
}
'@
    [IO.File]::WriteAllText((Join-Path $batchTests 'run_input_demo_replay.ps1'), @'
param(
    [string]$DemoPath, [string]$Game, [string]$Mode, [int]$TimeoutSeconds,
    [switch]$D1InD2, [switch]$PreferHeadlessConsole, [string]$Runner = 'auto',
    [string]$Sanitizer
)
$PSBoundParameters | ConvertTo-Json | Set-Content -LiteralPath ($DemoPath + '.invocation.json') -Encoding utf8NoBOM
exit 0
'@)
    foreach ($recordedGame in @('d1', 'd2')) {
        [IO.File]::WriteAllText((Join-Path $batchDemos "$recordedGame.dximdemo"),
            (@{ type = 'header'; game = $recordedGame } | ConvertTo-Json -Compress) + "`n")
    }
    foreach ($batchCase in @(
            @{ Recorded = 'd1'; Game = 'd1'; Imported = $false; Graphics = $true },
            @{ Recorded = 'd2'; Game = 'd2'; Imported = $false; Graphics = $true },
            @{ Recorded = 'd1'; Game = 'd2'; Imported = $true; Graphics = $true },
            @{ Recorded = 'd1'; Game = 'd2'; Imported = $true; Graphics = $false }
        )) {
        $wrapperName = if ($batchCase.Graphics) { 'test_input_demo_regressions_graphics.ps1' } else { 'test_input_demo_regressions.ps1' }
        $batchArguments = @('-NoProfile', '-NonInteractive', '-File', (Join-Path $batchTests $wrapperName), '-Game', $batchCase.Game)
        if ($batchCase.Imported) { $batchArguments += '-D1InD2' }
        & (Get-RegressionCurrentPwshPath) @batchArguments | Out-Null
        if ($LASTEXITCODE -ne 0) { throw 'Replay batch forwarding fixture failed' }
        $invocationPath = Join-Path $batchDemos ($batchCase.Recorded + '.dximdemo.invocation.json')
        $invocation = Get-Content -LiteralPath $invocationPath -Raw | ConvertFrom-Json
        $expectedRunner = if ($batchCase.Graphics) { 'visual' } else { 'fast' }
        if ($invocation.Runner -ne $expectedRunner -or $invocation.Game -ne $batchCase.Game -or
            [bool]$invocation.D1InD2 -ne $batchCase.Imported) {
            throw "Replay batch selected the wrong backend for $($batchCase.Recorded) under $($batchCase.Game), graphics=$($batchCase.Graphics)"
        }
        Remove-Item -LiteralPath $invocationPath -Force
    }
    Write-Host 'Replay batches preserve visual graphics and fast imported headless selection'
    Write-Host "Catalog integration passed: $($catalogs[0].tests.Count) top-level entries, $($catalogs[0].support.Count) support scripts"

} finally {
    $producerLock.Dispose()
    Remove-Item -LiteralPath $fixtureRoot -Recurse -Force
}
