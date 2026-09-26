#!/usr/bin/env pwsh
# run-cmake-lint.ps1 -- Run cmake-lint (cheshirekow/cmakelang) on cmake files
# added by this branch. Skips upstream d1/ and d2/ CMakeLists.txt files.
# Usage:
#   .\run-cmake-lint.ps1            # report issues, exit 1 if any
#   .\run-cmake-lint.ps1 -Paths path\to\file path\to\dir

param(
    [string[]]$Paths
)

$ErrorActionPreference = "Stop"
$androidRoot = Split-Path $PSScriptRoot
$repoRoot = Split-Path $androidRoot

. (Join-Path $PSScriptRoot "code-quality-files.ps1")

$depBaseFile = Join-Path $repoRoot "dependency_base.txt"
if (-not (Test-Path $depBaseFile)) {
    Write-Error "dependency_base.txt not found at $depBaseFile"
    exit 1
}
$DEP_BASE = (Get-Content $depBaseFile -First 1).Trim()

$confFile = Join-Path $androidRoot "get_deps\tool_versions.conf"
$cmVersion = $null
foreach ($line in Get-Content $confFile) {
    if ($line -match '^CMAKELANG_VERSION=(.+)$') { $cmVersion = $Matches[1] }
}

$cmakeLint = $null
foreach ($candidate in @(
        (Join-Path $DEP_BASE "cmakelang-$cmVersion\python\Scripts\cmake-lint.exe"),
        (Join-Path $DEP_BASE "cmakelang-$cmVersion\venv\bin\cmake-lint")
    )) {
    if (Test-Path $candidate) { $cmakeLint = $candidate; break }
}
if (-not $cmakeLint) {
    $inPath = Get-Command "cmake-lint" -ErrorAction SilentlyContinue
    if ($inPath) { $cmakeLint = $inPath.Source }
}
if (-not $cmakeLint) {
    Write-Error "cmake-lint not found. Run android/get_deps/helpers/get_cmake_format.sh to install"
    exit 1
}
Write-Host "Using: $cmakeLint"
& $cmakeLint --version

$files = @(Get-CodeQualityCmakeFiles -RepoRoot $repoRoot -InputPaths $Paths)
if ($files.Count -eq 0) {
    Write-Host "No cmake files in scope to lint"
    exit 0
}
Write-Host "Found $($files.Count) cmake files"

$savedPref = $ErrorActionPreference
$ErrorActionPreference = "Continue"
$badFiles = @()
foreach ($f in $files) {
    & $cmakeLint "$($f.FullName)"
    if ($LASTEXITCODE -ne 0) { $badFiles += $f.FullName }
}
$ErrorActionPreference = $savedPref

if ($badFiles.Count -gt 0) {
    Write-Host ""
    Write-Host "cmake-lint reported issues in $($badFiles.Count) file(s)"
    exit 1
}
Write-Host "All cmake files pass cmake-lint"
