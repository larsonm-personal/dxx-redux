#!/usr/bin/env pwsh
# Tooling checks that need no game media, native build, or Android device
[CmdletBinding()]
param([string]$OutputRoot, [switch]$IncludeBoundedRuntime)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $repoRoot 'android/helpers/test_host_platform.ps1')
. (Join-Path $repoRoot 'android/helpers/headless_process_pool.ps1')
. (Join-Path $repoRoot 'android/helpers/atomic_text_file.ps1')
$tests = @(
    'test_test_execution_evidence', 'test_download_verification', 'test_d2xxl_sound_format', 'test_d2xxl_tga_pixels',
    'test_dep_platform', 'test_clean_workspace', 'test_clean_old_artifacts',
    'test_host_process_cleanup', 'test_headless_process_pool', 'test_managed_dependencies',
    'test_standard_game_data_resolution', 'test_sdk_package_inventory', 'test_sdk_package_cleanup', 'test_sdk_writer_lock', 'test_test_process_output_capture',
    'test_code_quality_files', 'test_formatter_process_cleanup', 'test_input_demo_comparison_policy', 'test_run_all_tests_catalog', 'test_validate_automation_catalog',
    'test_github_release'
)
# These shell fixtures simulate Linux installations and Windows archive packages
$linuxInstallerFixtures = [Runtime.InteropServices.RuntimeInformation]::IsOSPlatform([Runtime.InteropServices.OSPlatform]::Linux)
if ($linuxInstallerFixtures) { $tests += 'test_dependency_install' }
if (Test-RegressionWindowsHost) { $tests += 'test_regression_process_lifetime' }
if ($IncludeBoundedRuntime) { $tests += @('test_bounded_python_runtime', 'test_bounded_extraction') }
if (-not $OutputRoot) {
    $OutputRoot = Join-Path $repoRoot ('android/temp/tooling_smoke/run_' + [guid]::NewGuid().ToString('N'))
}
$OutputRoot = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputRoot)
if (Test-Path -LiteralPath $OutputRoot) { throw "Use a new output directory: $OutputRoot" }
& (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $OutputRoot -DirectoryPrefix run_
New-Item -ItemType Directory -Path $OutputRoot | Out-Null
$results = [Collections.Generic.List[object]]::new()
$producerLock = $null
try {
    $producerLock = [IO.File]::Open((Join-Path $OutputRoot 'producer.lock'), [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    $tasks = @($tests | ForEach-Object {
            [pscustomobject]@{
                Name = $_
                FilePath = Get-RegressionCurrentPwshPath
                Arguments = @('-NoProfile', '-NonInteractive', '-File', (Join-Path $PSScriptRoot "$_.ps1"))
                WorkingDirectory = $repoRoot
                TimeoutSeconds = 180
            }
        })
    Invoke-HeadlessProcessPool -Tasks $tasks -MaxParallel 1 -OnCompleted {
        param($task, $result)
        $log = Join-Path $OutputRoot "$($task.Name).log"
        Write-Utf8NoBomTextAtomically -Path $log -Text ($result.StandardOutput + "`n" + $result.StandardError)
        $status = if ($result.TimedOut) { 'TIMEOUT' } elseif ($result.ExitCode -eq 0) { 'PASS' } else { 'FAIL' }
        $results.Add([pscustomobject]@{ name = $task.Name; status = $status; exit_code = $result.ExitCode; log = $log })
        Write-Host "$status $($task.Name)"
        if ($status -ne 'PASS') {
            Write-Host "Failure diagnostics: $log"
            Write-Host ($result.StandardOutput + "`n" + $result.StandardError)
        }
        $summary = [ordered]@{
            host = [Runtime.InteropServices.RuntimeInformation]::OSDescription
            linux_installer_fixtures = $linuxInstallerFixtures
            results = @($results.ToArray())
        }
        Write-Utf8NoBomTextAtomically -Path (Join-Path $OutputRoot 'summary.json') -Text (($summary | ConvertTo-Json -Depth 5) + "`n")
    }
} finally {
    if ($producerLock) { $producerLock.Dispose() }
}
Write-Host "Tooling smoke report: $OutputRoot"
if ($results.Count -ne $tests.Count -or @($results | Where-Object status -ne PASS).Count) { exit 1 }
exit 0
