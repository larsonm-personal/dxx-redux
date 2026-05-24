#!/usr/bin/env pwsh
# run-ktlint.ps1 -- Run ktlint on Kotlin source files.
# Usage:
#   .\run-ktlint.ps1          # auto-fix formatting (default)
#   .\run-ktlint.ps1 --check  # report issues, exit 1 if any
#   .\run-ktlint.ps1 -Paths path\to\file path\to\dir

param(
    [switch]$Check,
    [string[]]$Paths
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path $PSScriptRoot
$platformHelper = Join-Path $PSScriptRoot "get_deps/Get-DepPlatform.ps1"
. $platformHelper

function Get-ScopedFiles {
    param(
        [string]$RootPath,
        [string[]]$InputPaths,
        [string[]]$ValidExtensions
    )

    $results = @()
    if ($InputPaths -and $InputPaths.Count -gt 0) {
        foreach ($inputPath in $InputPaths) {
            if ([string]::IsNullOrWhiteSpace($inputPath)) {
                continue
            }

            $candidate = $inputPath
            if (-not [System.IO.Path]::IsPathRooted($candidate)) {
                $candidate = Join-Path $repoRoot $candidate
            }

            $item = Get-Item -LiteralPath $candidate -ErrorAction SilentlyContinue
            if (-not $item) {
                continue
            }

            if ($item.PSIsContainer) {
                $results += Get-ChildItem -LiteralPath $item.FullName -Recurse -File
            } else {
                $results += $item
            }
        }
    } else {
        $results = Get-ChildItem -Path $RootPath -Recurse -File
    }

    return @($results | Where-Object {
            $_.FullName.StartsWith($RootPath, [System.StringComparison]::OrdinalIgnoreCase) -and
            ($ValidExtensions -contains $_.Extension.ToLowerInvariant())
        } | Sort-Object FullName -Unique)
}

# --- Locate dependencies ---
$DEP_BASE = Get-DependencyBase -RepoRoot $repoRoot
if (-not $DEP_BASE) {
    $depBaseFile = Join-Path $repoRoot "dependency_base.txt"
    Write-Error "dependency_base.txt not found at $depBaseFile"
    exit 1
}

# Load versions from tool_versions.conf
$confFile = Join-Path $PSScriptRoot "get_deps\tool_versions.conf"
$ktlintVersion = $null
$jdkMajor = $null
foreach ($line in Get-Content $confFile) {
    if ($line -match '^KTLINT_VERSION=(.+)$') { $ktlintVersion = $Matches[1] }
    if ($line -match '^JDK_MAJOR=(.+)$') { $jdkMajor = $Matches[1] }
}

# Find ktlint jar
$ktlintJar = Join-Path $DEP_BASE "ktlint-$ktlintVersion\ktlint.jar"
if (-not (Test-Path $ktlintJar)) {
    Write-Error "ktlint not found at $ktlintJar. Run android/get_deps/get_ktlint.sh to install"
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
    Write-Error "java not found. Install JDK or run android/get_deps/get_jdk.sh"
    exit 1
}

Write-Host "Using java: $java"
Write-Host "Using ktlint: $ktlintJar"

# --- Gather Kotlin files ---
$ktDir = Join-Path $PSScriptRoot "app\src\main\java"
$files = Get-ScopedFiles -RootPath $ktDir -InputPaths $Paths -ValidExtensions @('.kt')

if ($files.Count -eq 0) {
    Write-Host "No Kotlin files found"
    exit 0
}

Write-Host "Found $($files.Count) Kotlin files"

# --- Run ---
$patterns = ($files | ForEach-Object { $_.FullName }) -join " "

if ($Check) {
    Write-Host "Checking..."
    & $java -jar $ktlintJar $files.FullName
    if ($LASTEXITCODE -ne 0) {
        Write-Host ""
        Write-Host "ktlint found issues. Run without --check to auto-fix"
        exit 1
    }
    Write-Host "All Kotlin files pass ktlint checks"
} else {
    Write-Host "Formatting..."
    & $java -jar $ktlintJar --format $files.FullName
    Write-Host "Done"
}
