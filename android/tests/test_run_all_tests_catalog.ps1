#!/usr/bin/env pwsh
# Exercise real catalog discovery without provisioning or running the discovered tests
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $repoRoot 'android/helpers/headless_process_pool.ps1')
. (Join-Path $repoRoot 'android/helpers/test_host_platform.ps1')
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
        $catalogs += $catalog
    }
    if (-not @($catalogs[0].tests | Where-Object name -like '*graphics_canary').Count -or @($catalogs[1].tests | Where-Object name -like '*graphics_canary').Count) { throw 'Extended replay matrix did not replace canaries' }
    Write-Host "Catalog integration passed: $($catalogs[0].tests.Count) top-level entries, $($catalogs[0].support.Count) support scripts"

} finally {
    $producerLock.Dispose()
    Remove-Item -LiteralPath $fixtureRoot -Recurse -Force
}
