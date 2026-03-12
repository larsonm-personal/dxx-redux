# run-ktlint.ps1 -- Run ktlint on Kotlin source files.
# Usage:
#   .\run-ktlint.ps1          # auto-fix formatting (default)
#   .\run-ktlint.ps1 --check  # report issues, exit 1 if any

param(
    [switch]$Check
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path $PSScriptRoot

# --- Locate dependencies ---
$depBaseFile = Join-Path $repoRoot "dependency_base.txt"
if (-not (Test-Path $depBaseFile)) {
    Write-Error "dependency_base.txt not found at $depBaseFile"
    exit 1
}
$DEP_BASE = (Get-Content $depBaseFile -First 1).Trim()

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
    Write-Error "ktlint not found at $ktlintJar. Run android/get_deps/get_ktlint.sh to install."
    exit 1
}

# Find java
$java = $null
$jdkDir = Join-Path $DEP_BASE "jdk-$jdkMajor"
$candidate = Join-Path $jdkDir "bin\java.exe"
if (Test-Path $candidate) {
    $java = $candidate
} else {
    $inPath = Get-Command "java" -ErrorAction SilentlyContinue
    if ($inPath) {
        $java = $inPath.Source
    }
}
if (-not $java) {
    Write-Error "java not found. Install JDK or run android/get_deps/get_jdk.sh."
    exit 1
}

Write-Host "Using java: $java"
Write-Host "Using ktlint: $ktlintJar"

# --- Gather Kotlin files ---
$ktDir = Join-Path $PSScriptRoot "app\src\main\java"
$files = Get-ChildItem -Path $ktDir -Recurse -Include "*.kt"

if ($files.Count -eq 0) {
    Write-Host "No Kotlin files found."
    exit 0
}

Write-Host "Found $($files.Count) Kotlin files."

# --- Run ---
$patterns = ($files | ForEach-Object { $_.FullName }) -join " "

if ($Check) {
    Write-Host "Checking..."
    & $java -jar $ktlintJar $files.FullName
    if ($LASTEXITCODE -ne 0) {
        Write-Host ""
        Write-Host "ktlint found issues. Run without --check to auto-fix."
        exit 1
    }
    Write-Host "All Kotlin files pass ktlint checks."
} else {
    Write-Host "Formatting..."
    & $java -jar $ktlintJar --format $files.FullName
    Write-Host "Done."
}
