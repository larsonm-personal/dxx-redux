#!/usr/bin/env pwsh
# Synthetic fixture lives under the operating-system temp directory, never the repository

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$helper = Join-Path $repoRoot "android\helpers\clean-old-artifacts.ps1"
$retentionHelper = Join-Path $repoRoot "android\helpers\retain-recent-artifacts.ps1"
$fixtureRoot = Join-Path ([IO.Path]::GetTempPath()) "dxx-clean-old-artifacts-TEST_ONLY-$([guid]::NewGuid().ToString('N'))"

function Assert-Exists {
    param([Parameter(Mandatory)][string]$RelativePath)

    if (-not (Test-Path -LiteralPath (Join-Path $fixtureRoot $RelativePath))) {
        throw "Expected path to exist: $RelativePath"
    }
}

function Assert-Missing {
    param([Parameter(Mandatory)][string]$RelativePath)

    if (Test-Path -LiteralPath (Join-Path $fixtureRoot $RelativePath)) {
        throw "Expected path to be removed: $RelativePath"
    }
}

function New-FixtureDirectory {
    param([Parameter(Mandatory)][string]$RelativePath)

    $path = Join-Path $fixtureRoot $RelativePath
    New-Item -ItemType Directory -Path $path -Force | Out-Null
    [IO.File]::WriteAllText((Join-Path $path "TEST_ONLY_artifact.txt"), "synthetic cleanup fixture")
}

function New-FixtureFile {
    param([Parameter(Mandatory)][string]$RelativePath)

    $path = Join-Path $fixtureRoot $RelativePath
    New-Item -ItemType Directory -Path (Split-Path $path) -Force | Out-Null
    [IO.File]::WriteAllText($path, "synthetic cleanup fixture")
}

