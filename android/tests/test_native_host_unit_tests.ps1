#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Builds and runs native host CTest suites for D1 and D2.

.EXAMPLE
    .\test_native_host_unit_tests.ps1
    .\test_native_host_unit_tests.ps1 -Game d2
#>

param(
    [ValidateSet('both', 'd1', 'd2')]
    [string]$Game = 'both',
    [string]$Configuration
)

$ErrorActionPreference = 'Stop'

. "$PSScriptRoot\..\helpers\test_env.ps1"

$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$games = if ($Game -eq 'both') { @('d1', 'd2') } else { @($Game) }

$linuxHost = [Runtime.InteropServices.RuntimeInformation]::IsOSPlatform([Runtime.InteropServices.OSPlatform]::Linux)
if (-not $Configuration) {
    $Configuration = if ($linuxHost) { 'Debug' } else { 'Release' }
}

if ($linuxHost) {
    $python = Resolve-RegressionPythonCommand
    if (-not $python) { throw 'Python is required for Linux build CLI regression tests' }
    & $python.Path @($python.PrefixArguments) (Join-Path $PSScriptRoot 'test_linux_build_cli.py')
    if ($LASTEXITCODE -ne 0) { throw 'Linux build CLI regression tests failed' }
    & $python.Path @($python.PrefixArguments) (Join-Path $PSScriptRoot 'test_run_host_test_suites.py')
    if ($LASTEXITCODE -ne 0) { throw 'Host suite adapter checks failed' }
    & $python.Path @($python.PrefixArguments) (Join-Path $PSScriptRoot 'test_nightly_tooling_sweep.py')
    if ($LASTEXITCODE -ne 0) { throw 'Nightly sweep planner checks failed' }
    & $python.Path @($python.PrefixArguments) (Join-Path $PSScriptRoot 'test_public_sanitizer_runner.py')
    if ($LASTEXITCODE -ne 0) { throw 'Public sanitizer runner regression tests failed' }
    & bash (Join-Path $repoRoot 'run-linux-build.sh') --target $Game --test --build-type $Configuration
    if ($LASTEXITCODE -ne 0) { throw "Native Linux build/tests failed with exit code $LASTEXITCODE" }
    return
}

function Invoke-HostBuild {
    param([string]$GameName)

    Invoke-RegressionHostBuild -RepoRoot $repoRoot -Target $GameName -Label $GameName
}

function Invoke-NativeCTest {
    param([string]$GameName)

    $buildDirName = if ($GameName -eq 'd1') { 'buildd1' } else { 'buildd2' }
    $buildDir = Join-Path $repoRoot $buildDirName
    if (-not (Test-Path -LiteralPath (Join-Path $buildDir 'CTestTestfile.cmake'))) {
        throw "CTest metadata not found for $GameName in $buildDir"
    }

    Write-Host ""
    Write-Host "Running native CTest suite for $GameName"
    # test_coop_transition_policy.ps1 is the focused entry point for the
    # transition-policy target included here through android/tests/CMakeLists.txt
    ctest --test-dir $buildDir -C $Configuration --output-on-failure
    if ($LASTEXITCODE -ne 0) {
        throw "CTest failed for $GameName with exit code $LASTEXITCODE"
    }
}

foreach ($gameName in $games) {
    Write-Host "Building native host target $gameName"
    Invoke-HostBuild -GameName $gameName
    Invoke-NativeCTest -GameName $gameName
}

Write-Host ""
Write-Host "Native host unit tests passed"
