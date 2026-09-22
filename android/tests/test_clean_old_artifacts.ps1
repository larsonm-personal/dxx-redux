#!/usr/bin/env pwsh
# Synthetic fixture lives under the operating-system temp directory, never the repository

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $repoRoot "android\helpers\powershell_compat.ps1")
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
    New-Item -ItemType Directory -Path $fixtureRoot | Out-Null
    & git -C $fixtureRoot init --quiet
    if ($LASTEXITCODE -ne 0) { throw 'Fixture Git init failed' }
    [IO.File]::WriteAllText((Join-Path $fixtureRoot '.gitignore'), "temp/`nandroid/temp_TEST_ONLY/`nandroid/build-outputs/`n")

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
    foreach ($path in $retentionFamily) { (Get-Item -LiteralPath $path).LastWriteTime = Get-Date }
    & $retentionHelper -RepositoryRoot $fixtureRoot -Artifacts $retentionFamily[-1] | Out-Null
    Assert-Missing -RelativePath "temp\TEST_ONLY_retention\TEST_ONLY_generation_20260101_010101.test-output"
    Assert-Missing -RelativePath "temp\TEST_ONLY_retention\TEST_ONLY_generation_20260102_010101.test-output"
    foreach ($path in $retentionFamily[2..5]) {
        Assert-Exists -RelativePath (Get-CompatibleRelativePath -BasePath $fixtureRoot -TargetPath $path)
    }

    $planned = Join-Path $fixtureRoot "temp/TEST_ONLY_retention/TEST_ONLY_generation_20260107_010101.test-output"
    & $retentionHelper -RepositoryRoot $fixtureRoot -Artifacts $planned | Out-Null
    if (Test-Path -LiteralPath $planned) { throw 'Startup retention created the planned artifact' }
    if (@(Get-ChildItem (Split-Path $planned) -File).Count -ne 3) { throw 'Startup must keep three prior outputs' }
    [IO.File]::WriteAllText($planned, 'new output')
    if (@(Get-ChildItem (Split-Path $planned) -File).Count -ne 4) { throw 'Production should leave four outputs' }
    & $retentionHelper -RepositoryRoot $fixtureRoot -Artifacts $planned | Out-Null
    if (@(Get-ChildItem (Split-Path $planned) -File).Count -ne 4) { throw 'Nested producers must not count the current output as prior history' }

    $packages = 1..5 | ForEach-Object {
        $relativePath = "android/build-outputs/TEST_ONLY_deploy-internal-v$_-$('a' * 32).aab"
        New-FixtureFile -RelativePath $relativePath
        $path = Join-Path $fixtureRoot $relativePath
        (Get-Item -LiteralPath $path).LastWriteTime = (Get-Date).AddSeconds($_)
        $path
    }
    $plannedPackage = Join-Path $fixtureRoot "android/build-outputs/TEST_ONLY_deploy-internal-v6-$('b' * 32).aab"
    & $retentionHelper -RepositoryRoot $fixtureRoot -Artifacts $plannedPackage | Out-Null
    foreach ($path in $packages[0..1]) {
        if (Test-Path -LiteralPath $path) { throw 'Package startup retention kept an older package' }
    }
    foreach ($path in $packages[2..4]) {
        if (-not (Test-Path -LiteralPath $path)) { throw 'Package startup retention removed a recent package' }
    }

    # Milliseconds belong to the timestamp, not to a distinct family per run
    foreach ($stamp in @('20260101_120000_111', '20260102_120000_222', '20260103_120000_333', '20260104_120000_444')) {
        New-FixtureDirectory "temp/TEST_ONLY_millis_$stamp"
    }
    & $retentionHelper -RepositoryRoot $fixtureRoot -Artifacts (Join-Path $fixtureRoot 'temp/TEST_ONLY_millis_20260105_120000_555') -Keep 2 | Out-Null
    Assert-Missing 'temp/TEST_ONLY_millis_20260101_120000_111'
    Assert-Missing 'temp/TEST_ONLY_millis_20260102_120000_222'
    Assert-Exists 'temp/TEST_ONLY_millis_20260103_120000_333'
    Assert-Exists 'temp/TEST_ONLY_millis_20260104_120000_444'

    # Explicit ownership groups descriptive names and caps bytes independently of count
    foreach ($name in @('old', 'middle', 'new')) { New-FixtureDirectory "temp/TEST_ONLY_owned_$name" }
    (Get-Item (Join-Path $fixtureRoot 'temp/TEST_ONLY_owned_old')).LastWriteTime = (Get-Date).AddDays(-3)
    (Get-Item (Join-Path $fixtureRoot 'temp/TEST_ONLY_owned_middle')).LastWriteTime = (Get-Date).AddDays(-2)
    $oneArtifactBytes = (Get-Item (Join-Path $fixtureRoot 'temp/TEST_ONLY_owned_new/TEST_ONLY_artifact.txt')).Length
    & $retentionHelper -RepositoryRoot $fixtureRoot -Artifacts (Join-Path $fixtureRoot 'temp/TEST_ONLY_owned_planned') `
        -DirectoryPrefix 'TEST_ONLY_owned_' -Keep 3 -MaxFamilyBytes $oneArtifactBytes | Out-Null
    Assert-Missing 'temp/TEST_ONLY_owned_old'
    Assert-Missing 'temp/TEST_ONLY_owned_middle'
    Assert-Exists 'temp/TEST_ONLY_owned_new'
    New-FixtureDirectory 'temp/TEST_ONLY_owned_active'
    $leasePath = Join-Path $fixtureRoot 'temp/TEST_ONLY_owned_active/producer.lock'
    $lease = [IO.File]::Open($leasePath, 'Create', 'Write', 'Read')
    try {
        & $retentionHelper -RepositoryRoot $fixtureRoot -Artifacts (Join-Path $fixtureRoot 'temp/TEST_ONLY_owned_planned') `
            -DirectoryPrefix 'TEST_ONLY_owned_' -MaxFamilyBytes 1 | Out-Null
        Assert-Exists 'temp/TEST_ONLY_owned_active'
    } finally { $lease.Dispose() }
    # A repository inside the explicit prefix is still protected
    New-FixtureFile 'temp/TEST_ONLY_owned_vendor/.git/config'
    & $retentionHelper -RepositoryRoot $fixtureRoot -Artifacts (Join-Path $fixtureRoot 'temp/TEST_ONLY_owned_planned') `
        -DirectoryPrefix 'TEST_ONLY_owned_' -MaxFamilyBytes 1 | Out-Null
    Assert-Exists 'temp/TEST_ONLY_owned_vendor/.git/config'
    New-FixtureFile 'temp/TEST_ONLY_owned_tracked/notes.txt'
    & git -C $fixtureRoot add -f temp/TEST_ONLY_owned_tracked/notes.txt
    if ($LASTEXITCODE -ne 0) { throw 'Could not protect fixture notes' }
    & $retentionHelper -RepositoryRoot $fixtureRoot -Artifacts (Join-Path $fixtureRoot 'temp/TEST_ONLY_owned_planned') `
        -DirectoryPrefix 'TEST_ONLY_owned_' -MaxFamilyBytes 1 | Out-Null
    Assert-Exists 'temp/TEST_ONLY_owned_tracked/notes.txt'

    $preview = & $helper -RepositoryRoot $fixtureRoot 2>&1 | Out-String
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
        $expectedPrefix = [IO.Path]::GetFullPath((Join-Path ([IO.Path]::GetTempPath()) 'dxx-clean-old-artifacts-TEST_ONLY-'))
        if (-not ([IO.Path]::GetFullPath($fixtureRoot)).StartsWith($expectedPrefix, [StringComparison]::OrdinalIgnoreCase)) {
            throw 'Unexpected artifact-retention fixture path'
        }
        Remove-Item -LiteralPath $fixtureRoot -Recurse -Force
    }
}
