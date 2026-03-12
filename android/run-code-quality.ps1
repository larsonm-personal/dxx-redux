# run-code-quality.ps1 -- Run all code quality checks (clang-format + ktlint).
# Usage:
#   .\run-code-quality.ps1          # check only (exit 1 if issues)
#   .\run-code-quality.ps1 --fix    # auto-format both C/C++ and Kotlin

param(
    [switch]$Fix
)

$ErrorActionPreference = "Continue"
$scriptDir = $PSScriptRoot
$failed = @()

Write-Host "=== Code Quality Checks ==="
Write-Host ""

# --- clang-format ---
Write-Host "--- C/C++ (clang-format) ---"
if ($Fix) {
    & "$scriptDir\run-clang-format.ps1"
} else {
    & "$scriptDir\run-clang-format.ps1" -Check
}
if ($LASTEXITCODE -ne 0) {
    $failed += "clang-format"
}
Write-Host ""

# --- ktlint ---
Write-Host "--- Kotlin (ktlint) ---"
if ($Fix) {
    & "$scriptDir\run-ktlint.ps1" -Format
} else {
    & "$scriptDir\run-ktlint.ps1"
}
if ($LASTEXITCODE -ne 0) {
    $failed += "ktlint"
}
Write-Host ""

# --- Summary ---
Write-Host "=== Summary ==="
if ($failed.Count -eq 0) {
    Write-Host "All checks passed."
} else {
    Write-Host "Failed checks: $($failed -join ', ')"
    if (-not $Fix) {
        Write-Host "Run with --fix to auto-format."
    }
    exit 1
}
