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
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'

. "$PSScriptRoot\..\helpers\test_env.ps1"

$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$games = if ($Game -eq 'both') { @('d1', 'd2') } else { @($Game) }

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
