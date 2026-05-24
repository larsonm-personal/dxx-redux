#!/usr/bin/env pwsh
# Build and run CUE parser + ISO reader tests (Windows).
#
# Usage:  .\test_cue_iso.ps1
#
# Uses CMake to build test executables in android/tests/build/.

$ErrorActionPreference = "Stop"

# Source shared env setup (adds cmake to PATH if needed)
. "$PSScriptRoot\..\test_env.ps1"

$srcDir = "$PSScriptRoot\..\app\src\main\cpp\extract"
$buildDir = "$PSScriptRoot\build"

# Configure + build
if (-not (Test-Path "$buildDir\CMakeCache.txt")) {
    Write-Host "Configuring cmake..."
    cmake -S $srcDir -B $buildDir
}
Write-Host "Building..."
cmake --build $buildDir --config Release
if ($LASTEXITCODE -ne 0) { throw "Build failed" }

# Run tests via CTest
Write-Host "`nRunning tests..."
Push-Location $buildDir
try {
    ctest -C Release --output-on-failure
    $rc = $LASTEXITCODE
} finally {
    Pop-Location
}

# Clean up test fixtures left by test_cue_iso
Remove-Item -Recurse -Force "$srcDir\test_fixtures" -ErrorAction SilentlyContinue

if ($rc -ne 0) {
    Write-Host "`nSome tests FAILED" -ForegroundColor Red
    exit $rc
}
Write-Host "`nAll tests passed" -ForegroundColor Green
