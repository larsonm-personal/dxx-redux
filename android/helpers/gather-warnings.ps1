#!/usr/bin/env pwsh
# gather-warnings.ps1 -- Build the Android project and capture compiler warnings.
# Writes all warnings to temp/warnings-YYYY-MM-DD.log.
#
# Usage:
#   .\gather-warnings.ps1               # all warnings (C/C++ + Kotlin)
#   .\gather-warnings.ps1 --native-only # C/C++ warnings only
#   .\gather-warnings.ps1 --kotlin-only # Kotlin warnings only
#
# NOTE: The log will contain warnings from d1/ and d2/ source files compiled
# via the NDK. When feeding this to a fixer, instruct it to only fix files
# under android/ -- do not modify d1/ or d2/ code.

param(
    [switch]$NativeOnly,
    [switch]$KotlinOnly
)

$ErrorActionPreference = "Stop"
$androidRoot = Split-Path $PSScriptRoot
$repoRoot = Split-Path $androidRoot

# Ensure temp/ exists
$tempDir = Join-Path $repoRoot "temp"
if (-not (Test-Path $tempDir)) {
    New-Item -ItemType Directory -Path $tempDir | Out-Null
}

$datestamp = Get-Date -Format "yyyy-MM-dd"
$logFile = Join-Path $tempDir "warnings-$datestamp.log"

# --- Set JAVA_HOME if needed ---
$depBaseFile = Join-Path $repoRoot "dependency_base.txt"
if (-not $env:JAVA_HOME -and (Test-Path $depBaseFile)) {
    $DEP_BASE = (Get-Content $depBaseFile -First 1).Trim()
    $jdk = Get-ChildItem "$DEP_BASE\jdk-*" -Directory -ErrorAction SilentlyContinue |
        Sort-Object Name -Descending | Select-Object -First 1
    if ($jdk) {
        $env:JAVA_HOME = $jdk.FullName
        Write-Host "JAVA_HOME = $env:JAVA_HOME"
    }
}

# --- Run Gradle build and capture output ---
Write-Host "Running assembleDebug to gather warnings..."
Write-Host "Log file: $logFile"

$gradlew = Join-Path $androidRoot "gradlew.bat"
if (-not (Test-Path $gradlew)) {
    Write-Error "gradlew.bat not found at $gradlew"
    exit 1
}

# Run build, capturing both stdout and stderr
$output = & $gradlew -p $androidRoot assembleDebug --no-daemon 2>&1 | Out-String -Stream

# Filter for warning lines
$warnings = @()
$header = @(
    "# Compiler warnings gathered on $datestamp",
    "# Source: gradlew assembleDebug",
    "# NOTE: Do NOT modify d1/ or d2/ source files to fix these warnings",
    "#       Only fix warnings in files under android/.",
    ""
)

foreach ($line in $output) {
    $isNativeWarning = $line -match ':\d+:\d+: warning:' -or $line -match '\[-W'
    $isKotlinWarning = $line -match 'w: ' -and $line -match '\.kt:'

    if ($NativeOnly -and $isNativeWarning) {
        $warnings += $line
    } elseif ($KotlinOnly -and $isKotlinWarning) {
        $warnings += $line
    } elseif (-not $NativeOnly -and -not $KotlinOnly -and ($isNativeWarning -or $isKotlinWarning)) {
        $warnings += $line
    }
}

($header + $warnings) | Set-Content $logFile -Encoding UTF8

$nativeCount = ($warnings | Where-Object { $_ -match ':\d+:\d+: warning:' -or $_ -match '\[-W' }).Count
$kotlinCount = ($warnings | Where-Object { $_ -match 'w: ' -and $_ -match '\.kt:' }).Count

Write-Host ""
Write-Host "Warnings found:"
Write-Host "  C/C++ (NDK):  $nativeCount"
Write-Host "  Kotlin:       $kotlinCount"
Write-Host "  Total:        $($warnings.Count)"
Write-Host ""
Write-Host "Written to: $logFile"
