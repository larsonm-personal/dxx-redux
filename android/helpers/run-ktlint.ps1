#!/usr/bin/env pwsh
# run-ktlint.ps1 -- Run ktlint on Kotlin source files.
# Usage:
#   .\run-ktlint.ps1          # auto-fix formatting (default)
#   .\run-ktlint.ps1 -Check  # report issues, exit 1 if any
#   .\run-ktlint.ps1 -Paths path\to\file path\to\dir

param(
    [switch]$Check,
    [string[]]$Paths
)

$ErrorActionPreference = "Stop"
$androidRoot = Split-Path $PSScriptRoot
$repoRoot = Split-Path $androidRoot
$platformHelper = Join-Path $androidRoot "get_deps/helpers/Get-DepPlatform.ps1"
. $platformHelper

. (Join-Path $PSScriptRoot "code-quality-files.ps1")
$Paths = @(Get-CodeQualityScriptPaths -InputPaths $Paths -RemainingPaths @($args) -ExplicitScope ($PSBoundParameters.ContainsKey('Paths')))

# --- Gather Kotlin files ---
# Include the production source selection for each distribution
$files = @(Get-CodeQualityScopedFiles -RepoRoot $repoRoot -RootPath $repoRoot -InputPaths $Paths -ValidExtensions @('.kt', '.kts'))

if ($files.Count -eq 0) {
    Write-Host "No Kotlin files found"
    exit 0
}


# --- Locate dependencies ---
$DEP_BASE = Get-DependencyBase -RepoRoot $repoRoot
if (-not $DEP_BASE) {
    $depBaseFile = Join-Path $repoRoot "dependency_base.txt"
    Write-Error "dependency_base.txt not found at $depBaseFile"
    exit 1
}

# Load versions from tool_versions.conf
$confFile = Join-Path $androidRoot "get_deps\tool_versions.conf"
$ktlintVersion = $null
$jdkMajor = $null
foreach ($line in Get-Content $confFile) {
    if ($line -match '^KTLINT_VERSION=(.+)$') { $ktlintVersion = $Matches[1] }
    if ($line -match '^JDK_MAJOR=(.+)$') { $jdkMajor = $Matches[1] }
}

# Find ktlint jar
$ktlintJar = Join-Path $DEP_BASE "ktlint-$ktlintVersion\ktlint.jar"
if (-not (Test-Path $ktlintJar)) {
    Write-Error "ktlint not found at $ktlintJar. Run android/get_deps/helpers/get_ktlint.sh to install"
    exit 1
}

# Find java
$java = $null
$jdkBinDir = Join-Path (Join-Path $DEP_BASE "jdk-$jdkMajor") "bin"
$java = Get-PlatformToolPath -BaseDir $jdkBinDir -ToolName "java"
if (-not $java) {
    $inPath = Get-Command "java" -ErrorAction SilentlyContinue
    if ($inPath) {
        $java = $inPath.Source
    }
}
if (-not $java) {
    Write-Error "java not found. Install JDK or run android/get_deps/helpers/get_jdk.sh"
    exit 1
}

Write-Host "Using java: $java"
Write-Host "Using ktlint: $ktlintJar"


Write-Host "Found $($files.Count) Kotlin files"

# Bound native argument lengths on Windows; include JVM test and CLI sources
$failed = $false
for ($index = 0; $index -lt $files.Count; $index += 30) {
    $batch = @($files[$index..([Math]::Min($index + 29, $files.Count - 1))] | ForEach-Object { $_.FullName })
    $toolArguments = @('-jar', $ktlintJar)
    if (-not $Check) { $toolArguments += '--format' }
    & $java @toolArguments @batch
    if ($LASTEXITCODE -ne 0) { $failed = $true }
}
if ($failed) { exit 1 }
Write-Host 'Kotlin formatting checks passed'
exit 0
