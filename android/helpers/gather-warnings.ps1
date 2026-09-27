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
& (Join-Path $PSScriptRoot "retain-recent-artifacts.ps1") -Artifacts $logFile

. (Join-Path $PSScriptRoot 'test_host_platform.ps1')
Initialize-RegressionJavaEnvironment -RepoRoot $repoRoot

# --- Run Gradle build and capture output ---
Write-Host "Running assembleDebug to gather warnings..."
Write-Host "Log file: $logFile"

$gradlew = Resolve-RegressionGradleWrapper -AndroidDir $androidRoot

# Run build, capturing both stdout and stderr
$output = & $gradlew -p $androidRoot assembleDebug --no-daemon 2>&1 | Out-String -Stream

$buildExitCode = $LASTEXITCODE

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

[IO.File]::WriteAllText($logFile, (($header + $warnings) -join [Environment]::NewLine) + [Environment]::NewLine, [Text.UTF8Encoding]::new($false))

$nativeCount = ($warnings | Where-Object { $_ -match ':\d+:\d+: warning:' -or $_ -match '\[-W' }).Count
$kotlinCount = ($warnings | Where-Object { $_ -match 'w: ' -and $_ -match '\.kt:' }).Count

Write-Host ""
Write-Host "Warnings found:"
Write-Host "  C/C++ (NDK):  $nativeCount"
Write-Host "  Kotlin:       $kotlinCount"
Write-Host "  Total:        $($warnings.Count)"
Write-Host ""
Write-Host "Written to: $logFile"

if ($buildExitCode -ne 0) { throw "Gradle build failed with exit code $buildExitCode" }
