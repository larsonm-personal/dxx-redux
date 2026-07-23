#!/usr/bin/env pwsh
$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'input_demo_host_build_guard.ps1')

function Assert-True {
    param(
        [bool]$Condition,
        [string]$Message
    )

    if (-not $Condition) {
        throw $Message
    }
}

$repoRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$testRoot = Join-Path $repoRoot 'temp\test_input_demo_host_build_guard'
$caseRoot = Join-Path $testRoot ([guid]::NewGuid().ToString('N'))

try {
    New-Item -ItemType Directory -Path (Join-Path $caseRoot 'd2') -Force | Out-Null
    [System.IO.File]::WriteAllText((Join-Path $caseRoot 'd2\source.c'), "int value;`n")
    & git -C $caseRoot init --quiet 2>$null
    & git -C $caseRoot config user.email 'input-demo-test@example.invalid' 2>$null
    & git -C $caseRoot config user.name 'Input Demo Test' 2>$null
    & git -C $caseRoot add d2/source.c 2>$null
    & git -C $caseRoot commit --quiet -m 'source' 2>$null

    $executablePath = Join-Path $caseRoot 'buildd2\main\test.exe'
    New-Item -ItemType Directory -Path (Split-Path $executablePath -Parent) -Force | Out-Null
    [System.IO.File]::WriteAllText($executablePath, 'test')
    $stampPath = Get-InputDemoBuildStampPath -RepoRoot $caseRoot -GameName 'd2'
    New-Item -ItemType Directory -Path (Split-Path $stampPath -Parent) -Force | Out-Null
    $revision = Get-InputDemoSourceRevision -RepoRoot $caseRoot
    [System.IO.File]::WriteAllText($stampPath, $revision)

    $issue = Get-InputDemoExecutableFreshnessIssue -RepoRoot $caseRoot -GameName 'd2' -ExecutablePath $executablePath
    Assert-True ($null -eq $issue) 'Matching revision stamp should accept a fresh executable'

    [System.IO.File]::WriteAllText((Join-Path $caseRoot 'README.md'), "revision only`n")
    & git -C $caseRoot add README.md 2>$null
    & git -C $caseRoot commit --quiet -m 'revision change' 2>$null
    $issue = Get-InputDemoExecutableFreshnessIssue -RepoRoot $caseRoot -GameName 'd2' -ExecutablePath $executablePath
    Assert-True ($null -ne $issue) 'A revision change must reject an executable even when source timestamps are unchanged'
    Assert-True ($issue.SourceRelative -eq 'source revision') 'Revision mismatch should identify its freshness cause'

    Write-Host 'Input demo host build guard tests passed'
} finally {
    $resolvedTestRoot = [System.IO.Path]::GetFullPath($testRoot)
    $resolvedCaseRoot = [System.IO.Path]::GetFullPath($caseRoot)
    if ($resolvedCaseRoot.StartsWith($resolvedTestRoot + [System.IO.Path]::DirectorySeparatorChar) -and
        (Test-Path -LiteralPath $resolvedCaseRoot)) {
        Remove-Item -LiteralPath $resolvedCaseRoot -Recurse -Force
    }
}