try {
    if (Test-Path -LiteralPath $fixtureRoot) {
        Remove-Item -LiteralPath $fixtureRoot -Recurse -Force
    }

    foreach ($relativePath in @(
            "temp\TEST_ONLY_run_bucket\TEST_ONLY_run_20260101_010101",
            "temp\TEST_ONLY_run_bucket\TEST_ONLY_run_20260102_010101",
            "temp\TEST_ONLY_run_bucket\TEST_ONLY_run_20260103_010101",
            "temp\TEST_ONLY_future_tool_run_20260101_010101",
            "temp\TEST_ONLY_future_tool_run_20260102_010101",
            "android\temp_TEST_ONLY\TEST_ONLY_snapshots\TEST_ONLY_capture-20260101-010101",
            "android\temp_TEST_ONLY\TEST_ONLY_snapshots\TEST_ONLY_capture-20260102-010101",
            "temp\TEST_ONLY_stable_cache",
            "temp\TEST_ONLY_vendor_workspace\TEST_ONLY_nested"
        )) {
        New-FixtureDirectory -RelativePath $relativePath
    }

    foreach ($relativePath in @(
            "android\build-outputs\TEST_ONLY_package-internal-20260101-010101-v1.test-output",
            "android\build-outputs\TEST_ONLY_package-internal-20260102-010101-v2.test-output",
            "android\build-outputs\TEST_ONLY_package-release-20260101-010101-v1.test-output",
            "temp\TEST_ONLY_reports\TEST_ONLY_report_20260101_010101.test-output",
            "temp\TEST_ONLY_reports\TEST_ONLY_report_20260102_010101.test-output",
            "temp\TEST_ONLY_warnings-2026-01-01.test-output",
            "temp\TEST_ONLY_warnings-2026-01-02.test-output",
            "temp\TEST_ONLY_stable-output.test-output",
            "temp\TEST_ONLY_vendor_workspace\TEST_ONLY_nested\TEST_ONLY_history_20260101_010101.test-output",
            "temp\TEST_ONLY_vendor_workspace\TEST_ONLY_nested\TEST_ONLY_history_20260102_010101.test-output"
        )) {
        New-FixtureFile -RelativePath $relativePath
    }

    $retentionFamily = 1..6 | ForEach-Object {
        $relativePath = "temp\TEST_ONLY_retention\TEST_ONLY_generation_2026010$($_)_010101.test-output"
        New-FixtureFile -RelativePath $relativePath
        Join-Path $fixtureRoot $relativePath
    }
    & $retentionHelper -RepositoryRoot $fixtureRoot -Artifacts $retentionFamily[-1] | Out-Null
    Assert-Missing -RelativePath "temp\TEST_ONLY_retention\TEST_ONLY_generation_20260101_010101.test-output"
    foreach ($path in $retentionFamily[1..5]) {
        Assert-Exists -RelativePath ([IO.Path]::GetRelativePath($fixtureRoot, $path))
    }

    $preview = & $helper -RepositoryRoot $fixtureRoot -MinimumAgeHours 0 2>&1 | Out-String
    foreach ($expected in @(
            'timestamped-generation-directory:',
            'timestamped-output-file:',
            'stable-workspace-directory:',
            'stable-output-file:',
            'Preview only'
        )) {
        if ($preview -notmatch [regex]::Escape($expected)) {
            throw "Preview did not report class counter: $expected"
        }
    }
    Assert-Exists -RelativePath "temp\TEST_ONLY_run_bucket\TEST_ONLY_run_20260101_010101"
    Assert-Exists -RelativePath "android\build-outputs\TEST_ONLY_package-internal-20260101-010101-v1.test-output"

    & $helper -RepositoryRoot $fixtureRoot -MinimumAgeHours 0 -KeepDirectoryGenerations 1 -KeepFileGenerations 1 -Apply -Confirm:$false | Out-Null
    & $helper -RepositoryRoot $fixtureRoot -MinimumAgeHours 0 -KeepDirectoryGenerations 1 -KeepFileGenerations 1 -Apply -Confirm:$false | Out-Null

    foreach ($relativePath in @(
            "temp\TEST_ONLY_run_bucket\TEST_ONLY_run_20260101_010101",
            "temp\TEST_ONLY_run_bucket\TEST_ONLY_run_20260102_010101",
            "temp\TEST_ONLY_future_tool_run_20260101_010101",
            "android\temp_TEST_ONLY\TEST_ONLY_snapshots\TEST_ONLY_capture-20260101-010101",
            "android\build-outputs\TEST_ONLY_package-internal-20260101-010101-v1.test-output",
            "temp\TEST_ONLY_reports\TEST_ONLY_report_20260101_010101.test-output",
            "temp\TEST_ONLY_warnings-2026-01-01.test-output"
        )) {
        Assert-Missing -RelativePath $relativePath
    }

    foreach ($relativePath in @(
            "temp\TEST_ONLY_run_bucket\TEST_ONLY_run_20260103_010101",
            "temp\TEST_ONLY_future_tool_run_20260102_010101",
            "android\temp_TEST_ONLY\TEST_ONLY_snapshots\TEST_ONLY_capture-20260102-010101",
            "android\build-outputs\TEST_ONLY_package-internal-20260102-010101-v2.test-output",
            "android\build-outputs\TEST_ONLY_package-release-20260101-010101-v1.test-output",
            "temp\TEST_ONLY_reports\TEST_ONLY_report_20260102_010101.test-output",
            "temp\TEST_ONLY_warnings-2026-01-02.test-output",
            "temp\TEST_ONLY_stable_cache\TEST_ONLY_artifact.txt",
            "temp\TEST_ONLY_stable-output.test-output",
            "temp\TEST_ONLY_vendor_workspace\TEST_ONLY_nested\TEST_ONLY_history_20260101_010101.test-output",
            "temp\TEST_ONLY_vendor_workspace\TEST_ONLY_nested\TEST_ONLY_history_20260102_010101.test-output"
        )) {
        Assert-Exists -RelativePath $relativePath
    }

    Write-Output "PASS: class-based old artifact cleanup helper"
} finally {
    if (Test-Path -LiteralPath $fixtureRoot) {
        Remove-Item -LiteralPath $fixtureRoot -Recurse -Force
    }
}
