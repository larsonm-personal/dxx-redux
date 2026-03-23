# gather-warnings-msvc.ps1 -- Build d1 and d2 with MSVC and capture warnings.
# Writes all warnings to temp/warnings-msvc-YYYY-MM-DD.log.
#
# Usage:
#   .\gather-warnings-msvc.ps1          # build both d1 and d2
#   .\gather-warnings-msvc.ps1 --d1     # d1 only
#   .\gather-warnings-msvc.ps1 --d2     # d2 only

param(
    [switch]$D1Only,
    [switch]$D2Only
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path $PSScriptRoot

# Ensure temp/ exists
$tempDir = Join-Path $repoRoot "temp"
if (-not (Test-Path $tempDir)) {
    New-Item -ItemType Directory -Path $tempDir | Out-Null
}

$datestamp = Get-Date -Format "yyyy-MM-dd"
$logFile = Join-Path $tempDir "warnings-msvc-$datestamp.log"

# Kill any zombie cl.exe processes
Get-Process cl -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue

$header = @(
    "# MSVC compiler warnings gathered on $datestamp",
    "# Source: cmake --build (d1 and/or d2)",
    ""
)
$allWarnings = @()

function Build-And-Gather($name, $buildDir) {
    $fullBuildDir = Join-Path $repoRoot $buildDir

    if (-not (Test-Path $fullBuildDir)) {
        Write-Host "Build directory $fullBuildDir does not exist -- skipping $name"
        Write-Host "  Create it with: mkdir $buildDir; cd $buildDir; cmake ..\$name"
        return @()
    }

    Write-Host "Building $name from $buildDir..."

    # Run cmake --build, capture output
    $output = cmake --build $fullBuildDir -- /m /errorlimit:10 2>&1 | Out-String -Stream

    # Filter for MSVC warning lines: "file.c(123): warning C1234: message"
    $warnings = @()
    foreach ($line in $output) {
        if ($line -match '\): warning C\d+:' -or $line -match ': warning :') {
            $warnings += $line
        }
    }

    Write-Host "  ${name}: $($warnings.Count) warnings"
    return $warnings
}

if (-not $D2Only) {
    $allWarnings += "# === D1 warnings ==="
    $allWarnings += ""
    $d1Warnings = Build-And-Gather "d1" "buildd1"
    $allWarnings += $d1Warnings
    $allWarnings += ""
}

if (-not $D1Only) {
    $allWarnings += "# === D2 warnings ==="
    $allWarnings += ""
    $d2Warnings = Build-And-Gather "d2" "buildd2"
    $allWarnings += $d2Warnings
    $allWarnings += ""
}

($header + $allWarnings) | Set-Content $logFile -Encoding UTF8

Write-Host ""
Write-Host "Written to: $logFile"
