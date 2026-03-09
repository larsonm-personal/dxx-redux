# Build and run CUE parser + ISO reader tests (Windows).
#
# Usage:  .\run_cue_iso_tests.ps1
#
# Requires cl.exe (Visual Studio) on PATH, or uses gcc if available.

$ErrorActionPreference = "Stop"

Push-Location "$PSScriptRoot\app\src\main\cpp"
try {
    $src = @("test_cue_iso.c", "cue_parser.c", "iso9660_reader.c")
    $exe = "test_cue_iso.exe"

    # Try gcc first (e.g., MSYS2 / MinGW), fall back to cl.exe
    $gcc = Get-Command gcc -ErrorAction SilentlyContinue
    if ($gcc) {
        Write-Host "Building with gcc..."
        & gcc -DTEST_STANDALONE -I. -Wall -Wextra -o $exe @src
    } else {
        # Find cl.exe via vswhere
        $vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
        if (Test-Path $vsWhere) {
            $installPath = & $vsWhere -latest -property installationPath
            $vcvars = Join-Path $installPath "VC\Auxiliary\Build\vcvars64.bat"
            if (Test-Path $vcvars) {
                Write-Host "Building with cl.exe..."
                cmd /c "`"$vcvars`" >nul 2>&1 && cl /DTEST_STANDALONE /I. /Fe:$exe $($src -join ' ') /link"
            } else {
                throw "Cannot find vcvars64.bat"
            }
        } else {
            throw "No compiler found. Install gcc (MinGW/MSYS2) or Visual Studio."
        }
    }

    if (-not (Test-Path $exe)) {
        throw "Build failed: $exe not found"
    }

    Write-Host "`nRunning tests..."
    & ".\$exe"
    $rc = $LASTEXITCODE

    # Clean up
    Remove-Item -Force $exe -ErrorAction SilentlyContinue
    Remove-Item -Force "*.obj" -ErrorAction SilentlyContinue
    Remove-Item -Recurse -Force "test_fixtures" -ErrorAction SilentlyContinue

    if ($rc -ne 0) {
        Write-Host "`nSome tests FAILED" -ForegroundColor Red
        exit $rc
    }
    Write-Host "`nAll tests passed" -ForegroundColor Green
} finally {
    Pop-Location
}
